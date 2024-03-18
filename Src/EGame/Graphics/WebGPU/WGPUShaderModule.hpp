#pragma once

#include "WGPU.hpp"

namespace eg::graphics_api::webgpu
{
struct ShaderModule
{
	static void InitializeTint();

	WGPUShaderModule shaderModule;
};
} // namespace eg::graphics_api::webgpu
