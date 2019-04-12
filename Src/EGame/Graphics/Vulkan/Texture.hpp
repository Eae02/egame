#pragma once

#include "Common.hpp"

namespace eg::graphics_api::vk
{
	VkImageLayout ImageLayoutFromUsage(TextureUsage usage, VkImageAspectFlags aspectFlags);
	
	struct TextureView
	{
		VkImageView view;
		TextureSubresource subresource;
	};
	
	struct Texture : Resource
	{
		VkImage image;
		VmaAllocation allocation;
		std::vector<TextureView> views;
		VkImageViewType viewType;
		VkExtent3D extent;
		uint32_t numMipLevels;
		uint32_t numArrayLayers;
		VkFormat format;
		VkImageAspectFlags aspectFlags;
		VkComponentMapping componentMapping;
		VkSampler defaultSampler;
		bool autoBarrier;
		std::string viewLabel;
		
		VkPipelineStageFlags currentStageFlags;
		TextureUsage currentUsage;
		
		VkImageLayout CurrentLayout() const
		{
			return ImageLayoutFromUsage(currentUsage, aspectFlags);
		}
		
		void AutoBarrier(VkCommandBuffer cb, TextureUsage newUsage,
			ShaderAccessFlags shaderAccessFlags = ShaderAccessFlags::None);
		
		VkImageView GetView(const TextureSubresource& subresource);
		
		void Free() override;
	};
	
	inline Texture* UnwrapTexture(TextureHandle handle)
	{
		return reinterpret_cast<Texture*>(handle);
	}
}
