#include "../../Assert.hpp"
#include "OpenGLBuffer.hpp"
#include "OpenGLTexture.hpp"
#include "Pipeline.hpp"

#include <cstring>

namespace eg::graphics_api::gl
{
struct Binding
{
	TextureView* textureView;
	GLuint bufferOrSampler;
	GLsizeiptr offset;
	GLsizeiptr range;
	bool assigned;
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

static inline DescriptorSet* UnwrapDescriptorSet(DescriptorSetHandle handle)
{
	return reinterpret_cast<DescriptorSet*>(handle);
}

static inline DescriptorSetHandle CreateDescriptorSet(uint32_t maxBinding)
{
	const size_t extraMemory = (maxBinding + 1) * sizeof(Binding);
	const size_t bindingsOffset = RoundToNextMultiple(sizeof(DescriptorSet), alignof(Binding));
	char* memory = static_cast<char*>(std::calloc(bindingsOffset + extraMemory, 1));
	DescriptorSet* ds = new (memory) DescriptorSet;

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
	set->bindings[binding].bufferOrSampler = UnsignedNarrow<GLuint>(reinterpret_cast<uintptr_t>(sampler));
	set->bindings[binding].assigned = true;
}

void BindStorageImageDS(TextureViewHandle viewHandle, DescriptorSetHandle setHandle, uint32_t binding)
{
	DescriptorSet* set = UnwrapDescriptorSet(setHandle);
	set->CheckBinding(binding);
	set->bindings[binding].textureView = UnwrapTextureView(viewHandle);
	set->bindings[binding].assigned = true;
}

static inline void BindBuffer(
	BufferHandle bufferHandle, DescriptorSetHandle setHandle, uint32_t binding, uint64_t offset,
	std::optional<uint64_t> range)
{
	if (offset == BIND_BUFFER_OFFSET_DYNAMIC)
		offset = 0;

	DescriptorSet* set = UnwrapDescriptorSet(setHandle);
	set->CheckBinding(binding);
	Buffer* buffer = UnwrapBuffer(bufferHandle);
	uint64_t resolvedRange = range.value_or(buffer->size - offset);
	buffer->AssertRange(offset, resolvedRange);
	set->bindings[binding].bufferOrSampler = buffer->buffer;
	set->bindings[binding].offset = static_cast<GLsizeiptr>(offset);
	set->bindings[binding].range = static_cast<GLsizeiptr>(resolvedRange);
	set->bindings[binding].assigned = true;
}

void BindUniformBufferDS(
	BufferHandle buffer, DescriptorSetHandle setHandle, uint32_t binding, uint64_t offset,
	std::optional<uint64_t> range)
{
	BindBuffer(buffer, setHandle, binding, offset, range);
}

void BindStorageBufferDS(
	BufferHandle buffer, DescriptorSetHandle setHandle, uint32_t binding, uint64_t offset,
	std::optional<uint64_t> range)
{
	BindBuffer(buffer, setHandle, binding, offset, range);
}

void BindDescriptorSet(
	CommandContextHandle, uint32_t set, DescriptorSetHandle handle, std::span<const uint32_t> dynamicOffsets)
{
	DescriptorSet* ds = UnwrapDescriptorSet(handle);

	size_t nextDynamicOffsetIndex = 0;

	size_t curidx = currentPipeline->FindBindingsSetStartIndex(set);
	while (curidx < currentPipeline->bindings.size() && currentPipeline->bindings[curidx].set == set)
	{
		MarkBindingAsSatisfied(curidx);

		const MappedBinding& binding = currentPipeline->bindings[curidx];
		const Binding& dsBinding = ds->bindings[binding.binding];
		if (!dsBinding.assigned)
			EG_PANIC("Descriptor set binding not updated before binding descriptor set");

		GLsizeiptr bufferOffset = dsBinding.offset;
		if (BindingTypeHasDynamicOffset(binding.type))
		{
			EG_ASSERT(nextDynamicOffsetIndex < dynamicOffsets.size());
			bufferOffset += dynamicOffsets[nextDynamicOffsetIndex];
			nextDynamicOffsetIndex++;
		}

		switch (binding.type)
		{
		case BindingType::UniformBuffer:
		case BindingType::UniformBufferDynamicOffset:
			glBindBufferRange(
				GL_UNIFORM_BUFFER, binding.glBinding, dsBinding.bufferOrSampler, bufferOffset, dsBinding.range);
			break;
		case BindingType::StorageBuffer:
		case BindingType::StorageBufferDynamicOffset:
#ifndef EG_GLES
			glBindBufferRange(
				GL_SHADER_STORAGE_BUFFER, binding.glBinding, dsBinding.bufferOrSampler, bufferOffset, dsBinding.range);
#endif
			break;
		case BindingType::Texture: dsBinding.textureView->Bind(dsBinding.bufferOrSampler, binding.glBinding); break;
		case BindingType::StorageImage: dsBinding.textureView->BindAsStorageImage(binding.glBinding); break;
		}

		curidx++;
	}
}
} // namespace eg::graphics_api::gl
