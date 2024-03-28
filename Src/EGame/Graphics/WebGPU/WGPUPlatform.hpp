#pragma once

#include "../Abstraction.hpp"
#include "WGPU.hpp"

namespace eg::graphics_api::webgpu
{
WGPUInstance PlatformInit(const GraphicsAPIInitArguments& initArguments);

bool IsMaybeAvailable();
} // namespace eg::graphics_api::webgpu
