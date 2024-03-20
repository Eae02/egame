#pragma once

#ifndef EG_NO_VULKAN

#include "../DescriptorSetLayoutCache.hpp"
#include "Common.hpp"

namespace eg::graphics_api::vk
{
class CachedDescriptorSetLayout : public ICachedDescriptorSetLayout
{
public:
	CachedDescriptorSetLayout(std::span<const DescriptorSetBinding> bindings, BindMode bindMode);

	~CachedDescriptorSetLayout();

	CachedDescriptorSetLayout(CachedDescriptorSetLayout&&) = delete;
	CachedDescriptorSetLayout(const CachedDescriptorSetLayout&) = delete;
	CachedDescriptorSetLayout& operator=(CachedDescriptorSetLayout&&) = delete;
	CachedDescriptorSetLayout& operator=(const CachedDescriptorSetLayout&) = delete;

	static CachedDescriptorSetLayout& FindOrCreateNew(
		std::span<const DescriptorSetBinding> bindings, BindMode bindMode);

	static void DestroyCached() { descriptorSetLayoutCache.Clear(); }
	static bool IsCacheEmpty() { return descriptorSetLayoutCache.IsEmpty(); }

	std::tuple<VkDescriptorSet, VkDescriptorPool> AllocateDescriptorSet();

	VkDescriptorSetLayout Layout() const { return m_layout; }
	uint32_t MaxBinding() const { return m_maxBinding; }

private:
	static DescriptorSetLayoutCache descriptorSetLayoutCache;

	VkDescriptorSetLayout m_layout;
	BindMode m_bindMode;
	uint32_t m_maxBinding;

	std::vector<uint32_t> m_bindingsWithDynamicOffset;

	std::vector<VkDescriptorPoolSize> m_sizes;
	std::vector<VkDescriptorPool> m_pools;
};
} // namespace eg::graphics_api::vk

#endif
