#pragma once

#ifndef EG_NO_VULKAN

#include "../Abstraction.hpp"

namespace eg::graphics_api::vk
{
	bool Initialize(const GraphicsAPIInitArguments& initArguments);
	
#define XM_ABSCALLBACK(name, ret, params) ret name params;
#include "../AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
}

#endif
