#include "VulkanMain.hpp"
#include "Common.hpp"
#include "RenderPasses.hpp"

namespace eg::graphics_api::vk
{
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
	
	void BeginRenderPass(CommandContextHandle cc, const RenderPassBeginInfo& beginInfo)
	{
		VkCommandBuffer cb = GetCB(cc);
		
		int numColorAttachments;
		VkFormat colorFormats[MAX_COLOR_ATTACHMENTS];
		VkFormat depthStencilFormat;
		VkFramebuffer framebuffer;
		VkExtent2D extent;
		VkSampleCountFlags numSamples = VK_SAMPLE_COUNT_1_BIT;
		
		if (beginInfo.framebuffer == nullptr)
		{
			numColorAttachments = 1;
			extent = ctx.surfaceExtent;
			framebuffer = ctx.defaultFramebuffers[ctx.currentImage];
			colorFormats[0] = ctx.surfaceFormat.format;
			depthStencilFormat = ctx.defaultDSFormat;
			ctx.defaultFramebufferInPresentMode = false;
		}
		else
		{
			EG_UNREACHABLE
		}
		
		uint32_t clearValueShift = 0;
		VkClearValue clearValues[MAX_COLOR_ATTACHMENTS + 1] = { };
		
		RenderPassDescription renderPassDescription;
		if (depthStencilFormat != VK_FORMAT_UNDEFINED)
		{
			renderPassDescription.depthAttachment.format = depthStencilFormat;
			renderPassDescription.depthAttachment.samples = numSamples;
			renderPassDescription.depthAttachment.loadOp = TranslateLoadOp(beginInfo.depthLoadOp);
			
			if (beginInfo.depthLoadOp == AttachmentLoadOp::Clear)
			{
				clearValues[0].depthStencil.depth = beginInfo.depthClearValue;
				clearValues[0].depthStencil.stencil = beginInfo.stencilClearValue;
			}
			
			clearValueShift = 1;
		}
		
		for (int i = 0; i < numColorAttachments; i++)
		{
			renderPassDescription.colorAttachments[i].loadOp = TranslateLoadOp(beginInfo.colorAttachments[i].loadOp);
			renderPassDescription.colorAttachments[i].format = colorFormats[i];
			renderPassDescription.colorAttachments[i].samples = numSamples;
			if (beginInfo.colorAttachments[i].loadOp == AttachmentLoadOp::Clear)
			{
				std::copy_n(&beginInfo.colorAttachments[i].clearValue.r, 4, clearValues[i + clearValueShift].color.float32);
			}
		}
		
		VkRenderPassBeginInfo vkBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		vkBeginInfo.renderArea.extent = extent;
		vkBeginInfo.framebuffer = framebuffer;
		vkBeginInfo.renderPass = GetRenderPass(renderPassDescription, false);
		vkBeginInfo.clearValueCount = ArrayLen(clearValues);
		vkBeginInfo.pClearValues = clearValues;
		
		vkCmdBeginRenderPass(cb, &vkBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		
		CommandContextState& ctxState = GetCtxState(cc);
		ctxState.framebufferW = extent.width;
		ctxState.framebufferH = extent.height;
		
		SetViewport(cc, 0, 0, extent.width, extent.height);
		SetScissor(cc, 0, 0, extent.width, extent.height);
	}
	
	void EndRenderPass(CommandContextHandle cc)
	{
		vkCmdEndRenderPass(GetCB(cc));
	}
}
