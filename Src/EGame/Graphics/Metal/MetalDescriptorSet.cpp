#include "../../Assert.hpp"
#include "../Abstraction.hpp"
#include "MetalBuffer.hpp"
#include "MetalCommandContext.hpp"
#include "MetalMain.hpp"
#include "MetalPipeline.hpp"
#include "MetalTexture.hpp"

namespace eg::graphics_api::mtl
{
struct BufferBinding
{
	MTL::Buffer* buffer;
	uint64_t offset;
};

using ResourceVariant = std::variant<std::monostate, BufferBinding, MTL::Texture*, MTL::SamplerState*>;

struct DescriptorSet
{
	uint32_t maxBindingPlusOne;
	ResourceVariant resources[];
};

DescriptorSet& UnwrapDescriptorSet(DescriptorSetHandle handle)
{
	return *reinterpret_cast<DescriptorSet*>(handle);
}

static inline DescriptorSetHandle CreateDescriptorSet(uint32_t maxBindingPlusOne)
{
	void* memory = malloc(sizeof(DescriptorSet) + sizeof(ResourceVariant) * maxBindingPlusOne);
	static_cast<DescriptorSet*>(memory)->maxBindingPlusOne = maxBindingPlusOne;
	for (uint32_t i = 0; i < maxBindingPlusOne; i++)
		new (static_cast<char*>(memory) + sizeof(DescriptorSet) + sizeof(ResourceVariant) * i) ResourceVariant;
	return static_cast<DescriptorSetHandle>(memory);
}

DescriptorSetHandle CreateDescriptorSetP(PipelineHandle pipeline, uint32_t set)
{
	return CreateDescriptorSet(UnwrapPipeline(pipeline).descriptorSetsMaxBindingPlusOne.at(set));
}

DescriptorSetHandle CreateDescriptorSetB(std::span<const DescriptorSetBinding> bindings)
{
	return CreateDescriptorSet(DescriptorSetBinding::MaxBindingPlusOne(bindings));
}

void DestroyDescriptorSet(DescriptorSetHandle set)
{
	free(set);
}

static void BindResource(DescriptorSetHandle setHandle, uint32_t binding, ResourceVariant resource)
{
	DescriptorSet& ds = UnwrapDescriptorSet(setHandle);
	EG_ASSERT(binding < ds.maxBindingPlusOne);
	ds.resources[binding] = resource;
}

void BindTextureDS(TextureViewHandle textureView, DescriptorSetHandle set, uint32_t binding, eg::TextureUsage _usage)
{
	BindResource(set, binding, UnwrapTextureView(textureView));
}

void BindStorageImageDS(TextureViewHandle textureView, DescriptorSetHandle set, uint32_t binding)
{
	BindResource(set, binding, UnwrapTextureView(textureView));
}

void BindSamplerDS(SamplerHandle sampler, DescriptorSetHandle set, uint32_t binding)
{
	BindResource(set, binding, reinterpret_cast<MTL::SamplerState*>(sampler));
}

void BindUniformBufferDS(
	BufferHandle handle, DescriptorSetHandle set, uint32_t binding, uint64_t offset, std::optional<uint64_t> _range)
{
	BindResource(set, binding, BufferBinding{ .buffer = UnwrapBuffer(handle), .offset = offset });
}

void BindStorageBufferDS(
	BufferHandle handle, DescriptorSetHandle set, uint32_t binding, uint64_t offset, std::optional<uint64_t> _range)
{
	BindResource(set, binding, BufferBinding{ .buffer = UnwrapBuffer(handle), .offset = offset });
}

void BindDescriptorSet(
	CommandContextHandle ctx, uint32_t set, DescriptorSetHandle handle, std::span<const uint32_t> dynamicOffsets)
{
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(ctx);

	size_t nextDynamicOffsetIndex = 0;

	const DescriptorSet& descriptorSet = UnwrapDescriptorSet(handle);
	for (uint32_t binding = 0; binding <= descriptorSet.maxBindingPlusOne; binding++)
	{
		if (auto resource = std::get_if<BufferBinding>(&descriptorSet.resources[binding]))
		{
			uint64_t offset = resource->offset;
			if (offset == BIND_BUFFER_OFFSET_DYNAMIC)
			{
				EG_ASSERT(nextDynamicOffsetIndex < dynamicOffsets.size());
				offset = dynamicOffsets[nextDynamicOffsetIndex];
				nextDynamicOffsetIndex++;
			}
			mcc.BindBuffer(resource->buffer, offset, set, binding);
		}
		else if (auto resource = std::get_if<MTL::Texture*>(&descriptorSet.resources[binding]))
		{
			mcc.BindTexture(*resource, set, binding);
		}
		else if (auto resource = std::get_if<MTL::SamplerState*>(&descriptorSet.resources[binding]))
		{
			mcc.BindSampler(*resource, set, binding);
		}
	}
}
} // namespace eg::graphics_api::mtl
