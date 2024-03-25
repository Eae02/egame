#include "../../Alloc/ObjectPool.hpp"
#include "../../Assert.hpp"
#include "Common.hpp"
#include "RenderPasses.hpp"
#include "Texture.hpp"
#include "VulkanCommandContext.hpp"
#include "VulkanMain.hpp"

namespace eg::graphics_api::vk
{
struct Framebuffer : Resource
{
	VkFramebuffer framebuffer;
	uint32_t numColorAttachments;
	uint32_t sampleCount;
	VkExtent2D extent;
	Texture* colorAttachments[MAX_COLOR_ATTACHMENTS];
	Texture* resolveColorAttachments[MAX_COLOR_ATTACHMENTS];
	Texture* depthStencilAttachment;
	Texture* resolveDepthStencilAttachment;

	void Free() override;
};

ObjectPool<Framebuffer> framebufferPool;

void Framebuffer::Free()
{
	for (uint32_t i = 0; i < numColorAttachments; i++)
		colorAttachments[i]->UnRef();
	if (depthStencilAttachment != nullptr)
		depthStencilAttachment->UnRef();
	vkDestroyFramebuffer(ctx.device, framebuffer, nullptr);
	framebufferPool.Delete(this);
}

inline Framebuffer* UnwrapFramebuffer(FramebufferHandle handle)
{
	return reinterpret_cast<Framebuffer*>(handle);
}

FramebufferHandle CreateFramebuffer(const FramebufferCreateInfo& createInfo)
{
	Framebuffer* framebuffer = framebufferPool.New();
	framebuffer->refCount = 1;

	VkImageView attachments[MAX_COLOR_ATTACHMENTS + 1];

	RenderPassDescription rpDescription;

	VkFramebufferCreateInfo vkCreateInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	vkCreateInfo.pAttachments = attachments;
	vkCreateInfo.layers = 1;

	uint32_t sampleCount = 1;

	bool sizeSet = false;

	auto ProcessAttachment = [&](const FramebufferAttachment& attachment, VkFormat& formatOut)
	{
		Texture* texture = UnwrapTexture(attachment.texture);
		const uint32_t layers = attachment.subresource.ResolveRem(texture->numArrayLayers).numArrayLayers;

		if (!sizeSet)
		{
			sizeSet = true;
			vkCreateInfo.width = texture->extent.width >> attachment.subresource.mipLevel;
			vkCreateInfo.height = texture->extent.height >> attachment.subresource.mipLevel;
			vkCreateInfo.layers = layers;

			// sampleCount should not be set for resolve attachments, but won't since these are processed last.
			sampleCount = texture->sampleCount;
		}
		else if (
			vkCreateInfo.width != texture->extent.width || vkCreateInfo.height != texture->extent.height ||
			vkCreateInfo.layers != layers)
		{
			EG_PANIC("Inconsistent framebuffer attachment resolution");
		}

		TextureView& view = texture->GetView(attachment.subresource.AsSubresource(), 0, VK_IMAGE_VIEW_TYPE_2D);
		attachments[vkCreateInfo.attachmentCount++] = view.view;

		texture->refCount++;
		formatOut = texture->format;

		return texture;
	};

	if (createInfo.depthStencilAttachment.texture != nullptr)
	{
		framebuffer->depthStencilAttachment =
			ProcessAttachment(createInfo.depthStencilAttachment, rpDescription.depthAttachment.format);
		rpDescription.depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}
	else
	{
		framebuffer->depthStencilAttachment = nullptr;
	}

	// Processes color attachments
	framebuffer->numColorAttachments = UnsignedNarrow<uint32_t>(createInfo.colorAttachments.size());
	rpDescription.numColorAttachments = UnsignedNarrow<uint32_t>(createInfo.colorAttachments.size());
	for (size_t i = 0; i < createInfo.colorAttachments.size(); i++)
	{
		framebuffer->colorAttachments[i] =
			ProcessAttachment(createInfo.colorAttachments[i], rpDescription.colorAttachments[i].format);
		rpDescription.colorAttachments[i].samples = sampleCount;
		rpDescription.colorAttachments[i].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	// Processes color resolve attachments
	rpDescription.numResolveColorAttachments = UnsignedNarrow<uint32_t>(createInfo.colorResolveAttachments.size());
	std::fill_n(framebuffer->resolveColorAttachments, MAX_COLOR_ATTACHMENTS, nullptr);
	for (size_t i = 0; i < createInfo.colorResolveAttachments.size(); i++)
	{
		if (createInfo.colorResolveAttachments[i].texture != nullptr)
		{
			framebuffer->resolveColorAttachments[i] = ProcessAttachment(
				createInfo.colorResolveAttachments[i], rpDescription.resolveColorAttachments[i].format);
			rpDescription.resolveColorAttachments[i].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
	}

	// Processes the depth resolve attachment
	if (createInfo.depthStencilResolveAttachment.texture != nullptr)
	{
		framebuffer->resolveDepthStencilAttachment =
			ProcessAttachment(createInfo.depthStencilResolveAttachment, rpDescription.resolveDepthAttachment.format);
		rpDescription.resolveDepthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}
	else
	{
		framebuffer->resolveDepthStencilAttachment = nullptr;
	}

	rpDescription.depthAttachment.samples = sampleCount;

	vkCreateInfo.renderPass = GetRenderPass(rpDescription, true);

	framebuffer->extent.width = vkCreateInfo.width;
	framebuffer->extent.height = vkCreateInfo.height;
	framebuffer->sampleCount = sampleCount;

	CheckRes(vkCreateFramebuffer(ctx.device, &vkCreateInfo, nullptr, &framebuffer->framebuffer));

	if (createInfo.label != nullptr)
	{
		SetObjectName(
			reinterpret_cast<uint64_t>(framebuffer->framebuffer), VK_OBJECT_TYPE_FRAMEBUFFER, createInfo.label);
	}

	return reinterpret_cast<FramebufferHandle>(framebuffer);
}

void DestroyFramebuffer(FramebufferHandle handle)
{
	UnwrapFramebuffer(handle)->UnRef();
}

inline VkAttachmentLoadOp TranslateLoadOp(AttachmentLoadOp loadOp)
{
	switch (loadOp)
	{
	case AttachmentLoadOp::Clear: return VK_ATTACHMENT_LOAD_OP_CLEAR;
	case AttachmentLoadOp::Discard: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	case AttachmentLoadOp::Load: return VK_ATTACHMENT_LOAD_OP_LOAD;
	}
	EG_UNREACHABLE
}

inline VkAttachmentStoreOp TranslateStoreOp(AttachmentStoreOp storeOp)
{
	switch (storeOp)
	{
	case AttachmentStoreOp::Store: return VK_ATTACHMENT_STORE_OP_STORE;
	case AttachmentStoreOp::Discard: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
	}
	EG_UNREACHABLE
}

void BeginRenderPass(CommandContextHandle cc, const RenderPassBeginInfo& beginInfo)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);

	uint32_t numColorAttachments = 1;
	uint32_t sampleCount = 1;
	VkImageLayout colorImageInitialLayouts[MAX_COLOR_ATTACHMENTS];
	VkImageLayout colorImageFinalLayouts[MAX_COLOR_ATTACHMENTS];
	VkImageLayout depthStencilImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	VkFramebuffer framebuffer;
	VkExtent2D extent;

	VkFormat colorFormats[MAX_COLOR_ATTACHMENTS] = {};
	VkFormat depthStencilFormat = VK_FORMAT_UNDEFINED;

	VkFormat resolveAttachmentFormats[MAX_COLOR_ATTACHMENTS] = {};
	VkFormat depthStencilResolveAttachmentFormat = VK_FORMAT_UNDEFINED;

	bool changeLoadToClear = false;

	if (beginInfo.framebuffer == nullptr)
	{
		MaybeAcquireSwapchainImage();

		extent = ctx.swapchain.m_surfaceExtent;
		framebuffer = ctx.defaultFramebuffers[ctx.swapchain.m_currentImage];
		colorFormats[0] = ctx.swapchain.m_surfaceFormat.format;
		depthStencilFormat = ctx.defaultDSFormat;
		colorImageInitialLayouts[0] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorImageFinalLayouts[0] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		changeLoadToClear = ctx.defaultFramebufferInPresentMode;
		ctx.defaultFramebufferInPresentMode = false;
	}
	else
	{
		Framebuffer* framebufferS = UnwrapFramebuffer(beginInfo.framebuffer);
		framebuffer = framebufferS->framebuffer;
		extent = framebufferS->extent;

		vcc.referencedResources.Add(*framebufferS);

		sampleCount = framebufferS->sampleCount;

		// Fetches the initial layouts and formats of color attachments and updates
		//  auto barrier usage state to reflect the changes made in the render pass.
		numColorAttachments = framebufferS->numColorAttachments;
		for (uint32_t i = 0; i < numColorAttachments; i++)
		{
			colorFormats[i] = framebufferS->colorAttachments[i]->format;

			colorImageFinalLayouts[i] =
				ImageLayoutFromUsage(beginInfo.colorAttachments[i].finalUsage, VK_IMAGE_ASPECT_COLOR_BIT);

			if (framebufferS->colorAttachments[i]->autoBarrier)
			{
				colorImageInitialLayouts[i] = framebufferS->colorAttachments[i]->CurrentLayout();
				framebufferS->colorAttachments[i]->currentUsage = beginInfo.colorAttachments[i].finalUsage;
				framebufferS->colorAttachments[i]->currentStageFlags = GetBarrierStageFlagsFromUsage(
					beginInfo.colorAttachments[i].finalUsage, ShaderAccessFlags::Fragment);
			}
			else if (beginInfo.colorAttachments[i].loadOp == eg::AttachmentLoadOp::Load)
			{
				colorImageInitialLayouts[i] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			}
			else
			{
				colorImageInitialLayouts[i] = VK_IMAGE_LAYOUT_UNDEFINED;
			}
		}

		// Fetches formats of resolve color attachments and updates
		//  auto barrier usage state to reflect the changes made in the render pass.
		for (uint32_t i = 0; i < MAX_COLOR_ATTACHMENTS; i++)
		{
			if (framebufferS->resolveColorAttachments[i] != nullptr)
			{
				resolveAttachmentFormats[i] = framebufferS->resolveColorAttachments[i]->format;

				if (framebufferS->resolveColorAttachments[i]->autoBarrier)
				{
					framebufferS->resolveColorAttachments[i]->currentUsage = TextureUsage::FramebufferAttachment;
					framebufferS->resolveColorAttachments[i]->currentStageFlags =
						VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				}
			}
		}

		if (framebufferS->resolveDepthStencilAttachment != nullptr)
		{
			depthStencilResolveAttachmentFormat = framebufferS->resolveDepthStencilAttachment->format;

			if (framebufferS->resolveDepthStencilAttachment->autoBarrier)
			{
				framebufferS->resolveDepthStencilAttachment->currentUsage = TextureUsage::FramebufferAttachment;
				framebufferS->resolveDepthStencilAttachment->currentStageFlags =
					VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			}
		}

		// Fetches the initial layout and format for the depth stencil attachment and updates the
		//  auto barrier usage state to reflect the changes made in the render pass.
		if (framebufferS->depthStencilAttachment != nullptr)
		{
			depthStencilFormat = framebufferS->depthStencilAttachment->format;
			if (beginInfo.depthStencilReadOnly)
			{
				depthStencilImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
				if (framebufferS->depthStencilAttachment->autoBarrier)
				{
					framebufferS->depthStencilAttachment->currentUsage = TextureUsage::DepthStencilReadOnly;
					framebufferS->depthStencilAttachment->currentStageFlags = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
				}
			}
			else if (framebufferS->depthStencilAttachment->autoBarrier)
			{
				depthStencilImageLayout = framebufferS->depthStencilAttachment->CurrentLayout();
				framebufferS->depthStencilAttachment->currentUsage = TextureUsage::FramebufferAttachment;
				framebufferS->depthStencilAttachment->currentStageFlags = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			}
			else if (beginInfo.depthLoadOp == eg::AttachmentLoadOp::Load)
			{
				depthStencilImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}
			else
			{
				depthStencilImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			}
		}
	}

	uint32_t clearValueShift = 0;
	VkClearValue clearValues[MAX_COLOR_ATTACHMENTS + 1] = {};

	RenderPassDescription renderPassDescription;
	if (depthStencilFormat != VK_FORMAT_UNDEFINED)
	{
		renderPassDescription.depthStencilReadOnly = beginInfo.depthStencilReadOnly;

		renderPassDescription.depthAttachment.format = depthStencilFormat;
		renderPassDescription.depthAttachment.samples = sampleCount;
		renderPassDescription.depthAttachment.loadOp = TranslateLoadOp(beginInfo.depthLoadOp);
		renderPassDescription.depthAttachment.stencilLoadOp = TranslateLoadOp(beginInfo.stencilLoadOp);
		renderPassDescription.depthAttachment.storeOp = TranslateStoreOp(beginInfo.depthStoreOp);
		renderPassDescription.depthAttachment.stencilStoreOp = TranslateStoreOp(beginInfo.stencilStoreOp);
		renderPassDescription.depthAttachment.initialLayout = depthStencilImageLayout;

		if (beginInfo.depthStencilReadOnly)
			renderPassDescription.depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		else
			renderPassDescription.depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		if (beginInfo.depthLoadOp == AttachmentLoadOp::Load && changeLoadToClear)
		{
			renderPassDescription.depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			renderPassDescription.depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			clearValues[0].depthStencil.depth = 1.0f;
			clearValues[0].depthStencil.stencil = 0;
		}
		else if (beginInfo.depthLoadOp == AttachmentLoadOp::Clear)
		{
			clearValues[0].depthStencil.depth = beginInfo.depthClearValue;
			clearValues[0].depthStencil.stencil = beginInfo.stencilClearValue;
		}

		renderPassDescription.resolveDepthAttachment.format = depthStencilResolveAttachmentFormat;
		renderPassDescription.resolveDepthAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		renderPassDescription.resolveDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

		clearValueShift = 1;
	}

	renderPassDescription.numColorAttachments = numColorAttachments;
	renderPassDescription.numResolveColorAttachments = numColorAttachments;
	for (uint32_t i = 0; i < numColorAttachments; i++)
	{
		renderPassDescription.colorAttachments[i].loadOp = TranslateLoadOp(beginInfo.colorAttachments[i].loadOp);
		renderPassDescription.colorAttachments[i].storeOp = TranslateStoreOp(beginInfo.colorAttachments[i].storeOp);
		renderPassDescription.colorAttachments[i].format = colorFormats[i];
		renderPassDescription.colorAttachments[i].samples = sampleCount;
		renderPassDescription.colorAttachments[i].initialLayout = colorImageInitialLayouts[i];
		renderPassDescription.colorAttachments[i].finalLayout = colorImageFinalLayouts[i];

		if (beginInfo.colorAttachments[i].loadOp == AttachmentLoadOp::Load && changeLoadToClear)
		{
			renderPassDescription.colorAttachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		}
		else if (beginInfo.colorAttachments[i].loadOp == AttachmentLoadOp::Clear)
		{
			std::visit(
				[&](const auto& clearValue)
				{
					static_assert(sizeof(clearValue) == sizeof(VkClearColorValue));
					std::memcpy(&clearValues[i + clearValueShift].color, &clearValue.r, sizeof(VkClearColorValue));
				},
				beginInfo.colorAttachments[i].clearValue);
		}

		renderPassDescription.resolveColorAttachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		renderPassDescription.resolveColorAttachments[i].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		renderPassDescription.resolveColorAttachments[i].format = resolveAttachmentFormats[i];
	}

	VkRenderPassBeginInfo vkBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	vkBeginInfo.renderArea.extent = extent;
	vkBeginInfo.framebuffer = framebuffer;
	vkBeginInfo.renderPass = GetRenderPass(renderPassDescription, false);
	vkBeginInfo.clearValueCount = std::size(clearValues);
	vkBeginInfo.pClearValues = clearValues;

	vkCmdBeginRenderPass(vcc.cb, &vkBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	vcc.framebufferW = extent.width;
	vcc.framebufferH = extent.height;
	vcc.renderPassDepthStencilReadOnly = beginInfo.depthStencilReadOnly;

	SetViewport(cc, 0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height));
	SetScissor(cc, 0, 0, extent.width, extent.height);
}

void EndRenderPass(CommandContextHandle cc)
{
	vkCmdEndRenderPass(UnwrapCC(cc).cb);
}
} // namespace eg::graphics_api::vk
