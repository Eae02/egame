#include "../../Alloc/ObjectPool.hpp"
#include "WGPU.hpp"
#include "WGPUCommandContext.hpp"
#include "WGPUTexture.hpp"

namespace eg::graphics_api::webgpu
{
struct Framebuffer
{
	uint32_t numColorAttachments = 0;
	uint32_t width = 0;
	uint32_t height = 0;

	std::array<WGPUTextureView, MAX_COLOR_ATTACHMENTS> colorAttachments;
	WGPUTextureView depthStencilAttachment = nullptr;
	bool hasStencil = false;
};

static ConcurrentObjectPool<Framebuffer> framebufferObjectPool;

FramebufferHandle CreateFramebuffer(const FramebufferCreateInfo& createInfo)
{
	EG_ASSERT(createInfo.colorAttachments.size() <= MAX_COLOR_ATTACHMENTS);

	Framebuffer* framebuffer = framebufferObjectPool.New();

	auto ProcessAttachment = [&](const FramebufferAttachment& attachment) -> WGPUTextureView
	{
		Texture& texture = Texture::Unwrap(attachment.texture);

		const uint32_t textureWidth = wgpuTextureGetWidth(texture.texture) >> attachment.subresource.mipLevel;
		const uint32_t textureHeight = wgpuTextureGetHeight(texture.texture) >> attachment.subresource.mipLevel;

		if (framebuffer->width == 0 && framebuffer->height == 0)
		{
			framebuffer->width = textureWidth;
			framebuffer->height = textureHeight;
		}
		else
		{
			EG_ASSERT(framebuffer->width == textureWidth && framebuffer->height == textureHeight);
		}

		return texture.GetTextureView(TextureViewType::Flat2D, attachment.subresource.AsSubresource());
	};

	framebuffer->numColorAttachments = static_cast<uint32_t>(createInfo.colorAttachments.size());
	for (uint32_t i = 0; i < framebuffer->numColorAttachments; i++)
	{
		framebuffer->colorAttachments[i] = ProcessAttachment(createInfo.colorAttachments[i]);
	}

	if (createInfo.depthStencilAttachment.texture != nullptr)
	{
		framebuffer->depthStencilAttachment = ProcessAttachment(createInfo.depthStencilAttachment);

		Format dsFormat = Texture::Unwrap(createInfo.depthStencilAttachment.texture).format;
		framebuffer->hasStencil = dsFormat == Format::Depth24Stencil8 || dsFormat == Format::Depth32Stencil8;
	}

	// TODO: Handle resolve attachments

	return reinterpret_cast<FramebufferHandle>(framebuffer);
}

void DestroyFramebuffer(FramebufferHandle framebuffer)
{
	framebufferObjectPool.Delete(reinterpret_cast<Framebuffer*>(framebuffer));
}

static inline WGPULoadOp TranslateLoadOp(AttachmentLoadOp loadOp)
{
	if (loadOp == AttachmentLoadOp::Load)
		return WGPULoadOp_Load;
	else
		return WGPULoadOp_Clear;
}

static inline WGPUStoreOp TranslateStoreOp(AttachmentStoreOp storeOp)
{
	if (storeOp == AttachmentStoreOp::Store)
		return WGPUStoreOp_Store;
	else
		return WGPUStoreOp_Discard;
}

void BeginRenderPass(CommandContextHandle cc, const RenderPassBeginInfo& beginInfo)
{
	WGPURenderPassColorAttachment colorAttachments[MAX_COLOR_ATTACHMENTS] = {};

	WGPURenderPassDepthStencilAttachment depthStencilAttachment = {};
	WGPURenderPassDescriptor renderPassDesc = { .colorAttachments = colorAttachments };

	uint32_t framebufferWidth;
	uint32_t framebufferHeight;

	if (beginInfo.framebuffer == nullptr)
	{
		renderPassDesc.colorAttachmentCount = 1;
		if (wgpuctx.srgbEmulationColorTextureView != nullptr)
			colorAttachments[0].view = wgpuctx.srgbEmulationColorTextureView;
		else
			colorAttachments[0].view = wgpuctx.currentSwapchainColorView;

		framebufferWidth = wgpuctx.swapchainImageWidth;
		framebufferHeight = wgpuctx.swapchainImageHeight;
	}
	else
	{
		const Framebuffer& framebuffer = *reinterpret_cast<Framebuffer*>(beginInfo.framebuffer);
		renderPassDesc.colorAttachmentCount = framebuffer.numColorAttachments;

		for (uint32_t i = 0; i < framebuffer.numColorAttachments; i++)
			colorAttachments[i].view = framebuffer.colorAttachments[i];

		if (framebuffer.depthStencilAttachment != nullptr)
		{
			depthStencilAttachment.view = framebuffer.depthStencilAttachment;
			renderPassDesc.depthStencilAttachment = &depthStencilAttachment;
		}

		depthStencilAttachment.depthReadOnly = beginInfo.depthStencilReadOnly;
		depthStencilAttachment.stencilReadOnly = beginInfo.depthStencilReadOnly;

		if (!beginInfo.depthStencilReadOnly)
		{
			depthStencilAttachment.depthClearValue = beginInfo.depthClearValue;
			depthStencilAttachment.stencilClearValue = beginInfo.stencilClearValue;

			depthStencilAttachment.depthLoadOp = TranslateLoadOp(beginInfo.depthLoadOp);
			depthStencilAttachment.depthStoreOp = TranslateStoreOp(beginInfo.depthStoreOp);

			if (framebuffer.hasStencil)
			{
				depthStencilAttachment.stencilLoadOp = TranslateLoadOp(beginInfo.stencilLoadOp);
				depthStencilAttachment.stencilStoreOp = TranslateStoreOp(beginInfo.stencilStoreOp);
			}
		}

		framebufferWidth = framebuffer.width;
		framebufferHeight = framebuffer.height;
	}

	for (size_t i = 0; i < renderPassDesc.colorAttachmentCount; i++)
	{
		colorAttachments[i].loadOp = TranslateLoadOp(beginInfo.colorAttachments[i].loadOp);
		colorAttachments[i].storeOp = TranslateStoreOp(beginInfo.colorAttachments[i].storeOp);
		colorAttachments[i].depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

		if (beginInfo.colorAttachments[i].loadOp == AttachmentLoadOp::Clear)
		{
			std::visit(
				[&](const auto& clearValue)
				{
					colorAttachments[i].clearValue = WGPUColor{
						.r = static_cast<double>(clearValue.r),
						.g = static_cast<double>(clearValue.g),
						.b = static_cast<double>(clearValue.b),
						.a = static_cast<double>(clearValue.a),
					};
				},
				beginInfo.colorAttachments[i].clearValue);
		}
	}

	CommandContext::Unwrap(cc).BeginRenderPass(renderPassDesc, framebufferWidth, framebufferHeight);
}

void EndRenderPass(CommandContextHandle cc)
{
	CommandContext& wcc = CommandContext::Unwrap(cc);
	EG_ASSERT(wcc.renderPassEncoder != nullptr);
	wgpuRenderPassEncoderEnd(wcc.renderPassEncoder);
	wgpuRenderPassEncoderRelease(wcc.renderPassEncoder);
	wcc.renderPassEncoder = nullptr;
}
} // namespace eg::graphics_api::webgpu
