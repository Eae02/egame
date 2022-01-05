#include "Pipeline.hpp"
#include "OpenGLTexture.hpp"
#include "OpenGLBuffer.hpp"

#include <cstring>

namespace eg::graphics_api::gl
{
	struct Binding
	{
		TextureView* textureView;
		GLuint bufferOrSampler;
		GLsizeiptr offset;
		GLsizeiptr range;
	};
	
	struct DescriptorSet
	{
		uint32_t maxBinding;
		Binding* bindings;
		
		void CheckBinding(uint32_t binding) const
		{
			if (binding > maxBinding)
			{
				EG_PANIC("Attempted to bind to out of range descriptor set binding: " << binding)
			}
		}
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
	
	DescriptorSetHandle CreateDescriptorSetB(std::span<const DescriptorSetBinding> bindings)
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
	
	void BindTextureDS(TextureViewHandle viewHandle, SamplerHandle sampler, DescriptorSetHandle setHandle, uint32_t binding)
	{
		DescriptorSet* set = UnwrapDescriptorSet(setHandle);
		set->CheckBinding(binding);
		set->bindings[binding].textureView = UnwrapTextureView(viewHandle);
		set->bindings[binding].bufferOrSampler = (GLuint)reinterpret_cast<uintptr_t>(sampler);
	}
	
	void BindStorageImageDS(TextureViewHandle viewHandle, DescriptorSetHandle setHandle, uint32_t binding)
	{
		DescriptorSet* set = UnwrapDescriptorSet(setHandle);
		set->CheckBinding(binding);
		set->bindings[binding].textureView = UnwrapTextureView(viewHandle);
	}
	
	static inline void BindBuffer(BufferHandle buffer, DescriptorSetHandle setHandle, uint32_t binding, uint64_t offset, uint64_t range)
	{
		DescriptorSet* set = UnwrapDescriptorSet(setHandle);
		set->CheckBinding(binding);
		UnwrapBuffer(buffer)->AssertRange(offset, range);
		set->bindings[binding].bufferOrSampler = UnwrapBuffer(buffer)->buffer;
		set->bindings[binding].offset = static_cast<GLsizeiptr>(offset);
		set->bindings[binding].range = static_cast<GLsizeiptr>(range);
	}
	
	void BindUniformBufferDS(BufferHandle buffer, DescriptorSetHandle setHandle, uint32_t binding, uint64_t offset, uint64_t range)
	{
		BindBuffer(buffer, setHandle, binding, offset, range);
	}
	
	void BindStorageBufferDS(BufferHandle buffer, DescriptorSetHandle setHandle, uint32_t binding, uint64_t offset, uint64_t range)
	{
		BindBuffer(buffer, setHandle, binding, offset, range);
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
				dsBinding.textureView->Bind(dsBinding.bufferOrSampler, binding.glBinding);
				break;
			case BindingType::StorageImage:
				dsBinding.textureView->BindAsStorageImage(binding.glBinding);
				break;
			}
		}
	}
}
