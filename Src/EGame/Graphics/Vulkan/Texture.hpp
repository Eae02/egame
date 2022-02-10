#pragma once

#ifndef EG_NO_VULKAN

#include "Common.hpp"
#include "../../Hash.hpp"

#include <optional>
#include <unordered_map>

namespace eg::graphics_api::vk
{
	VkImageLayout ImageLayoutFromUsage(TextureUsage usage, VkImageAspectFlags aspectFlags);
	
	VkPipelineStageFlags GetBarrierStageFlagsFromUsage(TextureUsage usage, ShaderAccessFlags shaderAccessFlags);
	
	struct TextureViewKey
	{
		VkImageAspectFlags aspectFlags;
		VkImageViewType type;
		VkFormat format;
		TextureSubresource subresource;
		
		size_t Hash() const;
		bool operator==(const TextureViewKey& other) const;
	};
	
	struct TextureView
	{
		VkImageView view;
		struct Texture* texture;
	};
	
	struct Texture : Resource
	{
		VkImage image;
		VmaAllocation allocation;
		std::unordered_map<TextureViewKey, TextureView, MemberFunctionHash<TextureViewKey>> views;
		VkImageViewType viewType;
		VkExtent3D extent;
		uint32_t numMipLevels;
		uint32_t numArrayLayers;
		uint32_t sampleCount;
		VkFormat format;
		VkImageAspectFlags aspectFlags;
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
		
		TextureView& GetView(const TextureSubresource& subresource, VkImageAspectFlags aspectFlags = 0,
			std::optional<VkImageViewType> forcedViewType = {}, VkFormat differentFormat = VK_FORMAT_UNDEFINED);
		
		void Free() override;
	};
	
	inline Texture* UnwrapTexture(TextureHandle handle)
	{
		return reinterpret_cast<Texture*>(handle);
	}
	
	inline TextureView* UnwrapTextureView(TextureViewHandle handle)
	{
		return reinterpret_cast<TextureView*>(handle);
	}
}

#endif
