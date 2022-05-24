#ifndef EG_NO_VULKAN
#include "CachedDescriptorSetLayout.hpp"
#include "../../Hash.hpp"
#include "../../Assert.hpp"

#include <algorithm>
#include <unordered_map>

namespace eg::graphics_api::vk
{
	struct DSLKey
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings;
		BindMode bindMode;
		
		bool operator==(const DSLKey& o) const
		{
			return bindMode == o.bindMode && std::equal(bindings.begin(), bindings.end(), o.bindings.begin(), o.bindings.end(),
				[] (const VkDescriptorSetLayoutBinding& a, const VkDescriptorSetLayoutBinding& b)
			{
				return
					a.binding == b.binding &&
					a.stageFlags == b.stageFlags &&
					a.descriptorType == b.descriptorType &&
					a.descriptorCount == b.descriptorCount;
			});
		}
		
		size_t Hash() const
		{
			size_t h = (size_t)bindMode | (bindings.size() << 1);
			for (const VkDescriptorSetLayoutBinding& binding : bindings)
			{
				HashAppend(h, binding.binding);
				HashAppend(h, binding.stageFlags);
				HashAppend(h, binding.descriptorType);
				HashAppend(h, binding.descriptorCount);
			}
			return h;
		}
	};
	
	static std::unordered_map<DSLKey, CachedDescriptorSetLayout, MemberFunctionHash<DSLKey>> cachedSetLayouts;
	
	CachedDescriptorSetLayout& CachedDescriptorSetLayout::FindOrCreateNew(
		std::vector<VkDescriptorSetLayoutBinding> _bindings, BindMode bindMode)
	{
		DSLKey dslKey;
		dslKey.bindings = std::move(_bindings);
		dslKey.bindMode = bindMode;
		
		std::sort(dslKey.bindings.begin(), dslKey.bindings.end(),
			[&] (const VkDescriptorSetLayoutBinding& a, const VkDescriptorSetLayoutBinding& b)
		{
			return a.binding < b.binding;
		});
		
		//Searches for a matching descriptor set in the cache
		auto cacheIt = cachedSetLayouts.find(dslKey);
		if (cacheIt != cachedSetLayouts.end())
			return cacheIt->second;
		
		std::vector<VkDescriptorPoolSize> poolSizes;
		uint32_t maxBinding = 0;
		for (const VkDescriptorSetLayoutBinding& binding : dslKey.bindings)
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
		createInfo.bindingCount = (uint32_t)dslKey.bindings.size();
		createInfo.pBindings = dslKey.bindings.data();
		
		if (bindMode == BindMode::Dynamic)
		{
			createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
		}
		
		CachedDescriptorSetLayout dsl;
		dsl.m_bindMode = bindMode;
		dsl.m_maxBinding = maxBinding;
		dsl.m_sizes = std::move(poolSizes);
		
		CheckRes(vkCreateDescriptorSetLayout(ctx.device, &createInfo, nullptr, &dsl.m_layout));
		
		return cachedSetLayouts.emplace(std::move(dslKey), std::move(dsl)).first->second;
	}
	
	std::tuple<VkDescriptorSet, VkDescriptorPool> CachedDescriptorSetLayout::AllocateDescriptorSet()
	{
		if (m_bindMode != eg::BindMode::DescriptorSet)
		{
			EG_PANIC("Attempted to create a descriptor set for a set with dynamic bind mode.");
		}
		
		VkDescriptorSetAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		allocateInfo.descriptorSetCount = 1;
		allocateInfo.pSetLayouts = &m_layout;
		
		//Attempts to allocate from an existing pool
		for (VkDescriptorPool pool : m_pools)
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
		poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
		poolCreateInfo.maxSets = SETS_PER_POOL;
		poolCreateInfo.poolSizeCount = (uint32_t)m_sizes.size();
		poolCreateInfo.pPoolSizes = m_sizes.data();
		
		VkDescriptorPool pool;
		CheckRes(vkCreateDescriptorPool(ctx.device, &poolCreateInfo, nullptr, &pool));
		m_pools.push_back(pool);
		
		allocateInfo.descriptorPool = pool;
		VkDescriptorSet set;
		CheckRes(vkAllocateDescriptorSets(ctx.device, &allocateInfo, &set));
		
		return {set, pool};
	}
	
	void CachedDescriptorSetLayout::DestroyCached()
	{
		for (const auto& [dslKey, setLayout] : cachedSetLayouts)
		{
			for (VkDescriptorPool pool : setLayout.m_pools)
			{
				vkDestroyDescriptorPool(ctx.device, pool, nullptr);
			}
			vkDestroyDescriptorSetLayout(ctx.device, setLayout.m_layout, nullptr);
		}
		cachedSetLayouts.clear();
	}
	
	bool CachedDescriptorSetLayout::IsCacheEmpty()
	{
		return cachedSetLayouts.empty();
	}
}

#endif
