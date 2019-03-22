#pragma once

#include "Common.hpp"

namespace eg::graphics_api::vk
{
	struct CachedDSL
	{
		VkDescriptorSetLayout layout;
		BindMode bindMode;
		uint32_t maxBinding;
	};
	
	size_t GetCachedDSLIndex(std::vector<VkDescriptorSetLayoutBinding> bindings, BindMode bindMode);
	const CachedDSL& GetDSLFromCache(size_t setLayoutIndex);
	std::tuple<VkDescriptorSet, VkDescriptorPool> AllocateDescriptorSet(size_t setLayoutIndex);
	void DestroyCachedDescriptorSets();
	bool IsDSLCacheEmpty();
}
