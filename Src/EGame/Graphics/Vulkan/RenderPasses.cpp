#ifndef EG_NO_VULKAN
#include "RenderPasses.hpp"
#include "../../Assert.hpp"

#include <list>

namespace eg::graphics_api::vk
{
bool RenderPassAttachment::Equals(const RenderPassAttachment& other, bool equalIfCompatible) const
{
	if (format == VK_FORMAT_UNDEFINED && other.format == VK_FORMAT_UNDEFINED)
		return true;
	if (!equalIfCompatible)
	{
		if (loadOp != other.loadOp || storeOp != other.storeOp || finalLayout != other.finalLayout)
			return false;
		if (loadOp == VK_ATTACHMENT_LOAD_OP_LOAD && initialLayout != other.initialLayout)
			return false;
	}
	return format == other.format && samples == other.samples;
}

struct RenderPass
{
	VkRenderPass renderPass;
	RenderPassDescription description;
};

static std::list<RenderPass> renderPasses;

VkRenderPass GetRenderPass(const RenderPassDescription& description, bool allowCompatible)
{
	// Searches for a compatible render pass in the cache.
	for (const RenderPass& renderPass : renderPasses)
	{
		if (!renderPass.description.depthAttachment.Equals(description.depthAttachment, allowCompatible) ||
		    (!allowCompatible &&
		     (!renderPass.description.resolveDepthAttachment.Equals(description.resolveDepthAttachment, true) ||
		      renderPass.description.depthStencilReadOnly != description.depthStencilReadOnly)))
		{
			continue;
		}

		bool ok = true;
		for (uint32_t i = 0; i < 8; i++)
		{
			if (!renderPass.description.colorAttachments[i].Equals(description.colorAttachments[i], allowCompatible) ||
			    (!allowCompatible && !renderPass.description.resolveColorAttachments[i].Equals(
										 description.resolveColorAttachments[i], true)))
			{
				ok = false;
				break;
			}
		}

		if (ok)
			return renderPass.renderPass;
	}

	VkAttachmentDescription2KHR attachments[MAX_COLOR_ATTACHMENTS + 1];

	VkRenderPassCreateInfo2KHR createInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2_KHR };
	createInfo.pAttachments = attachments;
	createInfo.attachmentCount = 0;

	VkSubpassDescription2KHR subpassDescription = { VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2_KHR };
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &subpassDescription;

	auto AddAttachment = [&](const RenderPassAttachment& attachment)
	{
		EG_DEBUG_ASSERT(attachment.finalLayout != VK_IMAGE_LAYOUT_UNDEFINED);
		EG_DEBUG_ASSERT(
			attachment.initialLayout != VK_IMAGE_LAYOUT_UNDEFINED || attachment.loadOp != VK_ATTACHMENT_LOAD_OP_LOAD);

		VkAttachmentDescription2KHR& attachmentDesc = attachments[createInfo.attachmentCount++];
		attachmentDesc.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2_KHR;
		attachmentDesc.pNext = nullptr;
		attachmentDesc.flags = 0;
		attachmentDesc.format = attachment.format;
		attachmentDesc.samples = static_cast<VkSampleCountFlagBits>(attachment.samples);
		attachmentDesc.loadOp = attachment.loadOp;
		attachmentDesc.stencilLoadOp = attachment.stencilLoadOp;
		attachmentDesc.storeOp = attachment.storeOp;
		attachmentDesc.stencilStoreOp = attachment.storeOp;
		attachmentDesc.finalLayout = attachment.finalLayout;

		if (attachment.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD)
			attachmentDesc.initialLayout = attachment.initialLayout;
		else
			attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	};

	// Adds the depth attachment if one exists
	VkAttachmentReference2KHR depthStencilAttachmentRef;
	if (description.depthAttachment.format != VK_FORMAT_UNDEFINED)
	{
		VkImageLayout layout = description.depthStencilReadOnly ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
		                                                        : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		depthStencilAttachmentRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR;
		depthStencilAttachmentRef.pNext = nullptr;
		depthStencilAttachmentRef.attachment = createInfo.attachmentCount;
		depthStencilAttachmentRef.layout = layout;
		depthStencilAttachmentRef.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		if (HasStencil(description.depthAttachment.format))
			depthStencilAttachmentRef.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

		subpassDescription.pDepthStencilAttachment = &depthStencilAttachmentRef;
		AddAttachment(description.depthAttachment);
	}

	// Adds color attachments
	VkAttachmentReference2KHR colorAttachmentRefs[8];
	subpassDescription.pColorAttachments = colorAttachmentRefs;
	subpassDescription.colorAttachmentCount = description.numColorAttachments;
	for (uint32_t i = 0; i < description.numColorAttachments; i++)
	{
		colorAttachmentRefs[i].sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR;
		colorAttachmentRefs[i].pNext = nullptr;
		colorAttachmentRefs[i].attachment = createInfo.attachmentCount;
		colorAttachmentRefs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachmentRefs[i].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		AddAttachment(description.colorAttachments[i]);
	}

	// Adds resolve color attachments
	VkAttachmentReference2KHR colorResolveAttachmentRefs[8];
	subpassDescription.pResolveAttachments = colorResolveAttachmentRefs;
	for (uint32_t i = 0; i < description.numColorAttachments; i++)
	{
		colorResolveAttachmentRefs[i].sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR;
		colorResolveAttachmentRefs[i].pNext = nullptr;
		colorResolveAttachmentRefs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorResolveAttachmentRefs[i].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

		if (i < description.numResolveColorAttachments &&
		    description.resolveColorAttachments[i].format != VK_FORMAT_UNDEFINED)
		{
			colorResolveAttachmentRefs[i].attachment = createInfo.attachmentCount;
			AddAttachment(description.resolveColorAttachments[i]);
		}
		else
		{
			colorResolveAttachmentRefs[i].attachment = VK_ATTACHMENT_UNUSED;
		}
	}

	// Adds the depth stencil resolve attachment if one exists
	VkSubpassDescriptionDepthStencilResolveKHR depthStencilResolve;
	VkAttachmentReference2KHR depthStencilResolveAttachmentRef;
	if (description.resolveDepthAttachment.format != VK_FORMAT_UNDEFINED)
	{
		depthStencilResolveAttachmentRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR;
		depthStencilResolveAttachmentRef.pNext = nullptr;
		depthStencilResolveAttachmentRef.attachment = createInfo.attachmentCount;
		depthStencilResolveAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depthStencilResolveAttachmentRef.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		depthStencilResolve.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE_KHR;
		depthStencilResolve.pNext = subpassDescription.pNext;
		depthStencilResolve.depthResolveMode = VK_RESOLVE_MODE_AVERAGE_BIT_KHR;
		depthStencilResolve.stencilResolveMode = VK_RESOLVE_MODE_NONE_KHR;
		depthStencilResolve.pDepthStencilResolveAttachment = &depthStencilResolveAttachmentRef;

		subpassDescription.pNext = &depthStencilResolve;

		AddAttachment(description.resolveDepthAttachment);
	}

	RenderPass& renderPass = renderPasses.emplace_back();
	CheckRes(vkCreateRenderPass2KHR(ctx.device, &createInfo, nullptr, &renderPass.renderPass));
	renderPass.description = description;

	return renderPass.renderPass;
}

void DestroyRenderPasses()
{
	for (const RenderPass& renderPass : renderPasses)
	{
		vkDestroyRenderPass(ctx.device, renderPass.renderPass, nullptr);
	}
}
} // namespace eg::graphics_api::vk

#endif
