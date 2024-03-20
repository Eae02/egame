#include "WGPUDescriptorSet.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "WGPUBuffer.hpp"
#include "WGPUCommandContext.hpp"
#include "WGPUPipeline.hpp"
#include "WGPUTranslation.hpp"

namespace eg::graphics_api::webgpu
{
CachedBindGroupLayout::~CachedBindGroupLayout()
{
	wgpuBindGroupLayoutRelease(bindGroupLayout);
}

static std::unique_ptr<ICachedDescriptorSetLayout> CreateCachedDescriptorSetLayout(
	std::span<const DescriptorSetBinding> bindings, BindMode)
{
	std::unique_ptr<CachedBindGroupLayout> result = std::make_unique<CachedBindGroupLayout>();

	std::vector<WGPUBindGroupLayoutEntry> layoutEntries(bindings.size());

	for (size_t i = 0; i < bindings.size(); i++)
	{
		layoutEntries[i] = WGPUBindGroupLayoutEntry{
			.binding = bindings[i].binding,
			.visibility = TranslateShaderStageFlags(bindings[i].shaderAccess),
		};

		switch (bindings[i].type)
		{
		case BindingType::UniformBufferDynamicOffset: layoutEntries[i].buffer.hasDynamicOffset = true; [[fallthrough]];
		case BindingType::UniformBuffer: layoutEntries[i].buffer.type = WGPUBufferBindingType_Uniform; break;

		case BindingType::StorageBufferDynamicOffset: layoutEntries[i].buffer.hasDynamicOffset = true; [[fallthrough]];
		case BindingType::StorageBuffer:
			if (bindings[i].rwMode == ReadWriteMode::ReadOnly)
				layoutEntries[i].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
			else
				layoutEntries[i].buffer.type = WGPUBufferBindingType_Storage;
			break;

		case BindingType::Texture:
		case BindingType::StorageImage: EG_PANIC("Unimplemented");
		}

		// The bindings passed in are guaranteed to be sorted by DescriptorSetLayoutCache
		result->activeBindingIndicesSorted.push_back(bindings[i].binding);
	}

	const WGPUBindGroupLayoutDescriptor descriptor = {
		.entries = layoutEntries.data(),
		.entryCount = layoutEntries.size(),
	};
	result->bindGroupLayout = wgpuDeviceCreateBindGroupLayout(wgpuctx.device, &descriptor);

	return result;
}

static DescriptorSetLayoutCache descriptorSetLayoutCache{ &CreateCachedDescriptorSetLayout };

const CachedBindGroupLayout& GetBindGroupLayout(std::span<const DescriptorSetBinding> bindings)
{
	return static_cast<CachedBindGroupLayout&>(descriptorSetLayoutCache.Get(bindings, {}));
}

struct DescriptorSet
{
	WGPUBindGroupLayout bindGroupLayout = nullptr;
	WGPUBindGroup bindGroup = nullptr;

	std::vector<WGPUBindGroupEntry> entries;
	std::bitset<64> entriesNotBoundMask;
	bool dirty = true;

	void ReleaseBindGroup()
	{
		if (bindGroup != nullptr)
		{
			wgpuBindGroupRelease(bindGroup);
			bindGroup = nullptr;
		}
	}

	void FlushDirty()
	{
		if (!dirty)
			return;
		ReleaseBindGroup();

		WGPUBindGroupDescriptor descriptor = { .layout = bindGroupLayout };

		// If there are unbound entries we need to copy only these to a new vector and use that to create the bind
		// group, otherwise we can use the entries vector in the DescriptorSet
		std::vector<WGPUBindGroupEntry> entriesBound;
		if (entriesNotBoundMask.any())
		{
			for (size_t i = 0; i < entries.size(); i++)
			{
				if (!entriesNotBoundMask.test(i))
					entriesBound.push_back(entries[i]);
			}
			descriptor.entries = entriesBound.data();
			descriptor.entryCount = entriesBound.size();
		}
		else
		{
			descriptor.entries = entries.data();
			descriptor.entryCount = entries.size();
		}

		bindGroup = wgpuDeviceCreateBindGroup(wgpuctx.device, &descriptor);
		dirty = false;
	}

	void SetBinding(const WGPUBindGroupEntry& entry)
	{
		auto entryIt = std::lower_bound(
			entries.begin(), entries.end(), entry.binding,
			[&](const WGPUBindGroupEntry& a, uint32_t b) { return a.binding < b; });

		if (entryIt == entries.end() || entryIt->binding != entry.binding)
		{
			// clang-format off
			EG_PANIC("Attempted to bind to binding index " << entry.binding << ", which does not exist in the descriptor set");
			// clang-format on
		}

		entriesNotBoundMask.reset(entryIt - entries.begin());
		dirty = true;
		*entryIt = entry;
	}

