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
CachedDescriptorSetLayout::CachedDescriptorSetLayout(std::span<const DescriptorSetBinding> bindings, BindMode bindMode)
	: m_bindMode(bindMode)
{
	std::vector<VkDescriptorSetLayoutBinding> vkBindings(bindings.size());

	m_maxBinding = 0;
	for (size_t i = 0; i < bindings.size(); i++)
	{
		VkDescriptorType descriptorType = TranslateBindingType(bindings[i].type);

		vkBindings[i] = VkDescriptorSetLayoutBinding{
			.binding = bindings[i].binding,
			.descriptorType = descriptorType,
			.descriptorCount = 1,
			.stageFlags = TranslateShaderStageFlags(bindings[i].shaderAccess),
		};

		m_maxBinding = std::max(m_maxBinding, bindings[i].binding);

		bool found = false;
		for (VkDescriptorPoolSize& poolSize : m_sizes)
		{
			if (poolSize.type == descriptorType)
			{
				poolSize.descriptorCount++;
				found = true;
				break;
			}
		}
		if (!found)
		{
			m_sizes.push_back({ .type = descriptorType, .descriptorCount = 1 });
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

	CheckRes(vkCreateDescriptorSetLayout(ctx.device, &createInfo, nullptr, &m_layout));
}

CachedDescriptorSetLayout::~CachedDescriptorSetLayout()
{
	for (VkDescriptorPool pool : m_pools)
	{
		vkDestroyDescriptorPool(ctx.device, pool, nullptr);
	}
	vkDestroyDescriptorSetLayout(ctx.device, m_layout, nullptr);
}

DescriptorSetLayoutCache CachedDescriptorSetLayout::descriptorSetLayoutCache{
	&DescriptorSetLayoutCache::ConstructorCreateLayoutCallback<CachedDescriptorSetLayout>
};

CachedDescriptorSetLayout& CachedDescriptorSetLayout::FindOrCreateNew(
	std::span<const DescriptorSetBinding> bindings, BindMode bindMode)
{
	return static_cast<CachedDescriptorSetLayout&>(descriptorSetLayoutCache.Get(bindings, bindMode));
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
} // namespace eg::graphics_api::vk

#endif
