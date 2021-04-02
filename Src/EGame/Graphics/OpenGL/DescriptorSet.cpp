#include "Pipeline.hpp"
#include "OpenGLTexture.hpp"
#include "OpenGLBuffer.hpp"

#include <cstring>

namespace eg::graphics_api::gl
{
	struct Binding
	{
		Texture* texture;
		TextureSubresource subresource;
		GLenum forcedViewType;
		Format textureViewFormat;
		GLuint bufferOrSampler;
		GLsizeiptr offset;
		GLsizeiptr range;
	};
	
	struct DescriptorSet
	{
		uint32_t maxBinding;
		Binding* bindings;
	};
	
	inline DescriptorSet* UnwrapDescriptorSet(DescriptorSetHandle handle)
	{
		return reinterpret_cast<DescriptorSet*>(handle);
	}
	
	DescriptorSetHandle CreateDescriptorSet(uint32_t maxBinding)
	{
		const size_t extraMemory = (maxBinding + 1) * sizeof(Binding);
		const size_t bindingsOffset = RoundToNextMultiple(sizeof(DescriptorSet), alignof(Binding));
		char* memory = static_cast<char*>(std::malloc(bindingsOffset + extraMemory));
		DescriptorSet* ds = new (memory) DescriptorSet;
		
		std::memset(memory + bindingsOffset, 0, extraMemory);
		ds->maxBinding = maxBinding;
		ds->bindings = reinterpret_cast<Binding*>(memory + sizeof(DescriptorSet));
		
		return reinterpret_cast<DescriptorSetHandle>(ds);
	}
	
	DescriptorSetHandle CreateDescriptorSetP(PipelineHandle pipelineHandle, uint32_t set)
	{
		AbstractPipeline* pipeline = UnwrapPipeline(pipelineHandle);
		return CreateDescriptorSet(pipeline->sets[set].maxBinding);
	}
	
	DescriptorSetHandle CreateDescriptorSetB(Span<const DescriptorSetBinding> bindings)
	{
		uint32_t maxBinding = 0;
		for (const DescriptorSetBinding& binding : bindings)
			maxBinding = std::max(maxBinding, binding.binding);
		return CreateDescriptorSet(maxBinding);
	}
	
	void DestroyDescriptorSet(DescriptorSetHandle set)
	{
		std::free(UnwrapDescriptorSet(set));
	}
	
	void BindTextureDS(TextureHandle texture, SamplerHandle sampler, DescriptorSetHandle setHandle, uint32_t binding,
	                   const TextureSubresource& subresource, TextureBindFlags flags, Format differentFormat)
	{
		DescriptorSet* set = UnwrapDescriptorSet(setHandle);
		EG_ASSERT(binding <= set->maxBinding);
		set->bindings[binding].texture = UnwrapTexture(texture);
		set->bindings[binding].forcedViewType = HasFlag(flags, TextureBindFlags::ArrayLayerAsTexture2D) ? GL_TEXTURE_2D : 0;
		set->bindings[binding].textureViewFormat = differentFormat;
		set->bindings[binding].subresource = subresource;
		set->bindings[binding].bufferOrSampler = (GLuint)reinterpret_cast<uintptr_t>(sampler);
	}
	
	void BindStorageImageDS(TextureHandle textureHandle, DescriptorSetHandle setHandle, uint32_t binding,
		const TextureSubresourceLayers& subresource)
	{
		DescriptorSet* set = UnwrapDescriptorSet(setHandle);
		EG_ASSERT(binding <= set->maxBinding);
		set->bindings[binding].texture = UnwrapTexture(textureHandle);
		set->bindings[binding].subresource = subresource.AsSubresource();
	}
	
	void BindUniformBufferDS(BufferHandle buffer, DescriptorSetHandle setHandle, uint32_t binding,
		uint64_t offset, uint64_t range)
	{
		DescriptorSet* set = UnwrapDescriptorSet(setHandle);
		EG_ASSERT(binding <= set->maxBinding);
		set->bindings[binding].bufferOrSampler = UnwrapBuffer(buffer)->buffer;
		set->bindings[binding].offset = offset;
		set->bindings[binding].range = range;
	}
	
	void BindStorageBufferDS(BufferHandle buffer, DescriptorSetHandle setHandle, uint32_t binding,
		uint64_t offset, uint64_t range)
	{
		DescriptorSet* set = UnwrapDescriptorSet(setHandle);
		EG_ASSERT(binding <= set->maxBinding);
		set->bindings[binding].bufferOrSampler = UnwrapBuffer(buffer)->buffer;
		set->bindings[binding].offset = offset;
		set->bindings[binding].range = range;
	}
	
	void BindDescriptorSet(CommandContextHandle, uint32_t set, DescriptorSetHandle handle)
	{
		DescriptorSet* ds = UnwrapDescriptorSet(handle);
		
		for (const MappedBinding& binding : currentPipeline->bindings)
		{
			if (binding.set != set)
				continue;
			
			const Binding& dsBinding = ds->bindings[binding.binding];
			
			switch (binding.type)
			{
			case BindingType::UniformBuffer:
				glBindBufferRange(GL_UNIFORM_BUFFER, binding.glBinding, dsBinding.bufferOrSampler,
					dsBinding.offset, dsBinding.range);
				break;
			case BindingType::StorageBuffer:
				glBindBufferRange(GL_SHADER_STORAGE_BUFFER, binding.glBinding, dsBinding.bufferOrSampler,
					dsBinding.offset, dsBinding.range);
				break;
			case BindingType::Texture:
			{
				GLuint view = dsBinding.texture->GetView(dsBinding.subresource, dsBinding.forcedViewType, dsBinding.textureViewFormat);
				glActiveTexture(GL_TEXTURE0 + binding.glBinding);
				glBindTexture(dsBinding.forcedViewType ? dsBinding.forcedViewType : dsBinding.texture->type, view);
				glBindSampler(binding.glBinding, dsBinding.bufferOrSampler);
				break;
			}
			case BindingType::StorageImage:
				dsBinding.texture->BindAsStorageImage(binding.glBinding, dsBinding.subresource);
				break;
			}
		}
	}
}
