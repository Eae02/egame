#include "VulkanTexture.hpp"
#include "VulkanBuffer.hpp"
#include "Sampler.hpp"
#include "../Graphics.hpp"
#include "../../Alloc/ObjectPool.hpp"

namespace eg::graphics_api::vk
{
	ConcurrentObjectPool<Texture> texturePool;
	
	void Texture::Free()
	{
		vkDestroyImageView(ctx.device, imageView, nullptr);
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
		texture.autoBarrier = !HasFlag(createInfo.flags, TextureFlags::ManualBarrier);
		texture.currentUsage = TextureUsage::Undefined;
		texture.currentStageFlags = 0;
		
		//Creates the image
		VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageCreateInfo.extent = extent;
		imageCreateInfo.format = TranslateFormat(createInfo.format);
		imageCreateInfo.imageType = imageType;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.mipLevels = createInfo.mipLevels;
		imageCreateInfo.arrayLayers = arrayLayers;
		
		if (HasFlag(createInfo.flags, TextureFlags::CopySrc) || HasFlag(createInfo.flags, TextureFlags::GenerateMipmaps))
			imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		if (HasFlag(createInfo.flags, TextureFlags::CopyDst) || HasFlag(createInfo.flags, TextureFlags::GenerateMipmaps))
			imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		if (HasFlag(createInfo.flags, TextureFlags::ShaderSample))
			imageCreateInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
		if (HasFlag(createInfo.flags, TextureFlags::FramebufferAttachment))
		{
			if (texture.aspectFlags == VK_IMAGE_ASPECT_COLOR_BIT)
				imageCreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			else
				imageCreateInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
		
		VmaAllocationCreateInfo allocationCreateInfo = { };
		allocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		CheckRes(vmaCreateImage(ctx.allocator, &imageCreateInfo, &allocationCreateInfo, &texture.image,
			&texture.allocation, nullptr));
		
		//Creates the image view
		VkImageViewCreateInfo viewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		viewCreateInfo.viewType = viewType;
		viewCreateInfo.image = texture.image;
		viewCreateInfo.format = imageCreateInfo.format;
		viewCreateInfo.subresourceRange = { texture.aspectFlags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
		CheckRes(vkCreateImageView(ctx.device, &viewCreateInfo, nullptr, &texture.imageView));
		
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
	
	TextureHandle CreateTexture2D(const Texture2DCreateInfo& createInfo)
	{
		Texture* texture = texturePool.New();
		
		InitializeImage(*texture, createInfo, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D,
			{ createInfo.width, createInfo.height, 1 }, 1);
		
		return reinterpret_cast<TextureHandle>(texture);
	}
	
	TextureHandle CreateTexture2DArray(const Texture2DArrayCreateInfo& createInfo)
	{
		Texture* texture = texturePool.New();
		
		InitializeImage(*texture, createInfo, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D_ARRAY,
			{ createInfo.width, createInfo.height, 1 }, createInfo.arrayLayers);
		
		return reinterpret_cast<TextureHandle>(texture);
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
		case TextureUsage::FramebufferAttachment:
			if (aspectFlags == VK_IMAGE_ASPECT_COLOR_BIT)
				return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			else
				return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}
		EG_UNREACHABLE
	}
	
	inline VkImageLayout GetBarrierLayout(TextureUsage usage, VkImageAspectFlags aspectFlags)
	{
		switch (usage)
		{
		case TextureUsage::Undefined: return VK_IMAGE_LAYOUT_UNDEFINED;
		case TextureUsage::CopySrc: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		case TextureUsage::CopyDst: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
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
		case TextureUsage::ShaderSample:
		{
			VkAccessFlags flags = 0;
			if (HasFlag(shaderAccessFlags, ShaderAccessFlags::Vertex))
				flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
			if (HasFlag(shaderAccessFlags, ShaderAccessFlags::Fragment))
				flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			return flags;
		}
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
		barrier.oldLayout = GetBarrierLayout(currentUsage, aspectFlags);
		barrier.newLayout = GetBarrierLayout(newUsage, aspectFlags);
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
		preBlitBarrier.subresourceRange = { texture->aspectFlags, 0, 1, 0, VK_REMAINING_ARRAY_LAYERS };
		
		int srcWidth = texture->extent.width;
		int srcHeight = texture->extent.height;
		for (uint32_t i = 1; i < texture->numMipLevels; i++)
		{
			preBlitBarrier.subresourceRange.baseMipLevel = i - 1;
			vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &preBlitBarrier);
			
			int dstWidth = srcWidth / 2;
			int dstHeight = srcHeight / 2;
			
			VkImageBlit blit;
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { srcWidth, srcHeight, 1 };
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { dstWidth, dstHeight, 1 };
			blit.srcSubresource = { texture->aspectFlags, i - 1, 0, VK_REMAINING_ARRAY_LAYERS };
			blit.dstSubresource = { texture->aspectFlags, i    , 0, VK_REMAINING_ARRAY_LAYERS };
			
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
}
