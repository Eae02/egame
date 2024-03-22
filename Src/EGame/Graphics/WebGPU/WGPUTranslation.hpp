#pragma once

#include "../Abstraction.hpp"
#include "WGPU.hpp"

namespace eg::graphics_api::webgpu
{
WGPUTextureFormat TranslateTextureFormat(Format format, bool undefinedIfUnsupported = false);
WGPUVertexFormat TranslateVertexFormat(Format format, bool undefinedIfUnsupported = false);

WGPUTextureViewDimension TranslateTextureViewType(TextureViewType viewType);

WGPUCompareFunction TranslateCompareOp(CompareOp compareOp);

WGPUShaderStageFlags TranslateShaderStageFlags(ShaderAccessFlags flags);

WGPUCullMode TranslateCullMode(CullMode cullMode);
} // namespace eg::graphics_api::webgpu
