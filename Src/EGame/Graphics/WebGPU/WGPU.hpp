#pragma once

#include <webgpu.h>

namespace eg::graphics_api::webgpu
{
struct WebGPUCtx
{
	WGPUInstance instance;
	WGPUAdapter adapter;
	WGPUSurface surface;
	WGPUDevice device;
	WGPUQueue queue;

	WGPUSwapChain swapchain;
	WGPUTextureFormat swapchainFormat;
};

extern WebGPUCtx wgpuctx;

#define XM_WGPU_FUNC(F) extern WGPUProc##F wgpu##F;
#include "WGPUFunctions.inl"
#undef XM_WGPU_FUNC
} // namespace eg::graphics_api::webgpu
