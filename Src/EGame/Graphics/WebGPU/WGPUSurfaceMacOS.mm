#if defined(EG_ENABLE_WEBGPU) && defined(__APPLE__)

#include "WGPU.hpp"

#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>
#include <QuartzCore/CAMetalLayer.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

namespace eg::graphics_api::webgpu
{
WGPUSurface CreateSurface(WGPUInstance instance, SDL_Window* window)
{
	SDL_SysWMinfo windowWMInfo;
	SDL_VERSION(&windowWMInfo.version);
	SDL_GetWindowWMInfo(window, &windowWMInfo);

	id metal_layer = NULL;
	NSWindow* ns_window = windowWMInfo.info.cocoa.window;
	[ns_window.contentView setWantsLayer:YES];
	metal_layer = [CAMetalLayer layer];
	[ns_window.contentView setLayer:metal_layer];

	WGPUSurfaceDescriptorFromMetalLayer surfaceDescFromMetalLayer = {
		.chain = { .sType = WGPUSType_SurfaceDescriptorFromMetalLayer },
		.layer = metal_layer,
	};

	WGPUSurfaceDescriptor surfaceDesc = {
		.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&surfaceDescFromMetalLayer),
	};

	return wgpuInstanceCreateSurface(instance, &surfaceDesc);
}
} // namespace eg::graphics_api::webgpu

#endif
