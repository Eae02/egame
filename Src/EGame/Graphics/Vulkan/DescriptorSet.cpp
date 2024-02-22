#ifndef EG_NO_VULKAN
#include "../../Alloc/ObjectPool.hpp"
#include "../../Assert.hpp"
#include "Buffer.hpp"
#include "CachedDescriptorSetLayout.hpp"
#include "Common.hpp"
#include "Pipeline.hpp"
#include "Texture.hpp"
#include "Translation.hpp"
#include "VulkanCommandContext.hpp"

namespace eg::graphics_api::vk
{
struct DescriptorSet : Resource
{
	VkDescriptorSet descriptorSet;
	VkDescriptorPool pool;
	std::vector<Resource*> resources;

	void AssignResource(uint32_t binding, Resource* resource)
	{
		if (binding >= resources.size())
			EG_PANIC("Descriptor set binding out of range");
		resource->refCount++;
		if (resources[binding] != nullptr)
			resources[binding]->UnRef();
		resources[binding] = resource;
	}

	void Free() override;
};

ConcurrentObjectPool<DescriptorSet> descriptorSets;

inline DescriptorSet* UnwrapDescriptorSet(DescriptorSetHandle handle)
{
	return reinterpret_cast<DescriptorSet*>(handle);
}

inline DescriptorSetHandle WrapDescriptorSet(DescriptorSet* set)
{
	return reinterpret_cast<DescriptorSetHandle>(set);
}

void DescriptorSet::Free()
{
	for (Resource* res : resources)
	{
		if (res != nullptr)
			res->UnRef();
	}
	if (!CachedDescriptorSetLayout::IsCacheEmpty())
	{
		CheckRes(vkFreeDescriptorSets(ctx.device, pool, 1, &descriptorSet));
	}
	descriptorSets.Delete(this);
}

DescriptorSetHandle CreateDescriptorSetP(PipelineHandle pipelineHandle, uint32_t set)
{
	AbstractPipeline* pipeline = UnwrapPipeline(pipelineHandle);
	auto [descriptorSet, pool] = pipeline->setLayouts[set]->AllocateDescriptorSet();

	DescriptorSet* ds = descriptorSets.New();
	ds->descriptorSet = descriptorSet;
	ds->pool = pool;
	ds->refCount = 1;
	ds->resources.resize(pipeline->setLayouts[set]->MaxBinding() + 1, nullptr);

	return WrapDescriptorSet(ds);
}

DescriptorSetHandle CreateDescriptorSetB(std::span<const DescriptorSetBinding> bindings)
{
	std::vector<VkDescriptorSetLayoutBinding> vkBindings(bindings.size());
	for (size_t i = 0; i < bindings.size(); i++)
	{
		vkBindings[i].binding = bindings[i].binding;
		vkBindings[i].descriptorType = TranslateBindingType(bindings[i].type);
		vkBindings[i].descriptorCount = bindings[i].count;
		vkBindings[i].stageFlags = TranslateShaderStage(bindings[i].shaderAccess);
		vkBindings[i].pImmutableSamplers = nullptr;
	}

	CachedDescriptorSetLayout& dsl =
		CachedDescriptorSetLayout::FindOrCreateNew(std::move(vkBindings), BindMode::DescriptorSet);

	auto [descriptorSet, pool] = dsl.AllocateDescriptorSet();

	DescriptorSet* ds = descriptorSets.New();
	ds->descriptorSet = descriptorSet;
	ds->pool = pool;
	ds->refCount = 1;
	ds->resources.resize(dsl.MaxBinding() + 1, nullptr);

	return WrapDescriptorSet(ds);
}

void DestroyDescriptorSet(DescriptorSetHandle set)
{
	UnwrapDescriptorSet(set)->UnRef();
}

void BindTextureDS(
	TextureViewHandle textureViewHandle, SamplerHandle samplerHandle, DescriptorSetHandle setHandle, uint32_t binding)
{
	DescriptorSet* ds = UnwrapDescriptorSet(setHandle);
	TextureView* view = UnwrapTextureView(textureViewHandle);

	ds->AssignResource(binding, view->texture);

	VkSampler sampler = reinterpret_cast<VkSampler>(samplerHandle);
	EG_ASSERT(sampler != VK_NULL_HANDLE);

	VkDescriptorImageInfo imageInfo = {
		.sampler = sampler,
		.imageView = view->view,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	VkWriteDescriptorSet writeDS = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = ds->descriptorSet,
		.dstBinding = binding,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = &imageInfo,
	};

	vkUpdateDescriptorSets(ctx.device, 1, &writeDS, 0, nullptr);
}

void BindStorageImageDS(TextureViewHandle textureViewHandle, DescriptorSetHandle setHandle, uint32_t binding)
{
	DescriptorSet* ds = UnwrapDescriptorSet(setHandle);
	TextureView* view = UnwrapTextureView(textureViewHandle);

	ds->AssignResource(binding, view->texture);

	VkDescriptorImageInfo imageInfo = {
		.imageView = view->view,
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
	};

	VkWriteDescriptorSet writeDS = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = ds->descriptorSet,
		.dstBinding = binding,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		.pImageInfo = &imageInfo,
	};

	vkUpdateDescriptorSets(ctx.device, 1, &writeDS, 0, nullptr);
}

void BindUniformBufferDS(
	BufferHandle bufferHandle, DescriptorSetHandle setHandle, uint32_t binding, uint64_t offset, uint64_t range)
{
	DescriptorSet* ds = UnwrapDescriptorSet(setHandle);
	Buffer* buffer = UnwrapBuffer(bufferHandle);

	ds->AssignResource(binding, buffer);

	VkDescriptorBufferInfo bufferInfo;
	bufferInfo.buffer = buffer->buffer;
	bufferInfo.offset = offset;
	bufferInfo.range = range;

	VkWriteDescriptorSet writeDS = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	writeDS.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeDS.descriptorCount = 1;
	writeDS.dstSet = ds->descriptorSet;
	writeDS.dstBinding = binding;
	writeDS.pBufferInfo = &bufferInfo;

	vkUpdateDescriptorSets(ctx.device, 1, &writeDS, 0, nullptr);
}

void BindStorageBufferDS(
	BufferHandle bufferHandle, DescriptorSetHandle setHandle, uint32_t binding, uint64_t offset, uint64_t range)
{
	DescriptorSet* ds = UnwrapDescriptorSet(setHandle);
	Buffer* buffer = UnwrapBuffer(bufferHandle);

	ds->AssignResource(binding, buffer);

	VkDescriptorBufferInfo bufferInfo;
	bufferInfo.buffer = buffer->buffer;
	bufferInfo.offset = offset;
	bufferInfo.range = range;

	VkWriteDescriptorSet writeDS = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	writeDS.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writeDS.descriptorCount = 1;
	writeDS.dstSet = ds->descriptorSet;
	writeDS.dstBinding = binding;
	writeDS.pBufferInfo = &bufferInfo;

	vkUpdateDescriptorSets(ctx.device, 1, &writeDS, 0, nullptr);
}

void BindDescriptorSet(CommandContextHandle cc, uint32_t set, DescriptorSetHandle handle)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	DescriptorSet* ds = UnwrapDescriptorSet(handle);
	vcc.referencedResources.Add(*ds);
	vkCmdBindDescriptorSets(
		vcc.cb, vcc.pipeline->bindPoint, vcc.pipeline->pipelineLayout, set, 1, &ds->descriptorSet, 0, nullptr);
}
} // namespace eg::graphics_api::vk

#endif
