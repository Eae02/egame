#pragma once

#include "../Abstraction.hpp"
#include "../DescriptorSetLayoutCache.hpp"
#include "WGPU.hpp"

#include <vector>

namespace eg::graphics_api::webgpu
{
struct CachedBindGroupLayout : public ICachedDescriptorSetLayout
{
	CachedBindGroupLayout() = default;

	~CachedBindGroupLayout();

	CachedBindGroupLayout(CachedBindGroupLayout&&) = delete;
	CachedBindGroupLayout(const CachedBindGroupLayout&) = delete;
	CachedBindGroupLayout& operator=(CachedBindGroupLayout&&) = delete;
	CachedBindGroupLayout& operator=(const CachedBindGroupLayout&) = delete;

	WGPUBindGroupLayout bindGroupLayout;
	std::vector<uint32_t> activeBindingIndicesSorted;
};

const CachedBindGroupLayout& GetBindGroupLayout(std::span<const DescriptorSetBinding> bindings);

void ClearBindGroupLayoutCache();
} // namespace eg::graphics_api::webgpu
