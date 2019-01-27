#include "RenderPasses.hpp"

#include <list>

namespace eg::graphics_api::vk
{
	bool RenderPassAttachment::Equals(const RenderPassAttachment& other, bool checkCompatible) const
	{
		if (format == VK_FORMAT_UNDEFINED && other.format == VK_FORMAT_UNDEFINED)
			return true;
		if (!checkCompatible && loadOp != other.loadOp)
			return false;
		if (loadOp == VK_ATTACHMENT_LOAD_OP_LOAD && initialLayout != other.initialLayout)
			return false;
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
		//Searches for a compatible render pass in the cache.
		for (const RenderPass& renderPass : renderPasses)
		{
			if (!renderPass.description.depthAttachment.Equals(description.depthAttachment, allowCompatible))
				continue;
			
			bool ok = true;
			for (uint32_t i = 0; i < 8; i++)
			{
				if (!renderPass.description.colorAttachments[i].Equals(description.colorAttachments[i], allowCompatible))
				{
					ok = false;
					break;
				}
			}
			
			if (ok)
				return renderPass.renderPass;
		}
		
		VkAttachmentDescription attachments[MAX_COLOR_ATTACHMENTS + 1];
		
		VkRenderPassCreateInfo createInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
		createInfo.pAttachments = attachments;
		createInfo.attachmentCount = 0;
		
		VkSubpassDescription subpassDescription = { };
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		
		createInfo.subpassCount = 1;
		createInfo.pSubpasses = &subpassDescription;
		
		auto AddAttachment = [&] (const RenderPassAttachment& attachment, VkImageLayout initialLayout, VkImageLayout finalLayout)
		{
			VkAttachmentDescription& attachmentDesc = attachments[createInfo.attachmentCount++];
			attachmentDesc.flags = 0;
			attachmentDesc.format = attachment.format;
			attachmentDesc.samples = (VkSampleCountFlagBits)attachment.samples;
			attachmentDesc.loadOp = attachment.loadOp;
			attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachmentDesc.initialLayout = attachment.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD ? initialLayout : VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDesc.finalLayout = finalLayout;
		};
		
		//Adds the depth attachment if one exists
		VkAttachmentReference depthStencilAttachmentRef;
		if (description.depthAttachment.format != VK_FORMAT_UNDEFINED)
		{
			depthStencilAttachmentRef.attachment = createInfo.attachmentCount;
			depthStencilAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			subpassDescription.pDepthStencilAttachment = &depthStencilAttachmentRef;
			AddAttachment(description.depthAttachment, description.depthAttachment.initialLayout,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		}
		
		//Adds color attachments
		VkAttachmentReference colorAttachmentRefs[8];
		subpassDescription.pColorAttachments = colorAttachmentRefs;
		subpassDescription.colorAttachmentCount = 0;
		for (const RenderPassAttachment& attachment : description.colorAttachments)
		{
			if (attachment.format == VK_FORMAT_UNDEFINED)
				continue;
			
			uint32_t idx = subpassDescription.colorAttachmentCount++;
			colorAttachmentRefs[idx].attachment = createInfo.attachmentCount;
			colorAttachmentRefs[idx].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			
			AddAttachment(attachment, attachment.initialLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		}
		
		RenderPass& renderPass = renderPasses.emplace_back();
		CheckRes(vkCreateRenderPass(ctx.device, &createInfo, nullptr, &renderPass.renderPass));
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
}
