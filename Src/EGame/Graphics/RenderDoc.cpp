#include "RenderDoc.hpp"
#include "../Log.hpp"

#include <dlfcn.h>
#include <renderdoc.h>

namespace eg::renderdoc
{
#if defined(__linux__)
	static const char* renderDocLibName = "librenderdoc.so";
#elif defined(_WIN32)
	static const char* renderDocLibName = "renderdoc.dll";
#else
	static const char* renderDocLibName = nullptr;
#endif
	
	static RENDERDOC_API_1_0_0* renderDocAPI = nullptr;
	
	void Init()
	{
		if (renderDocLibName == nullptr)
			return;
		
		void* renderDocLibrary = dlopen(renderDocLibName, RTLD_NOLOAD | RTLD_NOW);
		if (renderDocLibrary == nullptr)
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
