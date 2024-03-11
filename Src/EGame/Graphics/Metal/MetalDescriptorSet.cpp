#include "../../Assert.hpp"
#include "../Abstraction.hpp"
#include "../DescriptorSetWrapper.hpp"
#include "MetalBuffer.hpp"
#include "MetalCommandContext.hpp"
#include "MetalMain.hpp"
#include "MetalPipeline.hpp"
#include "MetalTexture.hpp"

namespace eg::graphics_api::mtl
{
static inline DescriptorSetHandle CreateDescriptorSet(uint32_t maxBindingPlusOne)
{
	return DescriptorSetWrapper::Allocate(maxBindingPlusOne)->Wrap();
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
	DescriptorSetWrapper::Unwrap(set);
}

void BindTextureDS(TextureViewHandle textureView, SamplerHandle sampler, DescriptorSetHandle set, uint32_t binding)
{
	EG_ASSERT(sampler != nullptr);
	DescriptorSetWrapper::Unwrap(set)->BindTexture(binding, { .textureView = textureView, .sampler = sampler });
}

void BindStorageImageDS(TextureViewHandle textureView, DescriptorSetHandle set, uint32_t binding)
{
	DescriptorSetWrapper::Unwrap(set)->BindTexture(binding, { .textureView = textureView });
}

static void BindBufferDS(BufferHandle handle, DescriptorSetHandle set, uint32_t binding, uint64_t offset)
{
	DescriptorSetWrapper::Unwrap(set)->BindBuffer(binding, { .buffer = handle, .offset = offset });
}

void BindUniformBufferDS(
	BufferHandle handle, DescriptorSetHandle set, uint32_t binding, uint64_t offset, std::optional<uint64_t> _range)
{
	BindBufferDS(handle, set, binding, offset);
}

void BindStorageBufferDS(
	BufferHandle handle, DescriptorSetHandle set, uint32_t binding, uint64_t offset, std::optional<uint64_t> _range)
{
	BindBufferDS(handle, set, binding, offset);
}

void BindDescriptorSet(
	CommandContextHandle ctx, uint32_t set, DescriptorSetHandle handle, std::span<const uint32_t> dynamicOffsets)
{
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(ctx);

	size_t nextDynamicOffsetIndex = 0;

	DescriptorSetWrapper::Unwrap(handle)->BindDescriptorSet(
		[&]<typename T>(uint32_t binding, const T& resource)
		{
			if constexpr (std::is_same_v<T, DescriptorSetWrapper::BufferBinding>)
			{
				uint64_t offset = resource.offset;
				if (offset == BIND_BUFFER_OFFSET_DYNAMIC)
				{
					EG_ASSERT(nextDynamicOffsetIndex < dynamicOffsets.size());
					offset = dynamicOffsets[nextDynamicOffsetIndex];
					nextDynamicOffsetIndex++;
				}

				mcc.BindBuffer(UnwrapBuffer(resource.buffer), offset, set, binding);
			}
			else if constexpr (std::is_same_v<T, DescriptorSetWrapper::TextureBinding>)
			{
				mcc.BindTexture(UnwrapTextureView(resource.textureView), set, binding);
				if (resource.sampler != nullptr)
				{
					mcc.BindSampler(reinterpret_cast<MTL::SamplerState*>(resource.sampler), set, binding);
				}
			}
		});
}
} // namespace eg::graphics_api::mtl
