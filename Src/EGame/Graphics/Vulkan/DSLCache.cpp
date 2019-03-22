#include "DSLCache.hpp"

namespace eg::graphics_api::vk
{
	struct CachedDSLExt : CachedDSL
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings;
		std::vector<VkDescriptorPoolSize> sizes;
		std::vector<VkDescriptorPool> pools;
	};
	
	static std::vector<CachedDSLExt> cachedSetLayouts;
	
	size_t GetCachedDSLIndex(std::vector<VkDescriptorSetLayoutBinding> bindings, BindMode bindMode)
	{
		std::sort(bindings.begin(), bindings.end(),
			[&] (const VkDescriptorSetLayoutBinding& a, const VkDescriptorSetLayoutBinding& b)
		{
			return a.binding < b.binding;
		});
		
		//Searches for a matching descriptor set in the cache
		for (size_t i = 0; i < cachedSetLayouts.size(); i++)
		{
			bool eq = std::equal(cachedSetLayouts[i].bindings.begin(), cachedSetLayouts[i].bindings.end(),
				bindings.begin(), bindings.end(),
				[&] (const VkDescriptorSetLayoutBinding& a, const VkDescriptorSetLayoutBinding& b)
			{
				return a.binding == b.binding && a.stageFlags == b.stageFlags && a.descriptorType == b.descriptorType &&
				       a.descriptorCount == b.descriptorCount;
			});
			
			if (eq && cachedSetLayouts[i].bindMode == bindMode)
				return i;
		}
		
		std::vector<VkDescriptorPoolSize> poolSizes;
		uint32_t maxBinding = 0;
		for (const VkDescriptorSetLayoutBinding& binding : bindings)
		{
			maxBinding = std::max(maxBinding, binding.binding);
			
			bool found = false;
			for (VkDescriptorPoolSize& poolSize : poolSizes)
			{
				if (poolSize.type == binding.descriptorType)
				{
					poolSize.descriptorCount += binding.descriptorCount;
					found = true;
					break;
				}
			}
			if (!found)
			{
				poolSizes.push_back({ binding.descriptorType, binding.descriptorCount });
			}
		}
		
		VkDescriptorSetLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		createInfo.bindingCount = (uint32_t)bindings.size();
		createInfo.pBindings = bindings.data();
		
		if (bindMode == BindMode::Dynamic)
		{
			createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
		}
		
		CachedDSLExt& dsl = cachedSetLayouts.emplace_back();
		CheckRes(vkCreateDescriptorSetLayout(ctx.device, &createInfo, nullptr, &dsl.layout));
		dsl.bindMode = bindMode;
		dsl.maxBinding = maxBinding;
		dsl.bindings = std::move(bindings);
		dsl.sizes = std::move(poolSizes);
		
		return cachedSetLayouts.size() - 1;
	}
	
	const CachedDSL& GetDSLFromCache(size_t setLayoutIndex)
	{
		return cachedSetLayouts[setLayoutIndex];
	}
	
	std::tuple<VkDescriptorSet, VkDescriptorPool> AllocateDescriptorSet(size_t setLayoutIndex)
	{
		if (cachedSetLayouts[setLayoutIndex].bindMode != eg::BindMode::DescriptorSet)
		{
			EG_PANIC("Attempted to create a descriptor set for a set with dynamic bind mode.");
		}
		
		VkDescriptorSetAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		allocateInfo.descriptorSetCount = 1;
		allocateInfo.pSetLayouts = &cachedSetLayouts[setLayoutIndex].layout;
		
		//Attempts to allocate from an existing pool
		for (VkDescriptorPool pool : cachedSetLayouts[setLayoutIndex].pools)
		{
			allocateInfo.descriptorPool = pool;
			VkDescriptorSet set;
			VkResult allocResult = vkAllocateDescriptorSets(ctx.device, &allocateInfo, &set);
			if (allocResult != VK_ERROR_OUT_OF_POOL_MEMORY)
			{
				CheckRes(allocResult);
				return {set, pool};
			}
		}
		
		constexpr uint32_t SETS_PER_POOL = 64;
		
		VkDescriptorPoolCreateInfo poolCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		poolCreateInfo.maxSets = SETS_PER_POOL;
		poolCreateInfo.poolSizeCount = (uint32_t)cachedSetLayouts[setLayoutIndex].sizes.size();
		poolCreateInfo.pPoolSizes = cachedSetLayouts[setLayoutIndex].sizes.data();
		
		VkDescriptorPool pool;
		CheckRes(vkCreateDescriptorPool(ctx.device, &poolCreateInfo, nullptr, &pool));
		cachedSetLayouts[setLayoutIndex].pools.push_back(pool);
		
		allocateInfo.descriptorPool = pool;
		VkDescriptorSet set;
		CheckRes(vkAllocateDescriptorSets(ctx.device, &allocateInfo, &set));
		
		return {set, pool};
	}
	
	void DestroyCachedDescriptorSets()
	{
		for (const CachedDSLExt& setLayout : cachedSetLayouts)
		{
			for (VkDescriptorPool pool : setLayout.pools)
			{
				vkDestroyDescriptorPool(ctx.device, pool, nullptr);
			}
			vkDestroyDescriptorSetLayout(ctx.device, setLayout.layout, nullptr);
		}
		cachedSetLayouts.clear();
	}
	
	bool IsDSLCacheEmpty()
	{
		return cachedSetLayouts.empty();
	}
}
