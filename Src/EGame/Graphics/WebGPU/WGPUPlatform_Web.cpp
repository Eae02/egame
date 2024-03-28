#ifdef __EMSCRIPTEN__

#include "WGPUPlatform.hpp"

namespace eg::graphics_api::webgpu
{
WGPUInstance PlatformInit(const GraphicsAPIInitArguments& initArguments)
{
	return wgpuCreateInstance(nullptr);
}

static std::optional<bool> isMaybeAvailable;

bool IsMaybeAvailable()
{
	if (!isMaybeAvailable.has_value())
	{
		isMaybeAvailable = EM_ASM_INT({ return !!navigator.gpu; });
	}
	return *isMaybeAvailable;
}
} // namespace eg::graphics_api::webgpu

#endif
