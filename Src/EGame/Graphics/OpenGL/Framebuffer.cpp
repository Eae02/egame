#include "OpenGL.hpp"
#include "Utils.hpp"
#include "OpenGLTexture.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "PipelineGraphics.hpp"

namespace eg::graphics_api::gl
{
	bool defaultFramebufferHasDepth;
	bool defaultFramebufferHasStencil;
	
	int drawableWidth;
	int drawableHeight;
	
	bool srgbBackBuffer;
	bool hasWrittenToBackBuffer;
	
	struct Framebuffer
	{
		GLuint framebuffer;
		uint32_t numColorAttachments;
		bool hasSRGB;
		bool hasDepth;
		bool hasStencil;
		uint32_t width;
		uint32_t height;
	};
	
	static ObjectPool<Framebuffer> framebuffers;
	
	inline Framebuffer* UnwrapFramebuffer(FramebufferHandle handle)
	{
		return reinterpret_cast<Framebuffer*>(handle);
	}
	
	FramebufferHandle CreateFramebuffer(Span<const FramebufferAttachment> colorAttachments,
		const FramebufferAttachment* dsAttachment)
	{
		Framebuffer* framebuffer = framebuffers.New();
		glCreateFramebuffers(1, &framebuffer->framebuffer);
		
		framebuffer->numColorAttachments = (uint32_t)colorAttachments.size();
		framebuffer->hasDepth = false;
		framebuffer->hasStencil = false;
		framebuffer->hasSRGB = false;
		
		bool hasSetSize = false;
		auto SetSize = [&] (uint32_t w, uint32_t h)
		{
			if (!hasSetSize)
			{
				framebuffer->width = w;
				framebuffer->height = h;
			}
			else if (framebuffer->width != w || framebuffer->height != h)
			{
				EG_PANIC("Inconsistent framebuffer attachment resolution");
			}
		};
		
		GLenum drawBuffers[MAX_COLOR_ATTACHMENTS];
		
		auto Attach = [&] (GLenum target, const FramebufferAttachment& attachment)
		{
			Texture* texture = UnwrapTexture(attachment.texture);
			
			SetSize(texture->width, texture->height);
			if (IsSRGBFormat(texture->format))
				framebuffer->hasSRGB = true;
			
			if ((texture->type == GL_TEXTURE_2D_ARRAY || texture->type == GL_TEXTURE_CUBE_MAP_ARRAY ||
			     texture->type == GL_TEXTURE_CUBE_MAP) && attachment.subresource.numArrayLayers == 1)
			{
				glNamedFramebufferTextureLayer(framebuffer->framebuffer, target, texture->texture,
					attachment.subresource.mipLevel, attachment.subresource.firstArrayLayer);
			}
			else
			{
				glNamedFramebufferTexture(framebuffer->framebuffer, target,
					texture->GetView(attachment.subresource.AsSubresource()), 0);
			}
		};
		
		for (uint32_t i = 0; i < colorAttachments.size(); i++)
		{
			Attach(GL_COLOR_ATTACHMENT0 + i, colorAttachments[i]);
			drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
		}
		
		if (dsAttachment != nullptr)
		{
			switch (UnwrapTexture(dsAttachment->texture)->format)
			{
			case Format::Depth32:
			case Format::Depth16:
				Attach(GL_DEPTH_ATTACHMENT, *dsAttachment);
				break;
			case Format::Depth24Stencil8:
			case Format::Depth32Stencil8:
				Attach(GL_DEPTH_STENCIL_ATTACHMENT, *dsAttachment);
				break;
			default:
				EG_PANIC("Invalid depth stencil attachment format");
			}
			framebuffer->hasDepth = true;
		}
		
		if (!colorAttachments.Empty())
		{
			glNamedFramebufferDrawBuffers(framebuffer->framebuffer, (GLsizei)colorAttachments.size(), drawBuffers);
		}
		
		return reinterpret_cast<FramebufferHandle>(framebuffer);
	}
	
	void DestroyFramebuffer(FramebufferHandle handle)
	{
		framebuffers.Free(UnwrapFramebuffer(handle));
	}
	
