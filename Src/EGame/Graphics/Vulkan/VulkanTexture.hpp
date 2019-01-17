#pragma once

#include "Common.hpp"

namespace eg::graphics_api::vk
{
	struct Texture : Resource
	{
		VkImage image;
		VmaAllocation allocation;
		VkImageView imageView;
		VkImageViewType viewType;
		VkExtent3D extent;
		uint32_t numMipLevels;
		VkImageAspectFlags aspectFlags;
		VkSampler defaultSampler;
		bool autoBarrier;
		
		VkPipelineStageFlags currentStageFlags;
		TextureUsage currentUsage;
		
		void AutoBarrier(VkCommandBuffer cb, TextureUsage newUsage,
			ShaderAccessFlags shaderAccessFlags = ShaderAccessFlags::None);
		
		void Free() override;
	};
	
	inline Texture* UnwrapTexture(TextureHandle handle)
	{
		return reinterpret_cast<Texture*>(handle);
	}
}
