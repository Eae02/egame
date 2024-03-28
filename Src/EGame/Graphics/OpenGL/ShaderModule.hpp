#pragma once

#include "OpenGL.hpp"
#include "Utils.hpp"

#include <spirv_cross.hpp>
#include <spirv_glsl.hpp>

namespace eg::graphics_api::gl
{
using SPIRType = spirv_cross::SPIRType;

struct ShaderModule
{
	ShaderStage stage;
	const spirv_cross::ParsedIR* parsedIR;
};

inline ShaderModule* UnwrapShaderModule(ShaderModuleHandle handle)
{
	return reinterpret_cast<ShaderModule*>(handle);
}
} // namespace eg::graphics_api::gl