	void BeginRenderPass(CommandContextHandle cc, const RenderPassBeginInfo& beginInfo)
	{
		uint32_t numColorAttachments;
		bool hasDepth;
		bool hasStencil;
		bool forceClear;
		if (beginInfo.framebuffer == nullptr)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			numColorAttachments = 1;
			hasDepth = defaultFramebufferHasDepth;
			hasStencil = defaultFramebufferHasStencil;
			forceClear = !hasWrittenToBackBuffer;
			
			SetEnabled<GL_FRAMEBUFFER_SRGB>(srgbBackBuffer);
			
			SetViewport(cc, 0, 0, drawableWidth, drawableHeight);
			SetScissor(cc, 0, 0, drawableWidth, drawableHeight);
		}
		else
		{
			Framebuffer* framebuffer = UnwrapFramebuffer(beginInfo.framebuffer);
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->framebuffer);
			
			numColorAttachments = framebuffer->numColorAttachments;
			hasDepth = framebuffer->hasDepth;
			hasStencil = framebuffer->hasStencil;
			forceClear = false;
			
			SetEnabled<GL_FRAMEBUFFER_SRGB>(true);
			
			SetViewport(cc, 0, 0, framebuffer->width, framebuffer->height);
			SetScissor(cc, 0, 0, framebuffer->width, framebuffer->height);
		}
		
		SetEnabled<GL_SCISSOR_TEST>(false);
		if (!IsDepthWriteEnabled())
			glDepthMask(GL_TRUE);
		
		uint32_t numInvalidateAttachments = 0;
		GLenum invalidateAttachments[MAX_COLOR_ATTACHMENTS + 2];
		
		if (hasDepth)
		{
			if (beginInfo.depthLoadOp == beginInfo.stencilLoadOp && hasStencil)
			{
				if (beginInfo.depthLoadOp == AttachmentLoadOp::Clear || forceClear)
				{
					glClearBufferfi(GL_DEPTH_STENCIL, 0, beginInfo.depthClearValue, beginInfo.stencilClearValue);
				}
				else if (beginInfo.depthLoadOp == AttachmentLoadOp::Discard)
				{
					if (beginInfo.framebuffer == nullptr)
					{
						invalidateAttachments[numInvalidateAttachments++] = GL_DEPTH;
						invalidateAttachments[numInvalidateAttachments++] = GL_STENCIL;
					}
					else
					{
						invalidateAttachments[numInvalidateAttachments++] = GL_DEPTH_STENCIL_ATTACHMENT;
					}
				}
			}
			else
			{
				if (beginInfo.depthLoadOp == AttachmentLoadOp::Clear || forceClear)
				{
					glClearBufferfv(GL_DEPTH, 0, &beginInfo.depthClearValue);
				}
				else if (beginInfo.depthLoadOp == AttachmentLoadOp::Discard)
				{
					if (beginInfo.framebuffer == nullptr)
					{
						invalidateAttachments[numInvalidateAttachments++] = GL_DEPTH;
					}
					else
					{
						invalidateAttachments[numInvalidateAttachments++] = GL_DEPTH_ATTACHMENT;
					}
				}
				
				if (hasStencil)
				{
					if (beginInfo.stencilLoadOp == AttachmentLoadOp::Clear || forceClear)
					{
						GLuint value = beginInfo.stencilClearValue;
						glClearBufferuiv(GL_STENCIL, 0, &value);
					}
					else if (beginInfo.stencilLoadOp == AttachmentLoadOp::Discard)
					{
						if (beginInfo.framebuffer == nullptr)
						{
							invalidateAttachments[numInvalidateAttachments++] = GL_STENCIL;
						}
						else
						{
							invalidateAttachments[numInvalidateAttachments++] = GL_STENCIL_ATTACHMENT;
						}
					}
				}
			}
		}
		
		for (uint32_t i = 0; i < numColorAttachments; i++)
		{
			const RenderPassColorAttachment& attachment = beginInfo.colorAttachments[i];
			if (attachment.loadOp == AttachmentLoadOp::Clear || forceClear)
			{
				glClearBufferfv(GL_COLOR, i, &attachment.clearValue.r);
			}
			else if (attachment.loadOp == AttachmentLoadOp::Discard)
			{
				if (beginInfo.framebuffer == nullptr)
				{
					invalidateAttachments[numInvalidateAttachments++] = GL_BACK_LEFT;
				}
				else
				{
					invalidateAttachments[numInvalidateAttachments++] = GL_COLOR_ATTACHMENT0 + i;
				}
			}
		}
		
		if (numInvalidateAttachments != 0)
		{
			glInvalidateFramebuffer(GL_FRAMEBUFFER, numInvalidateAttachments, invalidateAttachments);
		}
		
		InitScissorTest();
		if (!IsDepthWriteEnabled())
			glDepthMask(GL_FALSE);
		
		hasWrittenToBackBuffer = true;
	}
	
	void EndRenderPass(CommandContextHandle) { }
}
