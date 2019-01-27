#pragma once

#include "Common.hpp"

namespace eg::graphics_api::vk
{
	struct RenderPassAttachment
	{
		VkFormat format = VK_FORMAT_UNDEFINED;
		uint32_t samples = 1;
		VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; //Only used if loadOp is set to load
		
		bool Equals(const RenderPassAttachment& other, bool checkCompatible) const;
	};
	
	struct RenderPassDescription
	{
		RenderPassDescription() = default;
		
		RenderPassAttachment depthAttachment;
		RenderPassAttachment colorAttachments[8];
	};
	
	VkRenderPass GetRenderPass(const RenderPassDescription& description, bool allowCompatible);
	
	void DestroyRenderPasses();
}
