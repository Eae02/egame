#pragma once

#include "../Abstraction.hpp"
#include "WGPU.hpp"

namespace eg::graphics_api::webgpu
{
WGPUTextureFormat TranslateTextureFormat(Format format);

WGPUCompareFunction TranslateCompareOp(CompareOp compareOp);
} // namespace eg::graphics_api::webgpu
