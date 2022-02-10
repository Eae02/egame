#ifndef EG_NO_VULKAN
#include "Common.hpp"
#include "DSLCache.hpp"
#include "Pipeline.hpp"
#include "Texture.hpp"
#include "Buffer.hpp"
#include "Translation.hpp"
#include "../../Assert.hpp"
#include "../../Alloc/ObjectPool.hpp"

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
		if (!IsDSLCacheEmpty())
		{
			CheckRes(vkFreeDescriptorSets(ctx.device, pool, 1, &descriptorSet));
		}
		descriptorSets.Delete(this);
	}
	
	DescriptorSetHandle CreateDescriptorSetP(PipelineHandle pipelineHandle, uint32_t set)
	{
		AbstractPipeline* pipeline = UnwrapPipeline(pipelineHandle);
		auto [descriptorSet, pool] = AllocateDescriptorSet(pipeline->setsLayoutIndices[set]);
		
		DescriptorSet* ds = descriptorSets.New();
		ds->descriptorSet = descriptorSet;
		ds->pool = pool;
		ds->refCount = 1;
		ds->resources.resize(GetDSLFromCache(pipeline->setsLayoutIndices[set]).maxBinding + 1, nullptr);
		
		return WrapDescriptorSet(ds);
	}
	
	DescriptorSetHandle CreateDescriptorSetB(std::span<const DescriptorSetBinding> bindings)
	{
		std::vector<VkDescriptorSetLayoutBinding> vkBindings(bindings.size());
		uint32_t maxBinding = 0;
		for (size_t i = 0; i < bindings.size(); i++)
		{
			maxBinding = std::max(maxBinding, bindings[i].binding);
			vkBindings[i].binding = bindings[i].binding;
			vkBindings[i].descriptorType = TranslateBindingType(bindings[i].type);
			vkBindings[i].descriptorCount = bindings[i].count;
			vkBindings[i].stageFlags = TranslateShaderAccess(bindings[i].shaderAccess);
			vkBindings[i].pImmutableSamplers = nullptr;
		}
		
		size_t dslIndex = GetCachedDSLIndex(vkBindings, BindMode::DescriptorSet);
		auto [descriptorSet, pool] = AllocateDescriptorSet(dslIndex);
		
		DescriptorSet* ds = descriptorSets.New();
		ds->descriptorSet = descriptorSet;
		ds->pool = pool;
		ds->refCount = 1;
		ds->resources.resize(maxBinding + 1, nullptr);
		
		return WrapDescriptorSet(ds);
	}
	
	void DestroyDescriptorSet(DescriptorSetHandle set)
	{
		UnwrapDescriptorSet(set)->UnRef();
	}
	
	void BindTextureDS(TextureViewHandle textureViewHandle, SamplerHandle samplerHandle,
	                   DescriptorSetHandle setHandle, uint32_t binding)
	{
		DescriptorSet* ds = UnwrapDescriptorSet(setHandle);
		TextureView* view = UnwrapTextureView(textureViewHandle);
		
		ds->AssignResource(binding, view->texture);
		
		VkSampler sampler = reinterpret_cast<VkSampler>(samplerHandle);
		if (sampler == VK_NULL_HANDLE)
		{
			if (view->texture->defaultSampler == VK_NULL_HANDLE)
			{
				EG_PANIC("Attempted to bind texture with no sampler specified.")
			}
			sampler = view->texture->defaultSampler;
		}
		
		VkDescriptorImageInfo imageInfo;
		imageInfo.imageView = view->view;
		imageInfo.sampler = sampler;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		
		VkWriteDescriptorSet writeDS = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		writeDS.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeDS.descriptorCount = 1;
		writeDS.dstSet = ds->descriptorSet;
		writeDS.dstBinding = binding;
		writeDS.pImageInfo = &imageInfo;
		
		vkUpdateDescriptorSets(ctx.device, 1, &writeDS, 0, nullptr);
	}
	
	void BindStorageImageDS(TextureViewHandle textureViewHandle, DescriptorSetHandle setHandle, uint32_t binding)
	{
		DescriptorSet* ds = UnwrapDescriptorSet(setHandle);
		TextureView* view = UnwrapTextureView(textureViewHandle);
		
		ds->AssignResource(binding, view->texture);
		
		VkDescriptorImageInfo imageInfo;
		imageInfo.imageView = view->view;
		imageInfo.sampler = VK_NULL_HANDLE;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		
		VkWriteDescriptorSet writeDS = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		writeDS.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writeDS.descriptorCount = 1;
		writeDS.dstSet = ds->descriptorSet;
		writeDS.dstBinding = binding;
		writeDS.pImageInfo = &imageInfo;
		
		vkUpdateDescriptorSets(ctx.device, 1, &writeDS, 0, nullptr);
	}
	
	void BindUniformBufferDS(BufferHandle bufferHandle, DescriptorSetHandle setHandle,
		uint32_t binding, uint64_t offset, uint64_t range)
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
	
	void BindStorageBufferDS(BufferHandle bufferHandle, DescriptorSetHandle setHandle,
		uint32_t binding, uint64_t offset, uint64_t range)
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
		DescriptorSet* ds = UnwrapDescriptorSet(handle);
		RefResource(cc, *ds);
		AbstractPipeline* pipeline = GetCtxState(cc).pipeline;
		vkCmdBindDescriptorSets(GetCB(cc), pipeline->bindPoint, pipeline->pipelineLayout,
			set, 1, &ds->descriptorSet, 0, nullptr);
	}
}

#endif
