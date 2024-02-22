#ifndef EG_NO_VULKAN
#include "CachedDescriptorSetLayout.hpp"
#include "../../Assert.hpp"
#include "../../Hash.hpp"

#include "Translation.hpp"
#include <algorithm>
#include <memory>
#include <unordered_map>

namespace eg::graphics_api::vk
{
struct DSLKey
{
	BindMode bindMode;
	std::span<const DescriptorSetBinding> bindings;
	std::unique_ptr<DescriptorSetBinding[]> ownedBindings;

	bool operator==(const DSLKey& o) const
	{
		return bindMode == o.bindMode &&
		       std::equal(bindings.begin(), bindings.end(), o.bindings.begin(), o.bindings.end());
	}

	size_t Hash() const
	{
		size_t h = static_cast<size_t>(bindMode) | (bindings.size() << 1);
		for (const DescriptorSetBinding& binding : bindings)
		{
			HashAppend(h, binding.Hash());
		}
		return h;
	}

	void CreateOwnedCopyOfBindings()
	{
		ownedBindings = std::make_unique<DescriptorSetBinding[]>(bindings.size());
		std::copy(bindings.begin(), bindings.end(), ownedBindings.get());
		bindings = { ownedBindings.get(), bindings.size() };
	}
};

static std::unordered_map<DSLKey, CachedDescriptorSetLayout, MemberFunctionHash<DSLKey>> cachedSetLayouts;

CachedDescriptorSetLayout& CachedDescriptorSetLayout::FindOrCreateNew(
	std::span<const DescriptorSetBinding> bindings, BindMode bindMode)
{
	DSLKey dslKey;
	dslKey.bindings = bindings;
	dslKey.bindMode = bindMode;

	if (!std::is_sorted(bindings.begin(), bindings.end(), DescriptorSetBinding::BindingCmp()))
	{
		dslKey.CreateOwnedCopyOfBindings();
		std::sort(
			dslKey.ownedBindings.get(), dslKey.ownedBindings.get() + dslKey.bindings.size(),
			DescriptorSetBinding::BindingCmp());
		bindings = dslKey.bindings;
	}

	// Searches for a matching descriptor set in the cache
	auto cacheIt = cachedSetLayouts.find(dslKey);
	if (cacheIt != cachedSetLayouts.end())
		return cacheIt->second;

	std::vector<VkDescriptorSetLayoutBinding> vkBindings(bindings.size());

	std::vector<VkDescriptorPoolSize> poolSizes;
	uint32_t maxBinding = 0;
	for (size_t i = 0; i < bindings.size(); i++)
	{
		VkDescriptorType descriptorType = TranslateBindingType(bindings[i].type);

		vkBindings[i] = VkDescriptorSetLayoutBinding{
			.binding = bindings[i].binding,
			.descriptorType = descriptorType,
			.descriptorCount = bindings[i].count,
			.stageFlags = TranslateShaderStageFlags(bindings[i].shaderAccess),
		};

		maxBinding = std::max(maxBinding, bindings[i].binding);

		bool found = false;
		for (VkDescriptorPoolSize& poolSize : poolSizes)
		{
			if (poolSize.type == descriptorType)
			{
				poolSize.descriptorCount += bindings[i].count;
				found = true;
				break;
			}
		}
		if (!found)
		{
			poolSizes.push_back({ descriptorType, bindings[i].count });
		}
	}

	VkDescriptorSetLayoutCreateInfo createInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = UnsignedNarrow<uint32_t>(vkBindings.size()),
		.pBindings = vkBindings.data(),
	};

	if (bindMode == BindMode::Dynamic)
	{
		createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	}

	CachedDescriptorSetLayout dsl;
	dsl.m_bindMode = bindMode;
	dsl.m_maxBinding = maxBinding;
	dsl.m_sizes = std::move(poolSizes);

	CheckRes(vkCreateDescriptorSetLayout(ctx.device, &createInfo, nullptr, &dsl.m_layout));

	dslKey.ownedBindings = std::make_unique<DescriptorSetBinding[]>(bindings.size());
	std::copy(bindings.begin(), bindings.end(), dslKey.ownedBindings.get());
	dslKey.bindings = { dslKey.ownedBindings.get(), bindings.size() };

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

	// Attempts to allocate from an existing pool
	for (VkDescriptorPool pool : m_pools)
	{
		allocateInfo.descriptorPool = pool;
		VkDescriptorSet set;
		VkResult allocResult = vkAllocateDescriptorSets(ctx.device, &allocateInfo, &set);
		if (allocResult != VK_ERROR_OUT_OF_POOL_MEMORY)
		{
			CheckRes(allocResult);
			return { set, pool };
		}
	}

	constexpr uint32_t SETS_PER_POOL = 64;

	VkDescriptorPoolCreateInfo poolCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	poolCreateInfo.flags =
		VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	poolCreateInfo.maxSets = SETS_PER_POOL;
	poolCreateInfo.poolSizeCount = UnsignedNarrow<uint32_t>(m_sizes.size());
	poolCreateInfo.pPoolSizes = m_sizes.data();

	VkDescriptorPool pool;
	CheckRes(vkCreateDescriptorPool(ctx.device, &poolCreateInfo, nullptr, &pool));
	m_pools.push_back(pool);

	allocateInfo.descriptorPool = pool;
	VkDescriptorSet set;
	CheckRes(vkAllocateDescriptorSets(ctx.device, &allocateInfo, &set));

	return { set, pool };
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
} // namespace eg::graphics_api::vk

#endif
