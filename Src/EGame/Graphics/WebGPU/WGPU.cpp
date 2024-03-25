#include "WGPU.hpp"

namespace eg::graphics_api::webgpu
{
WGPUProcInstanceWaitAny wgpuInstanceWaitAny = nullptr;

WebGPUCtx wgpuctx;

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
