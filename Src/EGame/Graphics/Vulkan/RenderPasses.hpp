#pragma once

#include "Common.hpp"

namespace eg::graphics_api::vk
{
	struct RenderPassAttachment
	{
		VkFormat format = VK_FORMAT_UNDEFINED;
		uint32_t samples = 1;
		VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		
		bool Equals(const RenderPassAttachment& other, bool checkCompatible) const;
	};
	
	struct RenderPassDescription
	{
		RenderPassAttachment depthAttachment;
		RenderPassAttachment colorAttachments[8];
	};
	
	VkRenderPass GetRenderPass(const RenderPassDescription& description, bool allowCompatible);
	
	void DestroyRenderPasses();
}
