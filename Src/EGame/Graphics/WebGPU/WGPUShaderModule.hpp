#pragma once

#include "WGPU.hpp"

#include "../SpirvCrossUtils.hpp"

namespace eg::graphics_api::webgpu
{
struct ShaderModule
{
	static void InitializeTint();

	WGPUShaderModule shaderModule;

	DescriptorSetBindings bindings;
};
} // namespace eg::graphics_api::webgpu
