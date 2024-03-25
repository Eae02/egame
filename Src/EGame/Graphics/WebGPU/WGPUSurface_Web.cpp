#ifdef __EMSCRIPTEN__

#include "../../Utils.hpp"
#include "WGPUSurface.hpp"

#include <emscripten.h>

namespace eg::graphics_api::webgpu
{
WGPUSurface CreateSurface(WGPUInstance instance, SDL_Window* window)
{
	WGPUSurfaceDescriptorFromCanvasHTMLSelector surfaceDescriptorCanvas = {
		.chain = { .sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector },
		.selector = "canvas",
	};

	WGPUSurfaceDescriptor surfaceDesc = {
		.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&surfaceDescriptorCanvas),
	};

	return wgpuInstanceCreateSurface(instance, &surfaceDesc);
}

std::pair<uint32_t, uint32_t> GetWindowDrawableSize(SDL_Window* window)
{
	int width, height;
	emscripten_get_screen_size(&width, &height);
	return { ToUnsigned(width), ToUnsigned(height) };
}
} // namespace eg::graphics_api::webgpu

#endif
