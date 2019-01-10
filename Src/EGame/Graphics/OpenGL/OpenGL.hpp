#pragma once

#include "../Abstraction.hpp"

namespace eg::graphics_api::gl
{
	bool Initialize(SDL_Window* window);
	
#define XM_ABSCALLBACK(name, ret, params) ret name params;
#include "../AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
}
