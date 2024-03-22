#ifndef __APPLE__

#include "../../Utils.hpp"
#include "WGPU.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

namespace eg::graphics_api::webgpu
{
WGPUSurface CreateSurface(WGPUInstance instance, SDL_Window* window)
{
	SDL_SysWMinfo windowWMInfo;
	SDL_VERSION(&windowWMInfo.version);
	SDL_GetWindowWMInfo(window, &windowWMInfo);

	void* surfaceDescNext = nullptr;

#ifdef SDL_VIDEO_DRIVER_WAYLAND
	WGPUSurfaceDescriptorFromWaylandSurface surfaceDescWayland;
	if (windowWMInfo.subsystem == SDL_SYSWM_WAYLAND)
	{
		surfaceDescWayland = {
			.chain = { .sType = WGPUSType_SurfaceDescriptorFromWaylandSurface },
			.display = windowWMInfo.info.wl.display,
			.surface = windowWMInfo.info.wl.surface,
		};
		surfaceDescNext = &surfaceDescWayland;
	}
#endif

#ifdef SDL_VIDEO_DRIVER_X11
	WGPUSurfaceDescriptorFromXlibWindow surfaceDescXlib;
	if (windowWMInfo.subsystem == SDL_SYSWM_X11)
	{
		surfaceDescXlib = {
			.chain = { .sType = WGPUSType_SurfaceDescriptorFromXlibWindow },
			.display = windowWMInfo.info.x11.display,
			.window = windowWMInfo.info.x11.window,
		};
		surfaceDescNext = &surfaceDescXlib;
	}
#endif

	WGPUSurfaceDescriptor surfaceDesc = {
		.nextInChain = static_cast<WGPUChainedStruct*>(surfaceDescNext),
	};

	return wgpuInstanceCreateSurface(instance, &surfaceDesc);
}

std::pair<uint32_t, uint32_t> GetWindowDrawableSize(SDL_Window* window)
{
	int width, height;
	SDL_GetWindowSizeInPixels(window, &width, &height);
	return { ToUnsigned(width), ToUnsigned(height) };
}
} // namespace eg::graphics_api::webgpu

#endif
