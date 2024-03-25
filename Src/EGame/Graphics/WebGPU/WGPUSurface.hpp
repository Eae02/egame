#include "WGPU.hpp"

struct SDL_Window;

namespace eg::graphics_api::webgpu
{
WGPUSurface CreateSurface(WGPUInstance instance, SDL_Window* window);

std::pair<uint32_t, uint32_t> GetWindowDrawableSize(SDL_Window* window);
} // namespace eg::graphics_api::webgpu
