#pragma once

#include <webgpu.h>

#include <functional>

namespace eg::graphics_api::webgpu
{
struct WebGPUCtx
{
	WGPUInstance instance;
	WGPUAdapter adapter;
	WGPUSurface surface;
	WGPUDevice device;
	WGPUQueue queue;

	WGPUSwapChain swapchain = nullptr;
	WGPUPresentMode swapchainPresentMode;
	uint32_t swapchainImageWidth = 0;
	uint32_t swapchainImageHeight = 0;
	WGPUTextureFormat swapchainFormat;
	WGPUTextureView currentSwapchainColorView;

	WGPUTextureFormat defaultColorFormat;

	WGPUTexture srgbEmulationColorTexture;
	WGPUTextureView srgbEmulationColorTextureView;
};

extern WebGPUCtx wgpuctx;

bool IsDeviceFeatureEnabled(WGPUFeatureName feature);

void OnFrameEnd(std::function<void()> callback);
void RunFrameEndCallbacks();

#define XM_WGPU_FUNC(F) extern WGPUProc##F wgpu##F;
#include "WGPUFunctions.inl"
#undef XM_WGPU_FUNC

#ifdef __EMSCRIPTEN__
inline void wgpuDeviceTick(WGPUDevice);
#endif
} // namespace eg::graphics_api::webgpu
