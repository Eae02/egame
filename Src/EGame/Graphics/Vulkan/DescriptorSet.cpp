#include "Common.hpp"
#include "DSLCache.hpp"
#include "Pipeline.hpp"
#include "Texture.hpp"
#include "Buffer.hpp"
#include "../../Alloc/ObjectPool.hpp"

namespace eg::graphics_api::vk
{
	struct DescriptorSet : Resource
	{
		VkDescriptorSet descriptorSet;
		VkDescriptorPool pool;
		uint32_t setIndex;
		AbstractPipeline* pipeline;
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
	
	DescriptorSet* UnwrapDescriptorSet(DescriptorSetHandle handle)
	{
		return reinterpret_cast<DescriptorSet*>(handle);
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
		pipeline->UnRef();
		descriptorSets.Delete(this);
	}
	
	DescriptorSetHandle CreateDescriptorSet(PipelineHandle pipelineHandle, uint32_t set)
	{
		AbstractPipeline* pipeline = UnwrapPipeline(pipelineHandle);
		auto [descriptorSet, pool] = AllocateDescriptorSet(pipeline->setsLayoutIndices[set]);
		
		DescriptorSet* ds = descriptorSets.New();
		ds->descriptorSet = descriptorSet;
		ds->pool = pool;
		ds->pipeline = pipeline;
		ds->setIndex = set;
		ds->refCount = 1;
		ds->resources.resize(GetDSLFromCache(pipeline->setsLayoutIndices[set]).maxBinding + 1, nullptr);
		
		pipeline->refCount++;
		
		return reinterpret_cast<DescriptorSetHandle>(ds);
	}
	
	void DestroyDescriptorSet(DescriptorSetHandle set)
	{
		UnwrapDescriptorSet(set)->UnRef();
	}
	
	void BindTextureDS(TextureHandle textureHandle, SamplerHandle samplerHandle,
		DescriptorSetHandle setHandle, uint32_t binding)
	{
		DescriptorSet* ds = UnwrapDescriptorSet(setHandle);
		Texture* texture = UnwrapTexture(textureHandle);
		
		ds->AssignResource(binding, texture);
		
		VkSampler sampler = reinterpret_cast<VkSampler>(samplerHandle);
		if (sampler == VK_NULL_HANDLE)
		{
			if (texture->defaultSampler == VK_NULL_HANDLE)
			{
				EG_PANIC("Attempted to bind texture with no sampler specified.")
			}
			sampler = texture->defaultSampler;
		}
		
		VkDescriptorImageInfo imageInfo;
		imageInfo.imageView = texture->imageView;
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
	
	void BindDescriptorSet(CommandContextHandle cc, DescriptorSetHandle handle)
	{
		DescriptorSet* ds = UnwrapDescriptorSet(handle);
		RefResource(cc, *ds);
		AbstractPipeline* pipeline = GetCtxState(cc).pipeline;
		vkCmdBindDescriptorSets(GetCB(cc), pipeline->bindPoint, pipeline->pipelineLayout,
			ds->setIndex, 1, &ds->descriptorSet, 0, nullptr);
	}
}
