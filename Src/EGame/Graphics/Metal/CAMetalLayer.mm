#include "CAMetalLayer.hpp"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "MetalTranslation.hpp"

namespace eg::graphics_api::mtl
{
static CAMetalLayer* metalLayer;

void MetalLayerInit(void* metalLayerVP, MTL::Device* metalDevice, bool useSRGB)
{
	metalLayer = reinterpret_cast<CAMetalLayer*>(metalLayerVP);
	metalLayer.device = (__bridge id<MTLDevice>)metalDevice;
	if (useSRGB)
		metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
	else
		metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;

	defaultColorPixelFormat = static_cast<MTL::PixelFormat>(metalLayer.pixelFormat);
}

CA::MetalDrawable* GetNextDrawable()
{
	return (__bridge CA::MetalDrawable*)[metalLayer nextDrawable];
}

void SetEnableVSync(bool enableVSync)
{
	metalLayer.displaySyncEnabled = enableVSync;
}
} // namespace eg::graphics_api::mtl
