#ifndef EG_NO_VULKAN
#include "Texture.hpp"
#include "Buffer.hpp"
#include "Sampler.hpp"
#include "Translation.hpp"
#include "Pipeline.hpp"
#include "../Graphics.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "../../Utils.hpp"

namespace eg::graphics_api::vk
{
	static_assert(REMAINING_SUBRESOURCE == VK_REMAINING_MIP_LEVELS);
	static_assert(REMAINING_SUBRESOURCE == VK_REMAINING_ARRAY_LAYERS);
	
	ConcurrentObjectPool<Texture> texturePool;
	
	void Texture::Free()
	{
		for (const TextureView& view : views)
			vkDestroyImageView(ctx.device, view.view, nullptr);
		
		vmaDestroyImage(ctx.allocator, image, allocation);
		
		texturePool.Delete(this);
	}
	
	static void InitializeImage(Texture& texture, const TextureCreateInfo& createInfo, VkImageType imageType,
		VkImageViewType viewType, const VkExtent3D& extent, uint32_t arrayLayers)
	{
		texture.refCount = 1;
		texture.aspectFlags = GetFormatAspect(createInfo.format);
		texture.viewType = viewType;
		texture.numMipLevels = createInfo.mipLevels;
		texture.numArrayLayers = arrayLayers;
		texture.autoBarrier = !HasFlag(createInfo.flags, TextureFlags::ManualBarrier);
		texture.currentUsage = TextureUsage::Undefined;
		texture.currentStageFlags = 0;
		texture.extent = extent;
		texture.sampleCount = std::max(createInfo.sampleCount, 1U);
		texture.format = TranslateFormat(createInfo.format);
		
		//Creates the image
		VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageCreateInfo.extent = extent;
		imageCreateInfo.format = texture.format;
		imageCreateInfo.imageType = imageType;
		imageCreateInfo.samples = (VkSampleCountFlagBits)texture.sampleCount;
		imageCreateInfo.mipLevels = createInfo.mipLevels;
		imageCreateInfo.arrayLayers = arrayLayers;
		
		if (HasFlag(createInfo.flags, TextureFlags::CopySrc) || HasFlag(createInfo.flags, TextureFlags::GenerateMipmaps))
			imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		if (HasFlag(createInfo.flags, TextureFlags::CopyDst) || HasFlag(createInfo.flags, TextureFlags::GenerateMipmaps))
			imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		if (HasFlag(createInfo.flags, TextureFlags::ShaderSample))
			imageCreateInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
		if (HasFlag(createInfo.flags, TextureFlags::StorageImage))
			imageCreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
		if (HasFlag(createInfo.flags, TextureFlags::FramebufferAttachment))
		{
			if (texture.aspectFlags == VK_IMAGE_ASPECT_COLOR_BIT)
				imageCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			else
				imageCreateInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
		
		if (viewType == VK_IMAGE_VIEW_TYPE_CUBE || viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
			imageCreateInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		
		VmaAllocationCreateInfo allocationCreateInfo = { };
		allocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		CheckRes(vmaCreateImage(ctx.allocator, &imageCreateInfo, &allocationCreateInfo, &texture.image,
			&texture.allocation, nullptr));
		
		if (createInfo.label != nullptr)
		{
			texture.viewLabel = Concat({ createInfo.label, " [View]" });
			SetObjectName(reinterpret_cast<uint64_t>(texture.image), VK_OBJECT_TYPE_IMAGE, createInfo.label);
		}
		
		//Creates the default sampler
		if (createInfo.defaultSamplerDescription != nullptr)
		{
			texture.defaultSampler = GetSampler(*createInfo.defaultSamplerDescription);
		}
		else
		{
			texture.defaultSampler = VK_NULL_HANDLE;
		}
	}
	
	VkImageView Texture::GetView(const TextureSubresource& subresource, VkImageAspectFlags _aspectFlags)
	{
		if (_aspectFlags == 0)
			_aspectFlags = aspectFlags;
		TextureSubresource resolvedSubresource = subresource.ResolveRem(numMipLevels, numArrayLayers);
		
		for (const TextureView& view : views)
		{
			if (view.subresource == resolvedSubresource && view.aspectFlags == _aspectFlags)
				return view.view;
		}
		
		VkImageViewCreateInfo viewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		viewCreateInfo.viewType = viewType;
		viewCreateInfo.image = image;
		viewCreateInfo.format = format;
		viewCreateInfo.subresourceRange.aspectMask = _aspectFlags;
		viewCreateInfo.subresourceRange.baseMipLevel = resolvedSubresource.firstMipLevel;
		viewCreateInfo.subresourceRange.levelCount = resolvedSubresource.numMipLevels;
		viewCreateInfo.subresourceRange.baseArrayLayer = resolvedSubresource.firstArrayLayer;
		viewCreateInfo.subresourceRange.layerCount = resolvedSubresource.numArrayLayers;
		
		TextureView& view = views.emplace_back();
		CheckRes(vkCreateImageView(ctx.device, &viewCreateInfo, nullptr, &view.view));
		
		if (!viewLabel.empty())
		{
			SetObjectName(reinterpret_cast<uint64_t>(view.view), VK_OBJECT_TYPE_IMAGE_VIEW, viewLabel.c_str());
		}
		
		view.aspectFlags = _aspectFlags;
		view.subresource = resolvedSubresource;
		return view.view;
	}
	
	inline TextureHandle WrapTexture(Texture* texture)
	{
		return reinterpret_cast<TextureHandle>(texture);
	}
	
	TextureHandle CreateTexture2D(const TextureCreateInfo& createInfo)
	{
		Texture* texture = texturePool.New();
		
		InitializeImage(*texture, createInfo, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D,
			{ createInfo.width, createInfo.height, 1 }, 1);
		
		return WrapTexture(texture);
	}
	
	TextureHandle CreateTexture2DArray(const TextureCreateInfo& createInfo)
	{
		Texture* texture = texturePool.New();
		
		InitializeImage(*texture, createInfo, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D_ARRAY,
			{ createInfo.width, createInfo.height, 1 }, createInfo.arrayLayers);
		
		return WrapTexture(texture);
	}
	
	TextureHandle CreateTextureCube(const TextureCreateInfo& createInfo)
	{
		Texture* texture = texturePool.New();
		
		InitializeImage(*texture, createInfo, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_CUBE,
			{ createInfo.width, createInfo.width, 1 }, 6);
		
		return WrapTexture(texture);
	}
	
	TextureHandle CreateTextureCubeArray(const TextureCreateInfo& createInfo)
	{
		Texture* texture = texturePool.New();
		
		InitializeImage(*texture, createInfo, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,
			{ createInfo.width, createInfo.width, 1 }, 6 * createInfo.arrayLayers);
		
		return WrapTexture(texture);
	}
	
	TextureHandle CreateTexture3D(const TextureCreateInfo& createInfo)
	{
		Texture* texture = texturePool.New();
		
		InitializeImage(*texture, createInfo, VK_IMAGE_TYPE_3D, VK_IMAGE_VIEW_TYPE_3D,
			{ createInfo.width, createInfo.height, createInfo.depth }, 1);
		
		return WrapTexture(texture);
	}
	
	void DestroyTexture(TextureHandle handle)
	{
		UnwrapTexture(handle)->UnRef();
	}
	
	inline VkAccessFlags GetBarrierAccess(TextureUsage usage, VkImageAspectFlags aspectFlags)
	{
		switch (usage)
		{
		case TextureUsage::Undefined: return 0;
		case TextureUsage::CopySrc: return VK_ACCESS_TRANSFER_READ_BIT;
		case TextureUsage::CopyDst: return VK_ACCESS_TRANSFER_WRITE_BIT;
		case TextureUsage::ShaderSample: return VK_ACCESS_SHADER_READ_BIT;
		case TextureUsage::ILSRead: return VK_ACCESS_SHADER_READ_BIT;
		case TextureUsage::ILSWrite: return VK_ACCESS_SHADER_WRITE_BIT;
		case TextureUsage::ILSReadWrite: return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		case TextureUsage::FramebufferAttachment:
			if (aspectFlags == VK_IMAGE_ASPECT_COLOR_BIT)
				return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			else
				return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}
		EG_UNREACHABLE
	}
	
	VkImageLayout ImageLayoutFromUsage(TextureUsage usage, VkImageAspectFlags aspectFlags)
	{
		switch (usage)
		{
		case TextureUsage::Undefined: return VK_IMAGE_LAYOUT_UNDEFINED;
		case TextureUsage::CopySrc: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		case TextureUsage::CopyDst: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		case TextureUsage::ILSRead: return VK_IMAGE_LAYOUT_GENERAL;
		case TextureUsage::ILSWrite: return VK_IMAGE_LAYOUT_GENERAL;
		case TextureUsage::ILSReadWrite: return VK_IMAGE_LAYOUT_GENERAL;
		case TextureUsage::ShaderSample:
			if (aspectFlags == VK_IMAGE_ASPECT_COLOR_BIT)
				return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			else
				return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		case TextureUsage::FramebufferAttachment:
			if (aspectFlags == VK_IMAGE_ASPECT_COLOR_BIT)
				return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			else
				return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		EG_UNREACHABLE
	}
	
	inline VkPipelineStageFlags GetBarrierStageFlags(TextureUsage usage, ShaderAccessFlags shaderAccessFlags)
	{
		switch (usage)
		{
		case TextureUsage::Undefined: return 0;
		case TextureUsage::CopySrc: return VK_PIPELINE_STAGE_TRANSFER_BIT;
		case TextureUsage::CopyDst: return VK_PIPELINE_STAGE_TRANSFER_BIT;
		case TextureUsage::FramebufferAttachment:
			return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
			       VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
			       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		case TextureUsage::ILSRead:
		case TextureUsage::ILSWrite:
		case TextureUsage::ILSReadWrite:
		case TextureUsage::ShaderSample:
			return TranslateShaderAccess(shaderAccessFlags);
		}
		EG_UNREACHABLE
	}
	
	void Texture::AutoBarrier(VkCommandBuffer cb, TextureUsage newUsage, ShaderAccessFlags shaderAccessFlags)
	{
		if (!autoBarrier || currentUsage == newUsage)
			return;
		
		VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		barrier.image = image;
		barrier.srcAccessMask = GetBarrierAccess(currentUsage, aspectFlags);
		barrier.dstAccessMask = GetBarrierAccess(newUsage, aspectFlags);
		barrier.oldLayout = ImageLayoutFromUsage(currentUsage, aspectFlags);
		barrier.newLayout = ImageLayoutFromUsage(newUsage, aspectFlags);
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange = { aspectFlags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
		
		VkPipelineStageFlags dstStageFlags = GetBarrierStageFlags(newUsage, shaderAccessFlags);
		if (currentStageFlags == 0)
			currentStageFlags = dstStageFlags;
		
		vkCmdPipelineBarrier(cb, currentStageFlags, dstStageFlags, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		
		currentStageFlags = dstStageFlags;
		currentUsage = newUsage;
	}
	
	void TextureBarrier(CommandContextHandle cc, TextureHandle handle, const eg::TextureBarrier& barrier)
	{
		Texture* texture = UnwrapTexture(handle);
		RefResource(cc, *texture);
		
		VkImageMemoryBarrier vkBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		vkBarrier.image = texture->image;
		vkBarrier.srcAccessMask = GetBarrierAccess(barrier.oldUsage, texture->aspectFlags);
		vkBarrier.dstAccessMask = GetBarrierAccess(barrier.newUsage, texture->aspectFlags);
		vkBarrier.oldLayout = ImageLayoutFromUsage(barrier.oldUsage, texture->aspectFlags);
		vkBarrier.newLayout = ImageLayoutFromUsage(barrier.newUsage, texture->aspectFlags);
		vkBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		vkBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		vkBarrier.subresourceRange.aspectMask = texture->aspectFlags;
		vkBarrier.subresourceRange.baseMipLevel = barrier.subresource.firstMipLevel;
		vkBarrier.subresourceRange.levelCount = barrier.subresource.numMipLevels;
		vkBarrier.subresourceRange.baseArrayLayer = barrier.subresource.firstArrayLayer;
		vkBarrier.subresourceRange.layerCount = barrier.subresource.numArrayLayers;
		
		VkPipelineStageFlags srcStageFlags = GetBarrierStageFlags(barrier.oldUsage, barrier.oldAccess);
		VkPipelineStageFlags dstStageFlags = GetBarrierStageFlags(barrier.newUsage, barrier.newAccess);
		if (srcStageFlags == 0)
			srcStageFlags = dstStageFlags;
		
		vkCmdPipelineBarrier(GetCB(cc), srcStageFlags, dstStageFlags, 0, 0, nullptr, 0, nullptr, 1, &vkBarrier);
	}
	
	void SetTextureData(CommandContextHandle cc, TextureHandle handle, const TextureRange& range,
		BufferHandle bufferHandle, uint64_t offset)
	{
		Buffer* buffer = UnwrapBuffer(bufferHandle);
		RefResource(cc, *buffer);
		
		Texture* texture = UnwrapTexture(handle);
		RefResource(cc, *texture);
		
		VkCommandBuffer cb = GetCB(cc);
		
		texture->AutoBarrier(cb, TextureUsage::CopyDst);
		buffer->AutoBarrier(cb, BufferUsage::CopySrc);
		
		VkBufferImageCopy copyRegion = { };
		copyRegion.bufferOffset = offset;
		copyRegion.imageOffset.x = range.offsetX;
		copyRegion.imageOffset.y = range.offsetY;
		copyRegion.imageOffset.z = range.offsetZ;
		copyRegion.imageExtent.width = range.sizeX;
		copyRegion.imageExtent.height = range.sizeY;
		copyRegion.imageExtent.depth = range.sizeZ;
		copyRegion.imageSubresource = { texture->aspectFlags, range.mipLevel };
		
		switch (texture->viewType)
		{
		case VK_IMAGE_VIEW_TYPE_2D:
			copyRegion.imageOffset.z = 0;
			copyRegion.imageExtent.depth = 1;
			copyRegion.imageSubresource.baseArrayLayer = 0;
			copyRegion.imageSubresource.layerCount = 1;
			break;
		case VK_IMAGE_VIEW_TYPE_3D:
			copyRegion.imageSubresource.baseArrayLayer = 0;
			copyRegion.imageSubresource.layerCount = 1;
			break;
		case VK_IMAGE_VIEW_TYPE_CUBE:
		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
			copyRegion.imageOffset.z = 0;
			copyRegion.imageExtent.depth = 1;
			copyRegion.imageSubresource.baseArrayLayer = range.offsetZ;
			copyRegion.imageSubresource.layerCount = range.sizeZ;
			break;
		default: EG_PANIC("Unknown view type encountered in SetTextureDataBuffer.");
		}
		
		vkCmdCopyBufferToImage(cb, buffer->buffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
	}
	
	void ClearColorTexture(CommandContextHandle cc, TextureHandle handle, uint32_t mipLevel, const void* color)
	{
		Texture* texture = UnwrapTexture(handle);
		RefResource(cc, *texture);
		
		VkCommandBuffer cb = GetCB(cc);
		
		texture->AutoBarrier(cb, TextureUsage::CopyDst);
		
		VkImageSubresourceRange subresourceRange;
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = mipLevel;
		subresourceRange.levelCount = 1;
		subresourceRange.baseArrayLayer = 0;
		subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		
		vkCmdClearColorImage(cb, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		                     reinterpret_cast<const VkClearColorValue*>(color), 1, &subresourceRange);
	}
	
	void ResolveTexture(CommandContextHandle cc, TextureHandle srcHandle, TextureHandle dstHandle, const ResolveRegion& region)
	{
		Texture* src = UnwrapTexture(srcHandle);
		Texture* dst = UnwrapTexture(dstHandle);
		
		RefResource(cc, *src);
		RefResource(cc, *dst);
		
		src->AutoBarrier(GetCB(cc), TextureUsage::CopySrc);
		dst->AutoBarrier(GetCB(cc), TextureUsage::CopyDst);
		
		VkImageResolve resolve = { };
		resolve.srcOffset.x = region.srcOffset.x;
		resolve.srcOffset.y = region.srcOffset.y;
		resolve.dstOffset.x = region.dstOffset.x;
		resolve.dstOffset.y = region.dstOffset.y;
		resolve.extent.width = region.width;
		resolve.extent.height = region.height;
		resolve.extent.depth = 1;
		resolve.srcSubresource.aspectMask = src->aspectFlags;
		resolve.srcSubresource.mipLevel = region.srcSubresource.mipLevel;
		resolve.srcSubresource.baseArrayLayer = region.srcSubresource.firstArrayLayer;
		resolve.srcSubresource.layerCount = region.srcSubresource.numArrayLayers;
		resolve.dstSubresource.aspectMask = dst->aspectFlags;
		resolve.dstSubresource.mipLevel = region.dstSubresource.mipLevel;
		resolve.dstSubresource.baseArrayLayer = region.dstSubresource.firstArrayLayer;
		resolve.dstSubresource.layerCount = region.dstSubresource.numArrayLayers;
		
		vkCmdResolveImage(GetCB(cc), src->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &resolve);
	}
	
	void TextureUsageHint(TextureHandle handle, TextureUsage newUsage, ShaderAccessFlags shaderAccessFlags)
	{
		Texture* texture = UnwrapTexture(handle);
		RefResource(nullptr, *texture);
		texture->AutoBarrier(GetCB(nullptr), newUsage, shaderAccessFlags);
	}
	
	void GenerateMipmaps(CommandContextHandle cc, TextureHandle handle)
	{
		Texture* texture = UnwrapTexture(handle);
		RefResource(cc, *texture);
		
		VkCommandBuffer cb = GetCB(cc);
		
		texture->AutoBarrier(cb, TextureUsage::CopyDst);
		
		VkImageMemoryBarrier preBlitBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		preBlitBarrier.image = texture->image;
		preBlitBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		preBlitBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		preBlitBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		preBlitBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		preBlitBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		preBlitBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		preBlitBarrier.subresourceRange = { texture->aspectFlags, 0, 1, 0, texture->numArrayLayers };
		
		int srcWidth = texture->extent.width;
		int srcHeight = texture->extent.height;
		for (uint32_t i = 1; i < texture->numMipLevels; i++)
		{
			preBlitBarrier.subresourceRange.baseMipLevel = i - 1;
			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &preBlitBarrier);
			
			int dstWidth = std::max(srcWidth / 2, 1);
			int dstHeight = std::max(srcHeight / 2, 1);
			
			VkImageBlit blit;
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { srcWidth, srcHeight, 1 };
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { dstWidth, dstHeight, 1 };
			blit.srcSubresource = { texture->aspectFlags, i - 1, 0, texture->numArrayLayers };
			blit.dstSubresource = { texture->aspectFlags, i    , 0, texture->numArrayLayers };
			
			vkCmdBlitImage(cb, texture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
			
			srcWidth = dstWidth;
			srcHeight = dstHeight;
		}
		
		preBlitBarrier.subresourceRange.baseMipLevel = texture->numMipLevels - 1;
		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &preBlitBarrier);
		
		texture->currentUsage = TextureUsage::CopySrc;
	}
	
	void BindTexture(CommandContextHandle cc, TextureHandle textureHandle, SamplerHandle samplerHandle,
		uint32_t set, uint32_t binding, const TextureSubresource& subresource)
	{
		Texture* texture = UnwrapTexture(textureHandle);
		RefResource(cc, *texture);
		
		if (texture->autoBarrier && texture->currentUsage != TextureUsage::ShaderSample)
		{
			EG_PANIC("Texture passed to BindTexture not in the correct usage state, did you forget to call UsageHint?");
		}
		
		VkSampler sampler = reinterpret_cast<VkSampler>(samplerHandle);
		if (sampler == VK_NULL_HANDLE)
		{
			if (texture->defaultSampler == VK_NULL_HANDLE)
			{
				EG_PANIC("Attempted to bind texture with no sampler specified.")
			}
			sampler = texture->defaultSampler;
		}
		
		AbstractPipeline* pipeline = GetCtxState(cc).pipeline;
		
		VkDescriptorImageInfo imageInfo;
		imageInfo.imageView = texture->GetView(subresource, texture->aspectFlags & (~VK_IMAGE_ASPECT_STENCIL_BIT));
		imageInfo.sampler = sampler;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		
		VkWriteDescriptorSet writeDS = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		writeDS.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeDS.dstBinding = binding;
		writeDS.dstSet = 0;
		writeDS.descriptorCount = 1;
		writeDS.pImageInfo = &imageInfo;
		
		vkCmdPushDescriptorSetKHR(GetCB(cc), pipeline->bindPoint, pipeline->pipelineLayout, set, 1, &writeDS);
	}
	
	void BindStorageImage(CommandContextHandle cc, TextureHandle textureHandle, uint32_t set, uint32_t binding,
		const TextureSubresourceLayers& subresource)
	{
		Texture* texture = UnwrapTexture(textureHandle);
		RefResource(cc, *texture);
		
		if (texture->autoBarrier && texture->currentUsage != TextureUsage::ILSRead &&
		    texture->currentUsage != TextureUsage::ILSWrite && texture->currentUsage != TextureUsage::ILSReadWrite)
		{
			EG_PANIC("Texture passed to BindStorageImage not in the correct usage state, did you forget to call UsageHint?");
		}
		
		AbstractPipeline* pipeline = GetCtxState(cc).pipeline;
		
		VkDescriptorImageInfo imageInfo;
		imageInfo.imageView = texture->GetView(subresource.AsSubresource());
		imageInfo.sampler = VK_NULL_HANDLE;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		
		VkWriteDescriptorSet writeDS = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		writeDS.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writeDS.dstBinding = binding;
		writeDS.dstSet = 0;
		writeDS.descriptorCount = 1;
		writeDS.pImageInfo = &imageInfo;
		
		vkCmdPushDescriptorSetKHR(GetCB(cc), pipeline->bindPoint, pipeline->pipelineLayout, set, 1, &writeDS);
	}
}

#endif
