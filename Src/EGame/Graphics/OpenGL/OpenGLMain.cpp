#include "OpenGL.hpp"
#include "Utils.hpp"
#include "PipelineGraphics.hpp"
#include "PlatformSpecific.hpp"
#include "Framebuffer.hpp"
#include "../../Alloc/ObjectPool.hpp"

#include <sstream>
#include <bitset>

#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF

namespace eg::graphics_api::gl
{
	extern int maxAnistropy;
	
	static DepthRange depthRange = DepthRange::NegOneToOne;
	
	std::span<std::string> GetDeviceNames()
	{
		return { &rendererName, 1 };
	}
	
	bool Initialize(const GraphicsAPIInitArguments& initArguments)
	{
		std::vector<const char*> requiredExtensions;
		if (!InitializeGLPlatformSpecific(initArguments, requiredExtensions))
			return false;
		
		for (const char* ext : requiredExtensions)
		{
			if (!IsExtensionSupported(ext))
			{
				std::ostringstream messageStream;
				messageStream << "Required OpenGL extension " << ext << " is not supported by your graphics driver.";
				std::string message = messageStream.str();
				std::cout << message << std::endl;
				return false;
			}
		}
		
		depthRange = initArguments.forceDepthZeroToOne ? DepthRange::ZeroToOne : DepthRange::NegOneToOne;
		
		if (initArguments.defaultDepthStencilFormat == Format::Depth32 ||
		    initArguments.defaultDepthStencilFormat == Format::Depth16)
		{
			defaultFramebufferHasDepth = true;
			defaultFramebufferHasStencil = false;
		}
		
		if (initArguments.defaultDepthStencilFormat == Format::Depth24Stencil8 ||
		    initArguments.defaultDepthStencilFormat == Format::Depth32Stencil8)
		{
			defaultFramebufferHasDepth = true;
			defaultFramebufferHasStencil = true;
		}
		
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		
		GLuint vao;
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		
		float maxAnistropyF;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnistropyF);
		maxAnistropy = (int)maxAnistropyF;
		
		vendorName = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
		if (strstr(vendorName.c_str(), "Intel"))
			glVendor = GLVendor::Intel;
		else if (strstr(vendorName.c_str(), "NVIDIA"))
			glVendor = GLVendor::Nvidia;
		else
			glVendor = GLVendor::Unknown;
		
		rendererName = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
		
		Log(LogLevel::Info, "gl", "Using OpenGL renderer: '{0}', by vendor: '{1}'", rendererName, vendorName);
		
		return true;
	}
	
	void GetDeviceInfo(GraphicsDeviceInfo& deviceInfo)
	{
		deviceInfo.uniformBufferOffsetAlignment = (uint32_t)GetIntegerLimit(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT);
		deviceInfo.geometryShader               = true;
		deviceInfo.concurrentResourceCreation   = false;
		deviceInfo.depthRange                   = depthRange;
		deviceInfo.timerTicksPerNS              = 1.0f;
		deviceInfo.deviceName                   = rendererName;
		deviceInfo.deviceVendorName             = vendorName;
		deviceInfo.maxMSAA                      = (uint32_t)GetIntegerLimit(GL_MAX_SAMPLES);
		
		PlatformSpecificGetDeviceInfo(deviceInfo);
	}
	
	void BeginFrame()
	{
		GetDrawableSize(drawableWidth, drawableHeight);
		UpdateSRGBEmulationTexture(drawableWidth, drawableHeight);
		
		viewportOutOfDate = true;
		scissorOutOfDate = true;
		hasWrittenToBackBuffer = false;
		
		PlatformSpecificBeginFrame();
	}
	
	void EndFrame()
	{
		SRGBEmulationEndFrame();
		PlatformSpecificEndFrame();
	}
	
	void DeviceWaitIdle()
	{
		glFinish();
	}
	
	void DebugLabelBegin(CommandContextHandle, const char* label, const float* color)
	{
#ifndef __EMSCRIPTEN__
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, label);
#endif
	}
	
	void DebugLabelEnd(CommandContextHandle)
	{
#ifndef __EMSCRIPTEN__
		glPopDebugGroup();
#endif
	}
	
	void DebugLabelInsert(CommandContextHandle, const char* label, const float* color)
	{
#ifndef __EMSCRIPTEN__
		glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER, 0, GL_DEBUG_SEVERITY_NOTIFICATION, -1, label);
#endif
	}
}
