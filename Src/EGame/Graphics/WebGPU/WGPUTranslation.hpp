#pragma once

#include "../Abstraction.hpp"
#include "WGPU.hpp"

namespace eg::graphics_api::webgpu
{
WGPUTextureFormat TranslateTextureFormat(Format format);
WGPUVertexFormat TranslateVertexFormat(Format format);

WGPUCompareFunction TranslateCompareOp(CompareOp compareOp);

WGPUShaderStageFlags TranslateShaderStageFlags(ShaderAccessFlags flags);

WGPUCullMode TranslateCullMode(CullMode cullMode);
} // namespace eg::graphics_api::webgpu
