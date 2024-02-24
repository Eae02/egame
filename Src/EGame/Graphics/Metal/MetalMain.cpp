#include "MetalMain.hpp"
#include "CAMetalLayer.hpp"
#include "MetalCommandContext.hpp"

#include <Metal/MTLDevice.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/CAMetalDrawable.hpp>

#include <SDL.h>

namespace eg::graphics_api::mtl
{
MTL::Device* metalDevice;
MTL::CommandQueue* mainCommandQueue;

static SDL_MetalView metalView;

static std::string deviceName;

static NS::AutoreleasePool* globalAutoreleasePool;

static SDL_Window* sdlWindow;

static NS::AutoreleasePool* frameAutoreleasePool;
static MTL::CommandBuffer* frameCommandBuffer;

static dispatch_semaphore_t frameSemaphore;

bool Initialize(const GraphicsAPIInitArguments& initArguments)
{
	globalAutoreleasePool = NS::AutoreleasePool::alloc()->init();

	metalDevice = MTL::CreateSystemDefaultDevice();

	sdlWindow = initArguments.window;

	deviceName = metalDevice->name()->utf8String();

	metalView = SDL_Metal_CreateView(initArguments.window);
	MetalLayerInit(SDL_Metal_GetLayer(metalView), metalDevice, initArguments.defaultFramebufferSRGB);

	mainCommandQueue = metalDevice->newCommandQueue();

	frameSemaphore = dispatch_semaphore_create(MAX_CONCURRENT_FRAMES);

	frameAutoreleasePool = NS::AutoreleasePool::alloc()->init();
	frameCommandBuffer = mainCommandQueue->commandBuffer();
	MetalCommandContext::main = MetalCommandContext(frameCommandBuffer);

	return true;
}

void Shutdown()
{
	mainCommandQueue->release();
	metalDevice->release();
	globalAutoreleasePool->release();
}

void GetDrawableSize(int& width, int& height)
{
	SDL_Metal_GetDrawableSize(sdlWindow, &width, &height);
}

std::span<std::string> GetDeviceNames()
{
	return { &deviceName, 1 };
}

void GetDeviceInfo(GraphicsDeviceInfo& deviceInfo)
{
	auto maxThreadsPerThreadgroup = metalDevice->maxThreadsPerThreadgroup();

	DeviceFeatureFlags features = DeviceFeatureFlags::ComputeShader | DeviceFeatureFlags::TextureCubeMapArray |
	                              DeviceFeatureFlags::DynamicResourceBind |
	                              DeviceFeatureFlags::ConcurrentResourceCreation |
	                              DeviceFeatureFlags::PartialTextureViews | DeviceFeatureFlags::DeferredContext;

	if (metalDevice->supportsBCTextureCompression())
		features |= DeviceFeatureFlags::TextureCompressionBC;

	deviceInfo = GraphicsDeviceInfo{
		.uniformBufferOffsetAlignment = 4,
		.storageBufferOffsetAlignment = 4,
		.maxTessellationPatchSize = 0,
		.maxClipDistances = 0,
		.maxComputeWorkGroupSize = { static_cast<uint32_t>(maxThreadsPerThreadgroup.width),
		                             static_cast<uint32_t>(maxThreadsPerThreadgroup.height),
		                             static_cast<uint32_t>(maxThreadsPerThreadgroup.depth), },
		.maxComputeWorkGroupCount = { UINT32_MAX, UINT32_MAX, UINT32_MAX },
		.maxComputeWorkGroupInvocations = 1024, //?
		.depthRange = DepthRange::ZeroToOne,
		.features = features,
		.timerTicksPerNS = 1.0f,
		.deviceName = deviceName,
		.deviceVendorName = "Apple",
	};
}

void EndLoading()
{
	MetalCommandContext::main.FlushBlitCommands();
	MetalCommandContext::main.FlushComputeCommands();
	MetalCommandContext::main.Commit();
	frameAutoreleasePool->release();
}

bool IsLoadingComplete()
{
	return true;
}

CA::MetalDrawable* frameDrawable;

void BeginFrame()
{
	frameAutoreleasePool = NS::AutoreleasePool::alloc()->init();

	frameDrawable = GetNextDrawable();

	frameCommandBuffer = mainCommandQueue->commandBuffer();

	dispatch_semaphore_wait(frameSemaphore, DISPATCH_TIME_FOREVER);
	frameCommandBuffer->addCompletedHandler(^void(MTL::CommandBuffer* pCmd) {
	  dispatch_semaphore_signal(frameSemaphore);
	});

	MetalCommandContext::main = MetalCommandContext(frameCommandBuffer);
}

void EndFrame()
{
	MetalCommandContext::main.FlushBlitCommands();
	MetalCommandContext::main.FlushComputeCommands();

	frameCommandBuffer->presentDrawable(frameDrawable);

	MetalCommandContext::main.Commit();

	frameAutoreleasePool->release();
}

void DeviceWaitIdle() {}

void DebugLabelBegin(CommandContextHandle ctx, const char* label, const float* color)
{
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(ctx);
	mcc.m_commandBuffer->pushDebugGroup(NS::String::alloc()->init(label, NS::UTF8StringEncoding));
}

void DebugLabelEnd(CommandContextHandle ctx)
{
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(ctx);
	mcc.m_commandBuffer->popDebugGroup();
}

void DebugLabelInsert(CommandContextHandle ctx, const char* label, const float* color) {}
} // namespace eg::graphics_api::mtl
