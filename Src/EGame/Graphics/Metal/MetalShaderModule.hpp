#pragma once

#include "../SpirvCrossFwd.hpp"
#include "../Abstraction.hpp"

namespace eg::graphics_api::mtl
{
struct ShaderModule
{
	ShaderStage stage;
	const spirv_cross::ParsedIR* parsedIR;

	static ShaderModule* Unwrap(ShaderModuleHandle handle) { return reinterpret_cast<ShaderModule*>(handle); }
};
} // namespace eg::graphics_api::mtl
