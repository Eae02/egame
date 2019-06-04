#include "RenderDoc.hpp"
#include "../Log.hpp"

#ifdef __EMSCRIPTEN__
namespace eg::renderdoc
{
	void Init() { }
	bool IsPresent() { return false; }
	void CaptureNextFrame() { }
	void StartCapture() { }
	void EndCapture() { }
}
#else

#include <renderdoc.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define dlsym GetProcAddress
#elif defined(__linux__)
#include <dlfcn.h>
#endif

namespace eg::renderdoc
{
	static RENDERDOC_API_1_0_0* renderDocAPI = nullptr;
	
	void Init()
	{
#if defined(_WIN32)
		HMODULE renderDocLibrary = GetModuleHandleA("renderdoc.dll");
#elif defined(__linux__)
		void* renderDocLibrary = dlopen("librenderdoc.so", RTLD_NOLOAD | RTLD_NOW);
#else
		void* renderDocLibrary = nullptr;
#endif
		
		if (!renderDocLibrary)
			return;
		
		pRENDERDOC_GetAPI GetAPI = reinterpret_cast<pRENDERDOC_GetAPI>(dlsym(renderDocLibrary, "RENDERDOC_GetAPI"));
		if (GetAPI == nullptr)
			return;
		
		if (GetAPI(eRENDERDOC_API_Version_1_0_0, reinterpret_cast<void**>(&renderDocAPI)))
			Log(LogLevel::Info, "gfx", "RenderDoc API loaded");
		else
			Log(LogLevel::Error, "gfx", "RenderDoc API failed to load");
	}
	
	bool IsPresent()
	{
		return renderDocAPI != nullptr;
	}
	
	void CaptureNextFrame()
	{
		if (renderDocAPI)
			renderDocAPI->TriggerCapture();
	}
	
	void StartCapture()
	{
		if (renderDocAPI)
			renderDocAPI->StartFrameCapture(nullptr, nullptr);
	}
	
	void EndCapture()
	{
		if (renderDocAPI)
			renderDocAPI->EndFrameCapture(nullptr, nullptr);
	}
}

#endif
