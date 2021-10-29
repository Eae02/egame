#pragma once

#include "../Abstraction.hpp"

namespace eg::graphics_api::gl
{
	bool InitializeGLPlatformSpecific(const GraphicsAPIInitArguments& initArguments, std::vector<const char*>& requiredExtensions);
	
	bool IsExtensionSupported(const char* name);
	
	void PlatformSpecificGetDeviceInfo(GraphicsDeviceInfo& deviceInfo);
	
	void PlatformSpecificBeginFrame();
}
