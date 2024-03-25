#include "CachedDescriptorSetLayout.hpp"
#include "../../Assert.hpp"

#include "Translation.hpp"
#include <algorithm>

namespace eg::graphics_api::vk
{
VkDescriptorType GetBindingType(const DescriptorSetBinding& binding)
{
	if (const auto* uniformBuffer = std::get_if<BindingTypeUniformBuffer>(&binding.type))
	{
		if (uniformBuffer->dynamicOffset)
			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	}

	if (const auto* storageBuffer = std::get_if<BindingTypeStorageBuffer>(&binding.type))
	{
		if (storageBuffer->dynamicOffset)
			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	}

	if (std::holds_alternative<BindingTypeTexture>(binding.type))
		return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	if (std::holds_alternative<BindingTypeStorageImage>(binding.type))
		return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	if (std::holds_alternative<BindingTypeSampler>(binding.type))
		return VK_DESCRIPTOR_TYPE_SAMPLER;

	EG_UNREACHABLE;
}

CachedDescriptorSetLayout::CachedDescriptorSetLayout(std::span<const DescriptorSetBinding> bindings, bool dynamicBind)
{
	std::vector<VkDescriptorSetLayoutBinding> vkBindings(bindings.size());

	m_maxBinding = 0;
	for (size_t i = 0; i < bindings.size(); i++)
	{
		VkDescriptorType descriptorType = GetBindingType(bindings[i]);

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

	m_dynamicBind = dynamicBind;
	if (dynamicBind)
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
	&DescriptorSetLayoutCache::ConstructorCreateLayoutCallback<CachedDescriptorSetLayout>,
};

CachedDescriptorSetLayout& CachedDescriptorSetLayout::FindOrCreateNew(
	std::span<const DescriptorSetBinding> bindings, bool dynamicBind)
{
	DescriptorSetBinding* bindingsNormalized = static_cast<DescriptorSetBinding*>(alloca(bindings.size_bytes()));
	std::memcpy(bindingsNormalized, bindings.data(), bindings.size_bytes());
	for (size_t i = 0; i < bindings.size(); i++)
		NormalizeBinding(bindingsNormalized[i]);

	std::sort(bindingsNormalized, bindingsNormalized + bindings.size(), DescriptorSetBinding::BindingCmp());

	return static_cast<CachedDescriptorSetLayout&>(
		descriptorSetLayoutCache.Get({ bindingsNormalized, bindings.size() }, dynamicBind));
}

void NormalizeBinding(DescriptorSetBinding& binding)
{
	std::visit(
		[&]<typename T>(T& type)
		{
			// Discard all information except for dynamicOffset
			if constexpr (std::is_same_v<T, BindingTypeUniformBuffer> || std::is_same_v<T, BindingTypeStorageBuffer>)
				type = T{ .dynamicOffset = type.dynamicOffset };
			else
				type = T{};
		},
		binding.type);
}

std::tuple<VkDescriptorSet, VkDescriptorPool> CachedDescriptorSetLayout::AllocateDescriptorSet()
{
	if (m_dynamicBind)
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
