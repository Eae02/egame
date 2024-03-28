#ifndef __EMSCRIPTEN__

#include "../../Platform/DynamicLibrary.hpp"
#include "WGPUPlatform.hpp"

namespace eg::graphics_api::webgpu
{
#define XM_WGPU_FUNC(F) WGPUProc##F wgpu##F;
#include "WGPUNativeFunctions.inl"
#undef XM_WGPU_FUNC

static DynamicLibrary dawnLibrary;

WGPUInstance PlatformInit(const GraphicsAPIInitArguments& initArguments)
{
	std::string dawnLibraryName = DynamicLibrary::PlatformFormat("webgpu_dawn");
	if (!dawnLibrary.Open(dawnLibraryName.c_str()))
	{
		eg::Log(
			eg::LogLevel::Error, "wgpu", "Failed to load dawn library for webgpu ({0}): {1}", dawnLibraryName,
			dawnLibrary.FailureReason());
		return nullptr;
	}

#define XM_WGPU_FUNC(F) wgpu##F = reinterpret_cast<WGPUProc##F>(dawnLibrary.GetSymbol("wgpu" #F));
#include "WGPUNativeFunctions.inl"
	XM_WGPU_FUNC(InstanceWaitAny)
#undef XM_WGPU_FUNC

	WGPUInstanceDescriptor instanceDesc = {};
	instanceDesc.features.timedWaitAnyEnable = true;
	return wgpuCreateInstance(&instanceDesc);
}

bool IsMaybeAvailable()
{
	return true;
}
} // namespace eg::graphics_api::webgpu

#endif
