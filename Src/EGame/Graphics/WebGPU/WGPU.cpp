#include "WGPU.hpp"

namespace eg::graphics_api::webgpu
{
WebGPUCtx wgpuctx;

#define XM_WGPU_FUNC(F) WGPUProc##F wgpu##F;
#include "WGPUFunctions.inl"
#undef XM_WGPU_FUNC
} // namespace eg::graphics_api::webgpu
