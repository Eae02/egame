#pragma once

#ifdef __EMSCRIPTEN__
#include <webgpu/webgpu.h>
#else
#include <webgpu_native/webgpu.h>
#define WGPU_SKIP_DECLARATIONS
#endif

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

extern WGPUProcInstanceWaitAny wgpuInstanceWaitAny;

#ifdef __EMSCRIPTEN__
inline void wgpuDeviceTick(WGPUDevice) {}
inline WGPUFuture wgpuQueueOnSubmittedWorkDoneF(WGPUQueue, WGPUQueueWorkDoneCallbackInfo)
{
	return {};
}
inline void wgpuDeviceSetDeviceLostCallback(WGPUDevice, void (*)(WGPUDeviceLostReason, const char*, void*), void*) {}
#else
#define XM_WGPU_FUNC(F) extern WGPUProc##F wgpu##F;
#include "WGPUNativeFunctions.inl"
#undef XM_WGPU_FUNC
#endif
} // namespace eg::graphics_api::webgpu
