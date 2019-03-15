#include "VulkanMain.hpp"
#include "VulkanTexture.hpp"
#include "Common.hpp"
#include "RenderPasses.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "VulkanFramebuffer.hpp"


namespace eg::graphics_api::vk
{
	FramebufferFormat currentFBFormat;
	
	struct Framebuffer : Resource
	{
		VkFramebuffer framebuffer;
		uint32_t numColorAttachments;
		VkExtent2D extent;
		Texture* colorAttachments[MAX_COLOR_ATTACHMENTS];
		Texture* depthStencilAttachment;
		
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
	
	FramebufferHandle CreateFramebuffer(Span<const TextureHandle> colorAttachments, TextureHandle dsAttachment)
	{
		Framebuffer* framebuffer = framebufferPool.New();
		framebuffer->refCount = 1;
		
		VkImageView attachments[MAX_COLOR_ATTACHMENTS + 1];
		
		RenderPassDescription rpDescription;
		
		VkFramebufferCreateInfo createInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		createInfo.pAttachments = attachments;
		createInfo.layers = 1;
		
		bool sizeSet = false;
		
		if (dsAttachment != nullptr)
		{
			framebuffer->depthStencilAttachment = UnwrapTexture(dsAttachment);
			attachments[createInfo.attachmentCount++] = framebuffer->depthStencilAttachment->imageView;
			
			framebuffer->depthStencilAttachment->refCount++;
			
			sizeSet = true;
			createInfo.width = framebuffer->depthStencilAttachment->extent.width;
			createInfo.height = framebuffer->depthStencilAttachment->extent.height;
			rpDescription.depthAttachment.format = framebuffer->depthStencilAttachment->format;
		}
		else
		{
			framebuffer->depthStencilAttachment = nullptr;
		}
		
		framebuffer->numColorAttachments = colorAttachments.size();
		for (uint32_t i = 0; i < colorAttachments.size(); i++)
		{
			Texture* texture = UnwrapTexture(colorAttachments[i]);
			texture->refCount++;
			
			framebuffer->colorAttachments[i] = texture;
			attachments[createInfo.attachmentCount++] = texture->imageView;
			rpDescription.colorAttachments[i].format = texture->format;
			
			if (!sizeSet)
			{
				sizeSet = true;
				createInfo.width = framebuffer->colorAttachments[i]->extent.width;
				createInfo.height = framebuffer->colorAttachments[i]->extent.height;
			}
			else if (createInfo.width != texture->extent.width || createInfo.height != texture->extent.height)
			{
				EG_PANIC("Inconsistent framebuffer attachment resolution");
			}
		}
		
		createInfo.renderPass = GetRenderPass(rpDescription, true);
		
		framebuffer->extent.width = createInfo.width;
		framebuffer->extent.height = createInfo.height;
		
		CheckRes(vkCreateFramebuffer(ctx.device, &createInfo, nullptr, &framebuffer->framebuffer));
		
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
	
	void BeginRenderPass(CommandContextHandle cc, const RenderPassBeginInfo& beginInfo)
	{
		VkCommandBuffer cb = GetCB(cc);
		
		uint32_t numColorAttachments;
		VkImageLayout colorImageLayouts[MAX_COLOR_ATTACHMENTS];
		VkImageLayout depthStencilImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		VkFramebuffer framebuffer;
		VkExtent2D extent;
		
		currentFBFormat.depthStencilFormat = VK_FORMAT_UNDEFINED;
		std::fill_n(currentFBFormat.colorFormats, MAX_COLOR_ATTACHMENTS, VK_FORMAT_UNDEFINED);
		
		bool changeLoadToClear = false;
		
		if (beginInfo.framebuffer == nullptr)
		{
			numColorAttachments = 1;
			extent = ctx.surfaceExtent;
			framebuffer = ctx.defaultFramebuffers[ctx.currentImage];
			currentFBFormat.colorFormats[0] = ctx.surfaceFormat.format;
			currentFBFormat.depthStencilFormat = ctx.defaultDSFormat;
			currentFBFormat.sampleCount = VK_SAMPLE_COUNT_1_BIT;
			colorImageLayouts[0] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			changeLoadToClear = ctx.defaultFramebufferInPresentMode;
			ctx.defaultFramebufferInPresentMode = false;
		}
		else
		{
			Framebuffer* framebufferS = UnwrapFramebuffer(beginInfo.framebuffer);
			framebuffer = framebufferS->framebuffer;
			extent = framebufferS->extent;
			
			RefResource(cc, *framebufferS);
			
			currentFBFormat.sampleCount = VK_SAMPLE_COUNT_1_BIT;
			
			numColorAttachments = framebufferS->numColorAttachments;
			for (uint32_t i = 0; i < numColorAttachments; i++)
			{
				currentFBFormat.colorFormats[i] = framebufferS->colorAttachments[i]->format;
				
				if (framebufferS->colorAttachments[i]->autoBarrier)
				{
					colorImageLayouts[i] = framebufferS->colorAttachments[i]->CurrentLayout();
					framebufferS->colorAttachments[i]->currentUsage = TextureUsage::FramebufferAttachment;
					framebufferS->colorAttachments[i]->currentStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				}
				else
				{
					colorImageLayouts[i] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				}
			}
			
			if (framebufferS->depthStencilAttachment != nullptr)
			{
				currentFBFormat.depthStencilFormat = framebufferS->depthStencilAttachment->format;
				if (framebufferS->depthStencilAttachment->autoBarrier)
				{
					depthStencilImageLayout = framebufferS->depthStencilAttachment->CurrentLayout();
					framebufferS->depthStencilAttachment->currentUsage = TextureUsage::FramebufferAttachment;
					framebufferS->depthStencilAttachment->currentStageFlags = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
				}
			}
		}
		
		currentFBFormat.CalcHash();
		
		uint32_t clearValueShift = 0;
		VkClearValue clearValues[MAX_COLOR_ATTACHMENTS + 1] = { };
		
		RenderPassDescription renderPassDescription;
		if (currentFBFormat.depthStencilFormat != VK_FORMAT_UNDEFINED)
		{
			renderPassDescription.depthAttachment.format = currentFBFormat.depthStencilFormat;
			renderPassDescription.depthAttachment.samples = currentFBFormat.sampleCount;
			renderPassDescription.depthAttachment.loadOp = TranslateLoadOp(beginInfo.depthLoadOp);
			renderPassDescription.depthAttachment.initialLayout = depthStencilImageLayout;
			
			if (beginInfo.depthLoadOp == AttachmentLoadOp::Load && changeLoadToClear)
			{
				renderPassDescription.depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				clearValues[0].depthStencil.depth = 1.0f;
				clearValues[0].depthStencil.stencil = 0;
			}
			else if (beginInfo.depthLoadOp == AttachmentLoadOp::Clear)
			{
				clearValues[0].depthStencil.depth = beginInfo.depthClearValue;
				clearValues[0].depthStencil.stencil = beginInfo.stencilClearValue;
			}
			
			clearValueShift = 1;
		}
		
		for (uint32_t i = 0; i < numColorAttachments; i++)
		{
			renderPassDescription.colorAttachments[i].loadOp = TranslateLoadOp(beginInfo.colorAttachments[i].loadOp);
			renderPassDescription.colorAttachments[i].format = currentFBFormat.colorFormats[i];
			renderPassDescription.colorAttachments[i].samples = currentFBFormat.sampleCount;
			renderPassDescription.colorAttachments[i].initialLayout = colorImageLayouts[i];
			
			if (beginInfo.colorAttachments[i].loadOp == AttachmentLoadOp::Load && changeLoadToClear)
			{
				renderPassDescription.colorAttachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			}
			else if (beginInfo.colorAttachments[i].loadOp == AttachmentLoadOp::Clear)
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
	
	void FramebufferFormat::CalcHash()
	{
		hash = 0;
		HashAppend(hash, sampleCount);
		HashAppend(hash, (int)depthStencilFormat);
		for (VkFormat colorFormat : colorFormats)
			HashAppend(hash, (int)colorFormat);
	}
	
	FramebufferFormat FramebufferFormat::FromHint(const FramebufferFormatHint& hint)
	{
		FramebufferFormat res;
		res.sampleCount = (VkSampleCountFlags)hint.sampleCount;
		res.depthStencilFormat = TranslateFormat(hint.depthStencilFormat);
		for (uint32_t i = 0; i < MAX_COLOR_ATTACHMENTS; i++)
			res.colorFormats[i] = TranslateFormat(hint.colorFormats[i]);
		res.CalcHash();
		return res;
	}
}
