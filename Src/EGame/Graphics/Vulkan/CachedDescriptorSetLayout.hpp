#pragma once

#ifndef EG_NO_VULKAN

#include "../Abstraction.hpp"
#include "../DescriptorSetLayoutCache.hpp"
#include "Common.hpp"

namespace eg::graphics_api::vk
{
class CachedDescriptorSetLayout : public ICachedDescriptorSetLayout
{
public:
	CachedDescriptorSetLayout(std::span<const DescriptorSetBinding> bindings, bool dynamicBind);

	~CachedDescriptorSetLayout();

	CachedDescriptorSetLayout(CachedDescriptorSetLayout&&) = delete;
	CachedDescriptorSetLayout(const CachedDescriptorSetLayout&) = delete;
	CachedDescriptorSetLayout& operator=(CachedDescriptorSetLayout&&) = delete;
	CachedDescriptorSetLayout& operator=(const CachedDescriptorSetLayout&) = delete;

	static CachedDescriptorSetLayout& FindOrCreateNew(std::span<const DescriptorSetBinding> bindings, bool dynamicBind);

	static void DestroyCached() { descriptorSetLayoutCache.Clear(); }
	static bool IsCacheEmpty() { return descriptorSetLayoutCache.IsEmpty(); }

	std::tuple<VkDescriptorSet, VkDescriptorPool> AllocateDescriptorSet();

	VkDescriptorSetLayout Layout() const { return m_layout; }
	uint32_t MaxBinding() const { return m_maxBinding; }

private:
	static DescriptorSetLayoutCache descriptorSetLayoutCache;

	VkDescriptorSetLayout m_layout;
	bool m_dynamicBind;
	uint32_t m_maxBinding;

	std::vector<uint32_t> m_bindingsWithDynamicOffset;

	std::vector<VkDescriptorPoolSize> m_sizes;
	std::vector<VkDescriptorPool> m_pools;
};

// This function removes all information from binding that is not needed by the vulkan backend so that we won't
// unneccessarily create multiple descriptor set layouts
void NormalizeBinding(DescriptorSetBinding& binding);
} // namespace eg::graphics_api::vk

#endif