	static DescriptorSet& Unwrap(DescriptorSetHandle handle) { return *reinterpret_cast<DescriptorSet*>(handle); }
};

static ConcurrentObjectPool<DescriptorSet> descriptorSetObjectPool;

static DescriptorSetHandle CreateDescriptorSet(const CachedBindGroupLayout& bindGroupLayout)
{
	DescriptorSet& descriptorSet = *descriptorSetObjectPool.New();
	descriptorSet.bindGroupLayout = bindGroupLayout.bindGroupLayout;

	descriptorSet.entries.resize(bindGroupLayout.activeBindingIndicesSorted.size());
	for (size_t i = 0; i < descriptorSet.entries.size(); i++)
	{
		descriptorSet.entries[i] = { .binding = bindGroupLayout.activeBindingIndicesSorted[i] };
		descriptorSet.entriesNotBoundMask.set(i);
	}

	return reinterpret_cast<DescriptorSetHandle>(&descriptorSet);
}

DescriptorSetHandle CreateDescriptorSetP(PipelineHandle pipeline, uint32_t setIndex)
{
	const CachedBindGroupLayout* layout = AbstractPipeline::Unwrap(pipeline).bindGroupLayouts.at(setIndex);
	EG_ASSERT(layout != nullptr);
	return CreateDescriptorSet(*layout);
}

DescriptorSetHandle CreateDescriptorSetB(std::span<const DescriptorSetBinding> bindings)
{
	return CreateDescriptorSet(GetBindGroupLayout(bindings));
}

void DestroyDescriptorSet(DescriptorSetHandle handle)
{
	DescriptorSet& descriptorSet = DescriptorSet::Unwrap(handle);
	descriptorSet.ReleaseBindGroup();
	descriptorSetObjectPool.Delete(&descriptorSet);
}

void BindTextureDS(TextureViewHandle textureView, SamplerHandle sampler, DescriptorSetHandle set, uint32_t binding)
{
	EG_PANIC("Unimplemented: BindTextureDS")
}

void BindStorageImageDS(TextureViewHandle textureView, DescriptorSetHandle set, uint32_t binding)
{
	EG_PANIC("Unimplemented: BindStorageImageDS")
}

static void BindBufferDS(
	BufferHandle handle, DescriptorSetHandle set, uint32_t binding, uint64_t offset, std::optional<uint64_t> range)
{
	Buffer& buffer = Buffer::Unwrap(handle);

	uint64_t resolvedRange;
	if (offset == BIND_BUFFER_OFFSET_DYNAMIC)
	{
		resolvedRange = range.value();
		offset = 0;
	}
	else
	{
		resolvedRange = range.value_or(buffer.size - offset);
	}

	DescriptorSet::Unwrap(set).SetBinding(WGPUBindGroupEntry{
		.binding = binding,
		.buffer = buffer.buffer,
		.offset = offset,
		.size = resolvedRange,
	});
}

void BindUniformBufferDS(
	BufferHandle handle, DescriptorSetHandle set, uint32_t binding, uint64_t offset, std::optional<uint64_t> range)
{
	BindBufferDS(handle, set, binding, offset, range);
}

void BindStorageBufferDS(
	BufferHandle handle, DescriptorSetHandle set, uint32_t binding, uint64_t offset, std::optional<uint64_t> range)
{
	BindBufferDS(handle, set, binding, offset, range);
}

void BindDescriptorSet(
	CommandContextHandle cc, uint32_t setIndex, DescriptorSetHandle handle, std::span<const uint32_t> dynamicOffsets)
{
	CommandContext& wcc = CommandContext::Unwrap(cc);

	DescriptorSet& descriptorSet = DescriptorSet::Unwrap(handle);
	descriptorSet.FlushDirty();

	if (wcc.renderPassEncoder != nullptr)
	{
		wgpuRenderPassEncoderSetBindGroup(
			wcc.renderPassEncoder, setIndex, descriptorSet.bindGroup, dynamicOffsets.size(), dynamicOffsets.data());
	}
	else if (wcc.computePassEncoder != nullptr)
	{
		wgpuComputePassEncoderSetBindGroup(
			wcc.computePassEncoder, setIndex, descriptorSet.bindGroup, dynamicOffsets.size(), dynamicOffsets.data());
	}
	else
	{
		EG_PANIC("BindDescriptorSet called with no active encoder");
	}
}

} // namespace eg::graphics_api::webgpu
