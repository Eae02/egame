#pragma once

#include "../Abstraction.hpp"

#include <future>

namespace eg::graphics_api::webgpu
{

bool Initialize(const GraphicsAPIInitArguments& initArguments);

#define XM_ABSCALLBACK(name, ret, params) ret name params;
#include "../AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
} // namespace eg::graphics_api::webgpu
