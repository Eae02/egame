#include "OpenGL.hpp"
#include "Utils.hpp"
#include "OpenGLTexture.hpp"
#include "PipelineGraphics.hpp"
#include "../../Alloc/ObjectPool.hpp"

namespace eg::graphics_api::gl
{
	bool defaultFramebufferHasDepth;
	bool defaultFramebufferHasStencil;
	
	int drawableWidth;
	int drawableHeight;
	
	bool srgbBackBuffer;
	bool hasWrittenToBackBuffer;
	
	struct ResolveFBO
	{
		GLenum mask;
		GLuint framebuffers[2];
	};
	
	struct Framebuffer
	{
		GLuint framebuffer;
		uint32_t numColorAttachments;
		bool hasSRGB;
		bool hasDepth;
		bool hasStencil;
		bool multisampled;
		uint32_t width;
		uint32_t height;
		std::vector<ResolveFBO> resolveFBOs;
	};
	
	static ObjectPool<Framebuffer> framebuffers;
	
	inline Framebuffer* UnwrapFramebuffer(FramebufferHandle handle)
	{
		return reinterpret_cast<Framebuffer*>(handle);
	}
	
	void AttachTexture(GLenum target, GLuint framebuffer, const FramebufferAttachment& attachment)
	{
		Texture* texture = UnwrapTexture(attachment.texture);
		
		glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
		
		if ((texture->type == GL_TEXTURE_2D_ARRAY || texture->type == GL_TEXTURE_CUBE_MAP_ARRAY ||
		     texture->type == GL_TEXTURE_CUBE_MAP) && attachment.subresource.numArrayLayers == 1)
		{
			glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, target, texture->texture,
				attachment.subresource.mipLevel, attachment.subresource.firstArrayLayer);
		}
		else
		{
#ifdef EG_GLES
			glFramebufferTexture2D(GL_READ_FRAMEBUFFER, target, GL_TEXTURE_2D,
				texture->GetView(attachment.subresource.AsSubresource()), 0);
#else
			glFramebufferTexture(GL_READ_FRAMEBUFFER, target,
				texture->GetView(attachment.subresource.AsSubresource()), 0);
#endif
		}
	}
	
	FramebufferHandle CreateFramebuffer(const FramebufferCreateInfo& createInfo)
	{
		Framebuffer* framebuffer = framebuffers.New();
		glGenFramebuffers(1, &framebuffer->framebuffer);
		
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->framebuffer);
		
		if (createInfo.label != nullptr)
		{
			glObjectLabel(GL_FRAMEBUFFER, framebuffer->framebuffer, -1, createInfo.label);
		}
		
		framebuffer->numColorAttachments = (uint32_t)createInfo.colorAttachments.size();
		framebuffer->hasDepth = false;
		framebuffer->hasStencil = false;
		framebuffer->hasSRGB = false;
		framebuffer->multisampled = true;
		
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
			
			if (texture->sampleCount > 1)
				framebuffer->multisampled = true;
			
			SetSize(texture->width, texture->height);
			if (IsSRGBFormat(texture->format))
				framebuffer->hasSRGB = true;
			
			AttachTexture(target, framebuffer->framebuffer, attachment);
		};
		
		for (uint32_t i = 0; i < createInfo.colorAttachments.size(); i++)
		{
			Attach(GL_COLOR_ATTACHMENT0 + i, createInfo.colorAttachments[i]);
			drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
		}
		
		if (createInfo.depthStencilAttachment.texture != nullptr)
		{
			switch (UnwrapTexture(createInfo.depthStencilAttachment.texture)->format)
			{
			case Format::Depth32:
			case Format::Depth16:
				Attach(GL_DEPTH_ATTACHMENT, createInfo.depthStencilAttachment);
				break;
			case Format::Depth24Stencil8:
			case Format::Depth32Stencil8:
				Attach(GL_DEPTH_STENCIL_ATTACHMENT, createInfo.depthStencilAttachment);
				break;
			default:
				EG_PANIC("Invalid depth stencil attachment format");
			}
			framebuffer->hasDepth = true;
		}
		
		if (!createInfo.colorAttachments.Empty())
		{
			glDrawBuffers((GLsizei)createInfo.colorAttachments.size(),
				drawBuffers);
		}
		
		for (size_t i = 0; i < createInfo.colorResolveAttachments.size(); i++)
		{
			if (createInfo.colorResolveAttachments[i].texture == nullptr)
				continue;
			
			auto& fboPair = framebuffer->resolveFBOs.emplace_back();
			fboPair.mask = GL_COLOR_BUFFER_BIT;
			glGenFramebuffers(2, fboPair.framebuffers);
			AttachTexture(GL_COLOR_ATTACHMENT0, fboPair.framebuffers[0], createInfo.colorAttachments[i]);
			AttachTexture(GL_COLOR_ATTACHMENT0, fboPair.framebuffers[1], createInfo.colorResolveAttachments[i]);
		}
		
		if (createInfo.depthStencilResolveAttachment.texture != nullptr)
		{
			auto& fboPair = framebuffer->resolveFBOs.emplace_back();
			fboPair.mask = GL_DEPTH_BUFFER_BIT;
			glGenFramebuffers(2, fboPair.framebuffers);
			AttachTexture(GL_DEPTH_ATTACHMENT, fboPair.framebuffers[0], createInfo.depthStencilAttachment);
			AttachTexture(GL_DEPTH_ATTACHMENT, fboPair.framebuffers[1], createInfo.depthStencilResolveAttachment);
		}
		
		return reinterpret_cast<FramebufferHandle>(framebuffer);
	}
	
	void DestroyFramebuffer(FramebufferHandle handle)
	{
		Framebuffer* framebuffer = UnwrapFramebuffer(handle);
		glDeleteFramebuffers(1, &framebuffer->framebuffer);
		for (const ResolveFBO& resolveFbo : framebuffer->resolveFBOs)
			glDeleteFramebuffers(2, resolveFbo.framebuffers);
		framebuffers.Free(framebuffer);
	}
	
	Framebuffer* currentFramebuffer = nullptr;
	
	void BeginRenderPass(CommandContextHandle cc, const RenderPassBeginInfo& beginInfo)
	{
		uint32_t numColorAttachments;
		bool hasDepth;
		bool hasStencil;
		bool forceClear;
		if (beginInfo.framebuffer == nullptr)
		{
			currentFramebuffer = nullptr;
			
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			numColorAttachments = 1;
			hasDepth = defaultFramebufferHasDepth;
			hasStencil = defaultFramebufferHasStencil;
			forceClear = !hasWrittenToBackBuffer;
			
#ifndef EG_GLES
			SetEnabled<GL_FRAMEBUFFER_SRGB>(srgbBackBuffer);
			SetEnabled<GL_MULTISAMPLE>(false);
#endif
			
			SetViewport(cc, 0, 0, drawableWidth, drawableHeight);
			SetScissor(cc, 0, 0, drawableWidth, drawableHeight);
		}
		else
		{
			Framebuffer* framebuffer = UnwrapFramebuffer(beginInfo.framebuffer);
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->framebuffer);
			
			currentFramebuffer = framebuffer;
			
			numColorAttachments = framebuffer->numColorAttachments;
			hasDepth = framebuffer->hasDepth;
			hasStencil = framebuffer->hasStencil;
			forceClear = false;
			
#ifndef EG_GLES
			SetEnabled<GL_FRAMEBUFFER_SRGB>(true);
			SetEnabled<GL_MULTISAMPLE>(framebuffer->multisampled);
#endif
			
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
						invalidateAttachments[numInvalidateAttachments++] = GL_COLOR;
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
					invalidateAttachments[numInvalidateAttachments++] = GL_COLOR;
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
	
	void EndRenderPass(CommandContextHandle)
	{
		if (currentFramebuffer != nullptr)
		{
			for (const auto& resolveFBO : currentFramebuffer->resolveFBOs)
			{
				glBindFramebuffer(GL_READ_FRAMEBUFFER, resolveFBO.framebuffers[0]);
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFBO.framebuffers[1]);
				glBlitFramebuffer(0, 0, currentFramebuffer->width, currentFramebuffer->height, 0, 0,
					currentFramebuffer->width, currentFramebuffer->height, resolveFBO.mask, GL_NEAREST);
			}
		}
	}
}
