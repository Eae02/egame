#include "WGPUMain.hpp"
#include "../../Platform/DynamicLibrary.hpp"
#include "WGPU.hpp"
#include "WGPUCommandContext.hpp"

#include <SDL2/SDL_metal.h>

namespace eg::graphics_api::webgpu
{
DynamicLibrary dawnLibrary;

static std::vector<WGPUFeatureName> adapterFeatures;
static WGPUSupportedLimits adapterLimits;

static std::string deviceName;
static std::string apiName;

static SDL_Window* sdlWindow;

void OnAdapterRequestEnded(WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* pUserData)
{
	if (status == WGPURequestAdapterStatus_Success)
	{
		wgpuctx.adapter = adapter;
	}
	else
	{
		std::cout << "Could not get WebGPU adapter: " << message << std::endl;
	}
	*static_cast<bool*>(pUserData) = true;
}

void OnDeviceRequestEnded(WGPURequestDeviceStatus status, WGPUDevice device, const char* message, void* pUserData)
{
	if (status == WGPURequestDeviceStatus_Success)
	{
		wgpuctx.device = device;
	}
	else
	{
		std::cout << "Could not get WebGPU device: " << message << std::endl;
	}
	*static_cast<bool*>(pUserData) = true;
}

WGPUSurface CreateSurface(WGPUInstance instance, SDL_Window* window);

bool Initialize(const GraphicsAPIInitArguments& initArguments)
{
	std::string dawnLibraryName = DynamicLibrary::PlatformFormat("webgpu_dawn");
	if (!dawnLibrary.Open(dawnLibraryName.c_str()))
	{
		eg::Log(
			eg::LogLevel::Error, "wgpu", "Failed to load dawn library for webgpu ({0}): {1}", dawnLibraryName,
			dawnLibrary.FailureReason());
		return false;
	}

#define XM_WGPU_FUNC(F) wgpu##F = reinterpret_cast<WGPUProc##F>(dawnLibrary.GetSymbol("wgpu" #F));
#include "WGPUFunctions.inl"
#undef XM_WGPU_FUNC

	const WGPUInstanceDescriptor instanceDesc = {};
	wgpuctx.instance = wgpuCreateInstance(&instanceDesc);

	if (wgpuctx.instance == nullptr)
	{
		eg::Log(eg::LogLevel::Error, "wgpu", "Failed to create webgpu instance");
		return false;
	}

	wgpuctx.surface = CreateSurface(wgpuctx.instance, initArguments.window);

	const WGPURequestAdapterOptions requestAdapterOptions = { .compatibleSurface = wgpuctx.surface };
	bool adapterRequestDone = false;
	wgpuInstanceRequestAdapter(wgpuctx.instance, &requestAdapterOptions, OnAdapterRequestEnded, &adapterRequestDone);
	while (!adapterRequestDone)
	{
	}

	size_t numAdapterFeatures = wgpuAdapterEnumerateFeatures(wgpuctx.adapter, nullptr);
	adapterFeatures.resize(numAdapterFeatures);
	wgpuAdapterEnumerateFeatures(wgpuctx.adapter, adapterFeatures.data());

	wgpuAdapterGetLimits(wgpuctx.adapter, &adapterLimits);

	WGPUAdapterProperties adapterProperties = {};
	wgpuAdapterGetProperties(wgpuctx.adapter, &adapterProperties);
	deviceName = adapterProperties.name;

	apiName = "WebGPU";
	switch (adapterProperties.backendType)
	{
	case WGPUBackendType_D3D11: apiName += "/D3D11"; break;
	case WGPUBackendType_D3D12: apiName += "/D3D12"; break;
	case WGPUBackendType_Metal: apiName += "/Metal"; break;
	case WGPUBackendType_Vulkan: apiName += "/Vulkan"; break;
	case WGPUBackendType_OpenGL: apiName += "/OpenGL"; break;
	case WGPUBackendType_OpenGLES: apiName += "/OpenGLES"; break;
	default: break;
	}

	eg::Log(eg::LogLevel::Info, "webgpu", "Initializing WebGPU using device: {0}", adapterProperties.name);

	const WGPUDeviceDescriptor deviceDesc = {};
	bool deviceRequestDone = false;
	wgpuAdapterRequestDevice(wgpuctx.adapter, &deviceDesc, OnDeviceRequestEnded, &deviceRequestDone);
	while (!deviceRequestDone)
	{
	}

	wgpuctx.queue = wgpuDeviceGetQueue(wgpuctx.device);

	wgpuctx.swapchainFormat = wgpuSurfaceGetPreferredFormat(wgpuctx.surface, wgpuctx.adapter);

	int swapchainWidth, swapchainHeight;
	SDL_Metal_GetDrawableSize(initArguments.window, &swapchainWidth, &swapchainHeight);

	const WGPUSwapChainDescriptor swapChainDesc = {
		.width = static_cast<uint32_t>(swapchainWidth),
		.height = static_cast<uint32_t>(swapchainHeight),
		.usage = WGPUTextureUsage_RenderAttachment,
		.presentMode = WGPUPresentMode_Fifo,
		.format = wgpuctx.swapchainFormat,
	};
	wgpuctx.swapchain = wgpuDeviceCreateSwapChain(wgpuctx.device, wgpuctx.surface, &swapChainDesc);

	sdlWindow = initArguments.window;

	CommandContext::main.BeginEncode();

	return true;
}

static bool HasAdapterFeature(WGPUFeatureName feature)
{
	return Contains(adapterFeatures, feature);
}

void GetDeviceInfo(GraphicsDeviceInfo& deviceInfo)
{
	DeviceFeatureFlags features = DeviceFeatureFlags::ComputeShaderAndSSBO | DeviceFeatureFlags::PartialTextureViews;

	if (HasAdapterFeature(WGPUFeatureName_TextureCompressionBC))
		features |= DeviceFeatureFlags::TextureCompressionBC;
	if (HasAdapterFeature(WGPUFeatureName_TextureCompressionASTC))
		features |= DeviceFeatureFlags::TextureCompressionASTC;

	deviceInfo = {
		.uniformBufferOffsetAlignment = adapterLimits.limits.minUniformBufferOffsetAlignment,
		.storageBufferOffsetAlignment = adapterLimits.limits.minStorageBufferOffsetAlignment,
		.maxComputeWorkGroupSize[0] = adapterLimits.limits.maxComputeWorkgroupSizeX,
		.maxComputeWorkGroupSize[1] = adapterLimits.limits.maxComputeWorkgroupSizeY,
		.maxComputeWorkGroupSize[2] = adapterLimits.limits.maxComputeWorkgroupSizeZ,
		.maxComputeWorkGroupInvocations = adapterLimits.limits.maxComputeInvocationsPerWorkgroup,
		.depthRange = eg::DepthRange::ZeroToOne,
		.features = features,
		.timerTicksPerNS = 1.0f,
		.deviceName = deviceName,
		.apiName = apiName,
	};
}

std::span<std::string> GetDeviceNames()
{
	return { &deviceName, 1 };
}

void GetDrawableSize(int& width, int& height)
{
	SDL_Metal_GetDrawableSize(sdlWindow, &width, &height);
}

FormatCapabilities GetFormatCapabilities(Format format)
{
	EG_PANIC("Unimplemented")
}

static Fence* loadingFence;

void EndLoading()
{
	CommandContext::main.EndEncode();
	wgpuQueueSubmit(wgpuctx.queue, 1, &CommandContext::main.commandBuffer);
	loadingFence = Fence::CreateAndInsert();
}

bool IsLoadingComplete()
{
	return loadingFence->IsDone();
}

static std::array<Fence*, MAX_CONCURRENT_FRAMES> frameFences;

void BeginFrame()
{
	if (frameFences[CFrameIdx()] != nullptr)
	{
		frameFences[CFrameIdx()]->Wait();
		frameFences[CFrameIdx()]->Deref();
	}

	CommandContext::main.BeginEncode();
}

void EndFrame()
{
	CommandContext::main.EndEncode();
	wgpuQueueSubmit(wgpuctx.queue, 1, &CommandContext::main.commandBuffer);
	frameFences[CFrameIdx()] = Fence::CreateAndInsert();
}

void Shutdown() {}

void SetEnableVSync(bool enableVSync) {}

void DeviceWaitIdle() {}
} // namespace eg::graphics_api::webgpu
