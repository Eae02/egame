#ifndef EG_NO_VULKAN
#include "VulkanMain.hpp"
#include "Texture.hpp"
#include "Common.hpp"
#include "RenderPasses.hpp"
#include "Framebuffer.hpp"
#include "Translation.hpp"
#include "../../Alloc/ObjectPool.hpp"

namespace eg::graphics_api::vk
{
	FramebufferFormat currentFBFormat;
	
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
	
	VkImageView GetAttachmentView(const FramebufferAttachment& attachment)
	{
		return UnwrapTexture(attachment.texture)->GetView(attachment.subresource.AsSubresource());
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
		
		auto ProcessAttachment = [&] (const FramebufferAttachment& attachment, VkFormat& formatOut)
		{
			Texture* texture = UnwrapTexture(attachment.texture);
			const uint32_t layers = attachment.subresource.ResolveRem(texture->numArrayLayers).numArrayLayers;
			
			if (!sizeSet)
			{
				sizeSet = true;
				vkCreateInfo.width = texture->extent.width;
				vkCreateInfo.height = texture->extent.height;
				vkCreateInfo.layers = layers;
				
				//sampleCount should not be set for resolve attachments, but won't since these are processed last.
				sampleCount = texture->sampleCount;
			}
			else if (vkCreateInfo.width != texture->extent.width || vkCreateInfo.height != texture->extent.height ||
			         vkCreateInfo.layers != layers)
			{
				EG_PANIC("Inconsistent framebuffer attachment resolution");
			}
			
			attachments[vkCreateInfo.attachmentCount++] = texture->GetView(attachment.subresource.AsSubresource());
			
			texture->refCount++;
			formatOut = texture->format;
			
			return texture;
		};
		
		if (createInfo.depthStencilAttachment.texture != nullptr)
		{
			framebuffer->depthStencilAttachment = ProcessAttachment(createInfo.depthStencilAttachment,
				rpDescription.depthAttachment.format);
		}
		else
		{
			framebuffer->depthStencilAttachment = nullptr;
		}
		
		//Processes color attachments
		framebuffer->numColorAttachments = (uint32_t)createInfo.colorAttachments.size();
		rpDescription.numColorAttachments = createInfo.colorAttachments.size();
		for (uint32_t i = 0; i < createInfo.colorAttachments.size(); i++)
		{
			framebuffer->colorAttachments[i] = ProcessAttachment(createInfo.colorAttachments[i],
				rpDescription.colorAttachments[i].format);
			rpDescription.colorAttachments[i].samples = sampleCount;
		}
		
		//Processes color resolve attachments
		rpDescription.numResolveColorAttachments = createInfo.colorResolveAttachments.size();
		std::fill_n(framebuffer->resolveColorAttachments, MAX_COLOR_ATTACHMENTS, nullptr);
		for (uint32_t i = 0; i < createInfo.colorResolveAttachments.size(); i++)
		{
			if (createInfo.colorResolveAttachments[i].texture != nullptr)
			{
				framebuffer->resolveColorAttachments[i] =
					ProcessAttachment(createInfo.colorResolveAttachments[i], rpDescription.resolveColorAttachments[i].format);
			}
		}
		
		//Processes the depth resolve attachment
		if (createInfo.depthStencilResolveAttachment.texture != nullptr)
		{
			framebuffer->resolveDepthStencilAttachment = ProcessAttachment(createInfo.depthStencilResolveAttachment,
				rpDescription.resolveDepthAttachment.format);
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
	
	void MaybeAcquireSwapchainImage();
	
	void BeginRenderPass(CommandContextHandle cc, const RenderPassBeginInfo& beginInfo)
	{
		VkCommandBuffer cb = GetCB(cc);
		
		uint32_t numColorAttachments;
		VkImageLayout colorImageLayouts[MAX_COLOR_ATTACHMENTS];
		VkImageLayout depthStencilImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		VkFramebuffer framebuffer;
		VkExtent2D extent;
		
		VkFormat resolveAttachmentFormats[MAX_COLOR_ATTACHMENTS] = { };
		VkFormat depthStencilResolveAttachmentFormat = VK_FORMAT_UNDEFINED;
		
		currentFBFormat.depthStencilFormat = VK_FORMAT_UNDEFINED;
		std::fill_n(currentFBFormat.colorFormats, MAX_COLOR_ATTACHMENTS, VK_FORMAT_UNDEFINED);
		
		bool changeLoadToClear = false;
		
		if (beginInfo.framebuffer == nullptr)
		{
			MaybeAcquireSwapchainImage();
			
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
			
			currentFBFormat.sampleCount = framebufferS->sampleCount;
			
			//Fetches the initial layouts and formats of color attachments and updates
			// auto barrier usage state to reflect the changes made in the render pass.
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
				else if (beginInfo.colorAttachments[i].loadOp == eg::AttachmentLoadOp::Load)
				{
					colorImageLayouts[i] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				}
				else
				{
					colorImageLayouts[i] = VK_IMAGE_LAYOUT_UNDEFINED;
				}
			}
			
			//Fetches formats of resolve color attachments and updates
			// auto barrier usage state to reflect the changes made in the render pass.
			for (uint32_t i = 0; i < MAX_COLOR_ATTACHMENTS; i++)
			{
				if (framebufferS->resolveColorAttachments[i] != nullptr)
				{
					resolveAttachmentFormats[i] = framebufferS->resolveColorAttachments[i]->format;
					
					if (framebufferS->resolveColorAttachments[i]->autoBarrier)
					{
						framebufferS->resolveColorAttachments[i]->currentUsage = TextureUsage::FramebufferAttachment;
						framebufferS->resolveColorAttachments[i]->currentStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
					}
				}
			}
			
			if (framebufferS->resolveDepthStencilAttachment != nullptr)
			{
				depthStencilResolveAttachmentFormat = framebufferS->resolveDepthStencilAttachment->format;
				
				if (framebufferS->resolveDepthStencilAttachment->autoBarrier)
				{
					framebufferS->resolveDepthStencilAttachment->currentUsage = TextureUsage::FramebufferAttachment;
					framebufferS->resolveDepthStencilAttachment->currentStageFlags = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
				}
			}
			
			//Fetches the initial layout and format for the depth stencil attachment and updates the
			// auto barrier usage state to reflect the changes made in the render pass.
			if (framebufferS->depthStencilAttachment != nullptr)
			{
				currentFBFormat.depthStencilFormat = framebufferS->depthStencilAttachment->format;
				if (framebufferS->depthStencilAttachment->autoBarrier)
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
			
			renderPassDescription.resolveDepthAttachment.format = depthStencilResolveAttachmentFormat;
			renderPassDescription.resolveDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			
			clearValueShift = 1;
		}
		
		renderPassDescription.numColorAttachments = numColorAttachments;
		renderPassDescription.numResolveColorAttachments = numColorAttachments;
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
				std::copy_n(&beginInfo.colorAttachments[i].clearValue.r, 4,
				            clearValues[i + clearValueShift].color.float32);
			}
			
			renderPassDescription.resolveColorAttachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			renderPassDescription.resolveColorAttachments[i].format = resolveAttachmentFormats[i];
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

#endif
