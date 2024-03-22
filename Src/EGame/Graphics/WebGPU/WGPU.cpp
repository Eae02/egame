#include "WGPU.hpp"

namespace eg::graphics_api::webgpu
{
WebGPUCtx wgpuctx;

#define XM_WGPU_FUNC(F) WGPUProc##F wgpu##F;
#include "WGPUFunctions.inl"
#undef XM_WGPU_FUNC

static std::vector<std::function<void()>> frameEndCallbacks;

void OnFrameEnd(std::function<void()> callback)
{
	frameEndCallbacks.push_back(std::move(callback));
}

void RunFrameEndCallbacks()
{
	for (const auto& callback : frameEndCallbacks)
		callback();
	frameEndCallbacks.clear();
}
} // namespace eg::graphics_api::webgpu
