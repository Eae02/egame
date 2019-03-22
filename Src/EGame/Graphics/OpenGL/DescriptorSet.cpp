#include "Pipeline.hpp"
#include "OpenGLTexture.hpp"
#include "OpenGLBuffer.hpp"

namespace eg::graphics_api::gl
{
	struct DescriptorSet
	{
		uint32_t set;
		AbstractPipeline* pipeline;
		GLuint* textures;
		GLuint* samplers;
		GLuint* uniBuffers;
		GLsizeiptr* uniBufferOffsets;
		GLsizeiptr* uniBufferRanges;
	};
	
	inline DescriptorSet* UnwrapDescriptorSet(DescriptorSetHandle handle)
	{
		return reinterpret_cast<DescriptorSet*>(handle);
	}
	
	DescriptorSetHandle CreateDescriptorSet(PipelineHandle pipelineHandle, uint32_t set)
	{
		AbstractPipeline* pipeline = UnwrapPipeline(pipelineHandle);
		const PipelineDescriptorSet& pipelineDS = pipeline->sets[set];
		
		const size_t extraMemory = pipelineDS.numTextures * 2 * sizeof(GLuint) +
			pipelineDS.numUniformBuffers * (sizeof(GLuint) + sizeof(GLsizeiptr) * 2);
		char* memory = static_cast<char*>(std::malloc(sizeof(DescriptorSet) + extraMemory));
		DescriptorSet* ds = new (memory) DescriptorSet;
		
		std::memset(memory + sizeof(DescriptorSet), 0, extraMemory);
		ds->textures = reinterpret_cast<GLuint*>(memory + sizeof(DescriptorSet));
		ds->samplers = ds->textures + pipelineDS.numTextures;
		ds->uniBuffers = ds->samplers + pipelineDS.numTextures;
		ds->uniBufferOffsets = reinterpret_cast<GLsizeiptr*>(ds->uniBuffers + pipelineDS.numUniformBuffers);
		ds->uniBufferRanges = ds->uniBufferOffsets + pipelineDS.numUniformBuffers;
		
		ds->set = set;
		ds->pipeline = pipeline;
		
		return reinterpret_cast<DescriptorSetHandle>(ds);
	}
	
	void DestroyDescriptorSet(DescriptorSetHandle set)
	{
		std::free(UnwrapDescriptorSet(set));
	}
	
	void BindTextureDS(TextureHandle texture, SamplerHandle sampler, DescriptorSetHandle setHandle, uint32_t binding)
	{
		DescriptorSet* set = UnwrapDescriptorSet(setHandle);
		uint32_t idx = ResolveBinding(*set->pipeline, set->set, binding) - set->pipeline->sets[set->set].firstTexture;
		EG_ASSERT(idx < set->pipeline->sets[set->set].numTextures);
		set->textures[idx] = UnwrapTexture(texture)->texture;
		set->samplers[idx] = (GLuint)reinterpret_cast<uintptr_t>(sampler);
	}
	
	void BindUniformBufferDS(BufferHandle buffer, DescriptorSetHandle setHandle, uint32_t binding,
		uint64_t offset, uint64_t range)
	{
		DescriptorSet* set = UnwrapDescriptorSet(setHandle);
		uint32_t idx = ResolveBinding(*set->pipeline, set->set, binding) - set->pipeline->sets[set->set].firstUniformBuffer;
		EG_ASSERT(idx < set->pipeline->sets[set->set].numUniformBuffers);
		set->uniBuffers[idx] = UnwrapBuffer(buffer)->buffer;
		set->uniBufferOffsets[idx] = offset;
		set->uniBufferRanges[idx] = range;
	}
	
	void BindDescriptorSet(CommandContextHandle ctx, DescriptorSetHandle handle)
	{
		DescriptorSet* set = UnwrapDescriptorSet(handle);
		const PipelineDescriptorSet& pipelineDS = set->pipeline->sets[set->set];
		
		if (pipelineDS.numTextures > 0)
		{
			glBindTextures(pipelineDS.firstTexture, pipelineDS.numTextures, set->textures);
			glBindSamplers(pipelineDS.firstTexture, pipelineDS.numTextures, set->samplers);
		}
		
		if (pipelineDS.numUniformBuffers > 0)
		{
			glBindBuffersRange(GL_UNIFORM_BUFFER, pipelineDS.firstUniformBuffer, pipelineDS.numUniformBuffers,
				set->uniBuffers, set->uniBufferOffsets, set->uniBufferRanges);
		}
	}
}
