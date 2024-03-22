#include "WGPUDescriptorSet.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "WGPUBuffer.hpp"
#include "WGPUCommandContext.hpp"
#include "WGPUPipeline.hpp"
#include "WGPUTexture.hpp"
#include "WGPUTranslation.hpp"

#include <bitset>
#include <memory>
#include <vector>

namespace eg::graphics_api::webgpu
{
CachedBindGroupLayout::~CachedBindGroupLayout()
{
	wgpuBindGroupLayoutRelease(bindGroupLayout);
}

static inline WGPUStorageTextureAccess TranslateStorageTextureAccess(ReadWriteMode mode)
{
	switch (mode)
	{
	case ReadWriteMode::ReadWrite: return WGPUStorageTextureAccess_ReadWrite;
	case ReadWriteMode::ReadOnly: return WGPUStorageTextureAccess_ReadOnly;
	case ReadWriteMode::WriteOnly: return WGPUStorageTextureAccess_WriteOnly;
	}
	EG_UNREACHABLE
}

static void ProcessLayoutEntry(WGPUBindGroupLayoutEntry& entry, const BindingTypeUniformBuffer& bindingType)
{
	entry.buffer.type = WGPUBufferBindingType_Uniform;
	entry.buffer.hasDynamicOffset = bindingType.dynamicOffset;
}

static void ProcessLayoutEntry(WGPUBindGroupLayoutEntry& entry, const BindingTypeStorageBuffer& bindingType)
{
	if (bindingType.rwMode == ReadWriteMode::ReadOnly)
		entry.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
	else
		entry.buffer.type = WGPUBufferBindingType_Storage;
	entry.buffer.hasDynamicOffset = bindingType.dynamicOffset;
}

static WGPUTextureSampleType TranslateSampleType(TextureSampleMode sampleMode)
{
	switch (sampleMode)
	{
	case TextureSampleMode::Float: return WGPUTextureSampleType_Float;
	case TextureSampleMode::UnfilterableFloat: return WGPUTextureSampleType_UnfilterableFloat;
	case TextureSampleMode::UInt: return WGPUTextureSampleType_Uint;
	case TextureSampleMode::SInt: return WGPUTextureSampleType_Sint;
	case TextureSampleMode::Depth: return WGPUTextureSampleType_Depth;
	}
	EG_UNREACHABLE
}

static void ProcessLayoutEntry(WGPUBindGroupLayoutEntry& entry, const BindingTypeTexture& bindingType)
{
	entry.texture = {
		.sampleType = TranslateSampleType(bindingType.sampleMode),
		.viewDimension = TranslateTextureViewType(bindingType.viewType),
		.multisampled = bindingType.multisample,
	};
}

static void ProcessLayoutEntry(WGPUBindGroupLayoutEntry& entry, const BindingTypeStorageImage& bindingType)
{
	entry.storageTexture = {
		.access = TranslateStorageTextureAccess(bindingType.rwMode),
		.format = TranslateTextureFormat(bindingType.format),
		.viewDimension = TranslateTextureViewType(bindingType.viewType),
	};
}

static void ProcessLayoutEntry(WGPUBindGroupLayoutEntry& entry, const BindingTypeSampler& bindingType)
{
	switch (bindingType)
	{
	case BindingTypeSampler::Default: entry.sampler.type = WGPUSamplerBindingType_Filtering; break;
	case BindingTypeSampler::Nearest: entry.sampler.type = WGPUSamplerBindingType_NonFiltering; break;
	case BindingTypeSampler::Compare: entry.sampler.type = WGPUSamplerBindingType_Comparison; break;
	}
}

static std::unique_ptr<ICachedDescriptorSetLayout> CreateCachedDescriptorSetLayout(
	std::span<const DescriptorSetBinding> bindings, bool _dynamic)
{
	std::unique_ptr<CachedBindGroupLayout> result = std::make_unique<CachedBindGroupLayout>();

	std::vector<WGPUBindGroupLayoutEntry> layoutEntries(bindings.size());

	for (size_t i = 0; i < bindings.size(); i++)
	{
		layoutEntries[i] = WGPUBindGroupLayoutEntry{
			.binding = bindings[i].binding,
			.visibility = TranslateShaderStageFlags(bindings[i].shaderAccess),
		};

		std::visit([&](const auto& type) { ProcessLayoutEntry(layoutEntries[i], type); }, bindings[i].type);

		// The bindings passed in are guaranteed to be sorted by DescriptorSetLayoutCache
		result->activeBindingIndicesSorted.push_back(bindings[i].binding);
	}

	const WGPUBindGroupLayoutDescriptor descriptor = {
		.entryCount = layoutEntries.size(),
		.entries = layoutEntries.data(),
	};
	result->bindGroupLayout = wgpuDeviceCreateBindGroupLayout(wgpuctx.device, &descriptor);

	return result;
}

static DescriptorSetLayoutCache descriptorSetLayoutCache{ &CreateCachedDescriptorSetLayout };

const CachedBindGroupLayout& GetBindGroupLayout(std::span<const DescriptorSetBinding> bindings)
{
	return static_cast<CachedBindGroupLayout&>(descriptorSetLayoutCache.Get(bindings, {}));
}

void ClearBindGroupLayoutCache()
{
	descriptorSetLayoutCache.Clear();
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
			OnFrameEnd([bindGroupCopy = bindGroup] { wgpuBindGroupRelease(bindGroupCopy); });
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

		entriesNotBoundMask.reset(ToUnsigned(entryIt - entries.begin()));
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

void BindSamplerDS(SamplerHandle sampler, DescriptorSetHandle set, uint32_t binding)
{
	DescriptorSet::Unwrap(set).SetBinding(WGPUBindGroupEntry{
		.binding = binding,
		.sampler = UnwrapSampler(sampler),
	});
}

void BindTextureDS(TextureViewHandle textureView, DescriptorSetHandle set, uint32_t binding, TextureUsage usage)
{
	DescriptorSet::Unwrap(set).SetBinding(WGPUBindGroupEntry{
		.binding = binding,
		.textureView = UnwrapTextureView(textureView),
	});
}

void BindStorageImageDS(TextureViewHandle textureView, DescriptorSetHandle set, uint32_t binding)
{
	DescriptorSet::Unwrap(set).SetBinding(WGPUBindGroupEntry{
		.binding = binding,
		.textureView = UnwrapTextureView(textureView),
	});
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
	BindBufferDS(
		handle, set, binding, offset,
		range.has_value() ? std::optional<uint64_t>(RoundToNextMultiple(*range, 16)) : std::nullopt);
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
