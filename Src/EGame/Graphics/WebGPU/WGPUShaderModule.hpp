#pragma once

#include "WGPU.hpp"

#include "../SpirvCrossUtils.hpp"

namespace eg::graphics_api::webgpu
{
struct ShaderModuleOptDeleter
{
	bool shouldRelease;

	void operator()(WGPUShaderModule shaderModule) const;
};

struct ShaderModule
{
	static void InitializeTint();

	WGPUShaderModule shaderModule = nullptr;

	std::vector<uint32_t> spirvForLateCompile;

	DescriptorSetBindings bindings;

	std::string label;

	std::unique_ptr<WGPUShaderModuleImpl, ShaderModuleOptDeleter> GetSpecializedShaderModule(
		std::span<const SpecializationConstantEntry> specConstantEntries) const;
};

WGPUShaderModule CreateShaderModuleFromSpirV(std::span<const uint32_t> spirv, const char* label);
} // namespace eg::graphics_api::webgpu
