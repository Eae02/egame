#pragma once

#include "../Abstraction.hpp"
#include "WGPU.hpp"

namespace eg::graphics_api::webgpu
{
WGPUInstance PlatformInit(const GraphicsAPIInitArguments& initArguments);

extern bool platformIsLoadingComplete;

#ifdef __EMSCRIPTEN__
void StartWebMainLoop(void(*runFrame)());
#endif
}
