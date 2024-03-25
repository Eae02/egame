#include "../../Alloc/ObjectPool.hpp"
#include "../../Assert.hpp"
#include "Buffer.hpp"
#include "CachedDescriptorSetLayout.hpp"
#include "Common.hpp"
#include "Pipeline.hpp"
#include "Texture.hpp"
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

	EG_ASSERT(pipeline->dynamicDescriptorSetIndex != set);

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
	CachedDescriptorSetLayout& dsl = CachedDescriptorSetLayout::FindOrCreateNew(bindings, false);

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
	TextureViewHandle textureViewHandle, DescriptorSetHandle setHandle, uint32_t binding, eg::TextureUsage usage)
{
	DescriptorSet* ds = UnwrapDescriptorSet(setHandle);
	TextureView* view = UnwrapTextureView(textureViewHandle);

	ds->AssignResource(binding, view->texture);

	VkDescriptorImageInfo imageInfo = {
		.imageView = view->view,
		.imageLayout = ImageLayoutFromUsage(usage, view->texture->aspectFlags),
	};

	VkWriteDescriptorSet writeDS = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = ds->descriptorSet,
		.dstBinding = binding,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		.pImageInfo = &imageInfo,
	};

	vkUpdateDescriptorSets(ctx.device, 1, &writeDS, 0, nullptr);
}

void BindSamplerDS(SamplerHandle sampler, DescriptorSetHandle setHandle, uint32_t binding)
{
	DescriptorSet* ds = UnwrapDescriptorSet(setHandle);

	VkDescriptorImageInfo imageInfo = { .sampler = reinterpret_cast<VkSampler>(sampler) };

	VkWriteDescriptorSet writeDS = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = ds->descriptorSet,
		.dstBinding = binding,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
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

void BindBufferDS(
	BufferHandle bufferHandle, DescriptorSetHandle setHandle, uint32_t binding, uint64_t offset,
	std::optional<uint64_t> range, VkDescriptorType descriptorTypeIfNotDynamic)
{
	DescriptorSet* ds = UnwrapDescriptorSet(setHandle);
	Buffer* buffer = UnwrapBuffer(bufferHandle);

	ds->AssignResource(binding, buffer);

	bool isDynamic = offset == BIND_BUFFER_OFFSET_DYNAMIC;
	if (isDynamic)
		offset = 0;

	VkDescriptorBufferInfo bufferInfo;
	bufferInfo.buffer = buffer->buffer;
	bufferInfo.offset = offset;
	bufferInfo.range = range.value_or(VK_WHOLE_SIZE);

	VkWriteDescriptorSet writeDS = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	writeDS.descriptorType =
		static_cast<VkDescriptorType>(static_cast<int>(descriptorTypeIfNotDynamic) + 2 * isDynamic);
	writeDS.descriptorCount = 1;
	writeDS.dstSet = ds->descriptorSet;
	writeDS.dstBinding = binding;
	writeDS.pBufferInfo = &bufferInfo;

	vkUpdateDescriptorSets(ctx.device, 1, &writeDS, 0, nullptr);
}

void BindUniformBufferDS(
	BufferHandle bufferHandle, DescriptorSetHandle setHandle, uint32_t binding, uint64_t offset,
	std::optional<uint64_t> range)
{
	BindBufferDS(bufferHandle, setHandle, binding, offset, range, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

void BindStorageBufferDS(
	BufferHandle bufferHandle, DescriptorSetHandle setHandle, uint32_t binding, uint64_t offset,
	std::optional<uint64_t> range)
{
	BindBufferDS(bufferHandle, setHandle, binding, offset, range, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
}

void BindDescriptorSet(
	CommandContextHandle cc, uint32_t set, DescriptorSetHandle handle, std::span<const uint32_t> dynamicOffsets)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);

	EG_DEBUG_ASSERT(vcc.pipeline && vcc.pipeline->dynamicDescriptorSetIndex != set);

	DescriptorSet* ds = UnwrapDescriptorSet(handle);
	vcc.referencedResources.Add(*ds);
	vkCmdBindDescriptorSets(
		vcc.cb, vcc.pipeline->bindPoint, vcc.pipeline->pipelineLayout, set, 1, &ds->descriptorSet,
		static_cast<uint32_t>(dynamicOffsets.size()), dynamicOffsets.data());
}
} // namespace eg::graphics_api::vk
