#include <Metal/Metal.hpp>
#include <QuartzCore/CAMetalDrawable.hpp>

namespace eg::graphics_api::mtl
{
void MetalLayerInit(void* metalLayerVP, MTL::Device* metalDevice, bool useSRGB);

CA::MetalDrawable* GetNextDrawable();
} // namespace eg::graphics_api::mtl
