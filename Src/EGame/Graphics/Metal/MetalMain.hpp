#pragma once

#include "../Abstraction.hpp"

#include <Metal/Metal.hpp>
#include <QuartzCore/CAMetalLayer.hpp>

namespace eg::graphics_api::mtl
{
extern MTL::Device* metalDevice;
extern MTL::CommandQueue* mainCommandQueue;

extern CA::MetalDrawable* frameDrawable;

bool Initialize(const GraphicsAPIInitArguments& initArguments);

#define XM_ABSCALLBACK(name, ret, params) ret name params;
#include "../AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
} // namespace eg::graphics_api::mtl
