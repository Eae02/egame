#pragma once

#ifndef EG_NO_VULKAN

#include "Common.hpp"

namespace eg::graphics_api::vk
{
	class CachedDescriptorSetLayout
	{
	public:
		static CachedDescriptorSetLayout& FindOrCreateNew(
			std::vector<VkDescriptorSetLayoutBinding> bindings, BindMode bindMode);
		
		static void DestroyCached();
		static bool IsCacheEmpty();
		
		std::tuple<VkDescriptorSet, VkDescriptorPool> AllocateDescriptorSet();
		
		VkDescriptorSetLayout Layout() const { return m_layout; }
		uint32_t MaxBinding() const { return m_maxBinding; }
		
	private:
		CachedDescriptorSetLayout() = default;
		
		VkDescriptorSetLayout m_layout;
		BindMode m_bindMode;
		uint32_t m_maxBinding;
		
		std::vector<VkDescriptorPoolSize> m_sizes;
		std::vector<VkDescriptorPool> m_pools;
	};
	
	uint32_t CalculateMaxBindingIndex(std::span<const VkDescriptorSetLayoutBinding>& bindings);
}

#endif
