#pragma once

#ifndef EG_NO_VULKAN

#include "../Abstraction.hpp"

namespace eg::graphics_api::vk
{
bool EarlyInitializeMemoized();

bool Initialize(const GraphicsAPIInitArguments& initArguments);

GraphicsMemoryStat GetMemoryStat();

void MaybeAcquireSwapchainImage();

#define XM_ABSCALLBACK(name, ret, params) ret name params;
#include "../AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
} // namespace eg::graphics_api::vk

#endif
