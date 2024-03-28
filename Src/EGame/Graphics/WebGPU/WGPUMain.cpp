#include "WGPUMain.hpp"
#include "WGPU.hpp"
#include "WGPUCommandContext.hpp"
#include "WGPUDescriptorSet.hpp"
#include "WGPUFence.hpp"
#include "WGPUPlatform.hpp"
#include "WGPUSurface.hpp"
#include "WGPUTint.hpp"

#include <vector>

namespace eg::graphics_api::webgpu
{
static WGPUSupportedLimits adapterLimits;

static std::string deviceName;
static std::string apiName;

static SDL_Window* sdlWindow;

static bool enableSrgbEmulation;

static inline const char* GetMessage(const char* message)
{
	return message ? message : "(no message)";
}

static inline std::string_view WGPUErrorTypeToString(WGPUErrorType type)
{
	switch (type)
	{
	case WGPUErrorType_NoError: return "NoError";
	case WGPUErrorType_Validation: return "Validation";
	case WGPUErrorType_OutOfMemory: return "OutOfMemory";
	case WGPUErrorType_Internal: return "Internal";
	case WGPUErrorType_Unknown: return "Unknown";
	case WGPUErrorType_DeviceLost: return "DeviceLost";
	default: return "?";
	}
}

static void OnDeviceError(WGPUErrorType type, const char* message, void* userData)
{
	Log(LogLevel::Error, "webgpu", "WebGPU Device Error [{0}]: {1}", WGPUErrorTypeToString(type), GetMessage(message));
	EG_DEBUG_BREAK;
}

static void OnDeviceLost(WGPUDeviceLostReason reason, const char* message, void* userData)
{
	if (reason != WGPUDeviceLostReason_Destroyed)
	{
		EG_PANIC("WebGPU Device Lost: " << GetMessage(message));
	}
}

static WGPUPresentMode presentMode = WGPUPresentMode_Fifo;

static void UpdateSwapchain()
{
	auto [width, height] = GetWindowDrawableSize(sdlWindow);
	if (width == wgpuctx.swapchainImageWidth && height == wgpuctx.swapchainImageHeight &&
	    wgpuctx.swapchainPresentMode == presentMode)
	{
		return;
	}

	wgpuctx.swapchainImageWidth = width;
	wgpuctx.swapchainImageHeight = height;
	wgpuctx.swapchainPresentMode = presentMode;

	if (wgpuctx.swapchain != nullptr)
		wgpuSwapChainRelease(wgpuctx.swapchain);

	const WGPUSwapChainDescriptor swapChainDesc = {
		.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopyDst,
		.format = wgpuctx.swapchainFormat,
		.width = width,
		.height = height,
		.presentMode = presentMode,
	};
	wgpuctx.swapchain = wgpuDeviceCreateSwapChain(wgpuctx.device, wgpuctx.surface, &swapChainDesc);

	if (enableSrgbEmulation)
	{
		if (wgpuctx.srgbEmulationColorTextureView)
			wgpuTextureViewRelease(wgpuctx.srgbEmulationColorTextureView);
		if (wgpuctx.srgbEmulationColorTexture)
			wgpuTextureRelease(wgpuctx.srgbEmulationColorTexture);

		const WGPUTextureDescriptor textureDesc = {
			.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc,
			.dimension = WGPUTextureDimension_2D,
			.size = { .width = width, .height = height, .depthOrArrayLayers = 1 },
			.format = wgpuctx.defaultColorFormat,
			.mipLevelCount = 1,
			.sampleCount = 1,
			.viewFormatCount = 1,
			.viewFormats = &wgpuctx.defaultColorFormat,
		};

		wgpuctx.srgbEmulationColorTexture = wgpuDeviceCreateTexture(wgpuctx.device, &textureDesc);

		const WGPUTextureViewDescriptor viewDesc = {
			.format = wgpuctx.defaultColorFormat,
			.dimension = WGPUTextureViewDimension_2D,
			.mipLevelCount = 1,
			.arrayLayerCount = 1,
			.aspect = WGPUTextureAspect_All,
		};

		wgpuctx.srgbEmulationColorTextureView = wgpuTextureCreateView(wgpuctx.srgbEmulationColorTexture, &viewDesc);
	}
}

static const WGPUFeatureName WANTED_DEVICE_FEATURES[] = {
	WGPUFeatureName_DepthClipControl,     WGPUFeatureName_Float32Filterable,
	WGPUFeatureName_TextureCompressionBC, WGPUFeatureName_TextureCompressionASTC,
	WGPUFeatureName_TimestampQuery,       WGPUFeatureName_RG11B10UfloatRenderable,

#ifndef __EMSCRIPTEN__
	WGPUFeatureName_SurfaceCapabilities,  WGPUFeatureName_TransientAttachments,
#endif
};

static std::vector<WGPUFeatureName> enabledDeviceFeatures;

bool IsDeviceFeatureEnabled(WGPUFeatureName feature)
{
	return SortedContains(enabledDeviceFeatures, feature);
}

static std::pair<WGPUBackendType, std::string_view> BACKEND_NAMES[] = {
	{ WGPUBackendType_D3D11, "D3D11" },   { WGPUBackendType_D3D12, "D3D12" },
	{ WGPUBackendType_Metal, "Metal" },   { WGPUBackendType_Vulkan, "Vulkan" },
	{ WGPUBackendType_OpenGL, "OpenGL" }, { WGPUBackendType_OpenGLES, "OpenGLES" },
};

static bool requestedDefaultFramebufferSRGB;

static std::function<void()> initCompleteCallback;

static void OnDeviceRequestEnded(
	WGPURequestDeviceStatus status, WGPUDevice device, const char* message, void* pUserData)
{
	if (status != WGPURequestDeviceStatus_Success)
		EG_PANIC("Could not get WebGPU device: " << message);

	wgpuctx.device = device;

	wgpuDeviceSetUncapturedErrorCallback(wgpuctx.device, OnDeviceError, nullptr);
	wgpuDeviceSetDeviceLostCallback(wgpuctx.device, OnDeviceLost, nullptr);

	wgpuctx.queue = wgpuDeviceGetQueue(wgpuctx.device);

	wgpuctx.swapchainFormat = wgpuSurfaceGetPreferredFormat(wgpuctx.surface, wgpuctx.adapter);

	static const WGPUTextureFormat EXPECTED_SWAPCHAIN_FORMATS[] = {
		WGPUTextureFormat_BGRA8Unorm,
		WGPUTextureFormat_RGBA8Unorm,
		WGPUTextureFormat_BGRA8UnormSrgb,
		WGPUTextureFormat_RGBA8UnormSrgb,
	};
	if (!Contains(EXPECTED_SWAPCHAIN_FORMATS, wgpuctx.swapchainFormat))
	{
		eg::Log(eg::LogLevel::Warning, "webgpu", "Unexpected swapchain format: {0}", wgpuctx.swapchainFormat);
		wgpuctx.swapchainFormat = WGPUTextureFormat_BGRA8Unorm;
	}

	if (requestedDefaultFramebufferSRGB && wgpuctx.swapchainFormat != WGPUTextureFormat_BGRA8UnormSrgb &&
	    wgpuctx.swapchainFormat != WGPUTextureFormat_RGBA8UnormSrgb)
	{
		eg::Log(eg::LogLevel::Warning, "webgpu", "Using sRGB emulation of default framebuffer");
		enableSrgbEmulation = true;

		if (wgpuctx.swapchainFormat == WGPUTextureFormat_BGRA8Unorm)
			wgpuctx.defaultColorFormat = WGPUTextureFormat_BGRA8UnormSrgb;
		else
			wgpuctx.defaultColorFormat = WGPUTextureFormat_RGBA8UnormSrgb;
	}
	else
	{
		wgpuctx.defaultColorFormat = wgpuctx.swapchainFormat;
	}

	UpdateSwapchain();

	CommandContext::main.BeginEncode();

	if (initCompleteCallback)
		initCompleteCallback();
}

static void OnAdapterRequestEnded(
	WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* pUserData)
{
	if (status != WGPURequestAdapterStatus_Success)
		EG_PANIC("Could not get WebGPU adapter: " << message);

	wgpuctx.adapter = adapter;

	size_t numAdapterFeatures = wgpuAdapterEnumerateFeatures(wgpuctx.adapter, nullptr);
	std::vector<WGPUFeatureName> adapterFeatures(numAdapterFeatures);
	wgpuAdapterEnumerateFeatures(wgpuctx.adapter, adapterFeatures.data());
	std::sort(adapterFeatures.begin(), adapterFeatures.end());

#ifdef __EMSCRIPTEN__
	adapterLimits.limits.minUniformBufferOffsetAlignment = 256;
	adapterLimits.limits.minStorageBufferOffsetAlignment = 256;
	adapterLimits.limits.maxComputeInvocationsPerWorkgroup = 256;
	adapterLimits.limits.maxComputeWorkgroupSizeX = 256;
	adapterLimits.limits.maxComputeWorkgroupSizeY = 256;
	adapterLimits.limits.maxComputeWorkgroupSizeZ = 64;
#else
	wgpuAdapterGetLimits(wgpuctx.adapter, &adapterLimits);
#endif

	WGPUAdapterProperties adapterProperties = {};
	wgpuAdapterGetProperties(wgpuctx.adapter, &adapterProperties);
	deviceName = adapterProperties.name;

	apiName = "WebGPU";
	for (auto [backend, backendName] : BACKEND_NAMES)
	{
		if (adapterProperties.backendType == backend)
		{
			apiName += "/";
			apiName += backendName;
			break;
		}
	}

	eg::Log(eg::LogLevel::Info, "webgpu", "Initializing WebGPU using device: {0}", adapterProperties.name);

	for (WGPUFeatureName feature : WANTED_DEVICE_FEATURES)
	{
		if (SortedContains(adapterFeatures, feature))
		{
			enabledDeviceFeatures.push_back(feature);
		}
	}
	std::sort(enabledDeviceFeatures.begin(), enabledDeviceFeatures.end());

	const WGPURequiredLimits requiredLimits = {
		.limits = {
			.minUniformBufferOffsetAlignment = adapterLimits.limits.minUniformBufferOffsetAlignment,
			.minStorageBufferOffsetAlignment = adapterLimits.limits.minStorageBufferOffsetAlignment,
			.maxComputeInvocationsPerWorkgroup = adapterLimits.limits.maxComputeInvocationsPerWorkgroup,
			.maxComputeWorkgroupSizeX = adapterLimits.limits.maxComputeWorkgroupSizeX,
			.maxComputeWorkgroupSizeY = adapterLimits.limits.maxComputeWorkgroupSizeY,
			.maxComputeWorkgroupSizeZ = adapterLimits.limits.maxComputeWorkgroupSizeZ,
		},
	};

	const WGPUDeviceDescriptor deviceDesc = {
		.requiredFeatureCount = enabledDeviceFeatures.size(),
		.requiredFeatures = enabledDeviceFeatures.data(),
		.requiredLimits = &requiredLimits,
	};
	wgpuAdapterRequestDevice(wgpuctx.adapter, &deviceDesc, OnDeviceRequestEnded, nullptr);
}

bool Initialize(const GraphicsAPIInitArguments& initArguments)
{
	wgpuctx.instance = PlatformInit(initArguments);

	if (wgpuctx.instance == nullptr)
		return false;

	InitializeTint();

	wgpuctx.surface = CreateSurface(wgpuctx.instance, initArguments.window);

	WGPUBackendType preferredBackend = WGPUBackendType_Undefined;
	if (const char* preferredBackendName = std::getenv("EG_WEBGPU_BACKEND"))
	{
		for (auto [backend, backendName] : BACKEND_NAMES)
		{
			if (std::string_view(preferredBackendName) == backendName)
				preferredBackend = backend;
		}
	}

	sdlWindow = initArguments.window;
	requestedDefaultFramebufferSRGB = initArguments.defaultFramebufferSRGB;

	const WGPURequestAdapterOptions requestAdapterOptions = {
		.compatibleSurface = wgpuctx.surface,
		.backendType = preferredBackend,
	};

	static_assert(sizeof(void*) == sizeof(void (*)()));

	initCompleteCallback = std::move(initArguments.initDoneCallback);

	wgpuInstanceRequestAdapter(wgpuctx.instance, &requestAdapterOptions, OnAdapterRequestEnded, nullptr);

#ifndef __EMSCRIPTEN__
	// If we are not running in emscripten, all the callbacks should have been run by now
	EG_ASSERT(wgpuctx.device != nullptr);
#endif

	return true;
}

void GetDeviceInfo(GraphicsDeviceInfo& deviceInfo)
{
	DeviceFeatureFlags features = DeviceFeatureFlags::ComputeShaderAndSSBO | DeviceFeatureFlags::PartialTextureViews;

	if (IsDeviceFeatureEnabled(WGPUFeatureName_TextureCompressionBC))
		features |= DeviceFeatureFlags::TextureCompressionBC;
	if (IsDeviceFeatureEnabled(WGPUFeatureName_TextureCompressionASTC))
		features |= DeviceFeatureFlags::TextureCompressionASTC;

	deviceInfo = {
		.uniformBufferOffsetAlignment = adapterLimits.limits.minUniformBufferOffsetAlignment,
		.storageBufferOffsetAlignment = adapterLimits.limits.minStorageBufferOffsetAlignment,
		.maxComputeWorkGroupSize = {
			adapterLimits.limits.maxComputeWorkgroupSizeX,
			adapterLimits.limits.maxComputeWorkgroupSizeY,
			adapterLimits.limits.maxComputeWorkgroupSizeZ,
		},
		.maxComputeWorkGroupInvocations = adapterLimits.limits.maxComputeInvocationsPerWorkgroup,
		.textureBufferCopyStrideAlignment = 256,
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
	width = ToInt(wgpuctx.swapchainImageWidth);
	height = ToInt(wgpuctx.swapchainImageHeight);
}

static Fence* loadingFence;

void EndLoading()
{
	CommandContext::main.EndEncode();
	wgpuQueueSubmit(wgpuctx.queue, 1, &CommandContext::main.commandBuffer);
	loadingFence = Fence::CreateAndInsert();
	RunFrameEndCallbacks();
}

bool IsLoadingComplete()
{
	return loadingFence == nullptr || loadingFence->IsDone();
}

static std::array<Fence*, MAX_CONCURRENT_FRAMES> frameFences;

void BeginFrame()
{
	UpdateSwapchain();

	if (frameFences[CFrameIdx()] != nullptr)
	{
		frameFences[CFrameIdx()]->Wait();
		frameFences[CFrameIdx()]->Deref();
	}

	if (!enableSrgbEmulation)
	{
		wgpuctx.currentSwapchainColorView = wgpuSwapChainGetCurrentTextureView(wgpuctx.swapchain);
	}

	CommandContext::main.BeginEncode();
}

void EndFrame()
{
	if (enableSrgbEmulation)
	{
		WGPUTexture swapchainTexture = wgpuSwapChainGetCurrentTexture(wgpuctx.swapchain);

		uint32_t swapchainTextureWidth = wgpuTextureGetWidth(swapchainTexture);
		uint32_t swapchainTextureHeight = wgpuTextureGetHeight(swapchainTexture);

		const WGPUImageCopyTexture srcCopy = {
			.texture = wgpuctx.srgbEmulationColorTexture,
			.aspect = WGPUTextureAspect_All,
		};
		const WGPUImageCopyTexture dstCopy = {
			.texture = swapchainTexture,
			.aspect = WGPUTextureAspect_All,
		};
		const WGPUExtent3D copyExtent = {
			.width = std::min(wgpuctx.swapchainImageWidth, swapchainTextureWidth),
			.height = std::min(wgpuctx.swapchainImageHeight, swapchainTextureHeight),
			.depthOrArrayLayers = 1,
		};

		wgpuCommandEncoderCopyTextureToTexture(CommandContext::main.encoder, &srcCopy, &dstCopy, &copyExtent);
	}

	CommandContext::main.EndEncode();
	CommandContext::main.Submit();
	frameFences[CFrameIdx()] = Fence::CreateAndInsert();

	if (!enableSrgbEmulation)
	{
		wgpuTextureViewRelease(wgpuctx.currentSwapchainColorView);
	}

#ifndef __EMSCRIPTEN__
	wgpuSwapChainPresent(wgpuctx.swapchain);
#endif

	RunFrameEndCallbacks();
}

void SetEnableVSync(bool enableVSync)
{
	if (enableVSync)
		presentMode = WGPUPresentMode_Fifo;
	else
		presentMode = WGPUPresentMode_Immediate;
}

void Shutdown()
{
	ClearBindGroupLayoutCache();
	RunFrameEndCallbacks();

	wgpuSwapChainRelease(wgpuctx.swapchain);
	wgpuQueueRelease(wgpuctx.queue);
	wgpuDeviceRelease(wgpuctx.device);
	wgpuAdapterRelease(wgpuctx.adapter);
	wgpuSurfaceRelease(wgpuctx.surface);
	wgpuInstanceRelease(wgpuctx.instance);
}

void DeviceWaitIdle() {}
} // namespace eg::graphics_api::webgpu
