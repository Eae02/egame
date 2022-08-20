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
		maxAnistropy = static_cast<int>(maxAnistropyF);
		
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
		deviceInfo.uniformBufferOffsetAlignment = ToUnsigned(GetIntegerLimit(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT));
		deviceInfo.geometryShader               = true;
		deviceInfo.concurrentResourceCreation   = false;
		deviceInfo.depthRange                   = depthRange;
		deviceInfo.timerTicksPerNS              = 1.0f;
		deviceInfo.deviceName                   = rendererName;
		deviceInfo.deviceVendorName             = vendorName;
		deviceInfo.maxMSAA                      = ToUnsigned(GetIntegerLimit(GL_MAX_SAMPLES));
		
		PlatformSpecificGetDeviceInfo(deviceInfo);
	}
	
	FormatCapabilities GetFormatCapabilities(Format format)
	{
		FormatCapabilities capabilities = { };
		
		if (TranslateFormatForVertexAttribute(format, true).size != 0)
			capabilities |= FormatCapabilities::VertexAttribute;
		
		if (GLenum textureFormat = TranslateFormatForTexture(format, true))
		{
#ifdef EG_GLES
			switch (GetFormatType(format))
			{
			case FormatTypes::DepthStencil:
				capabilities |= FormatCapabilities::SampledImage | FormatCapabilities::DepthStencilAttachment;
				break;
			case FormatTypes::Float:
				capabilities |= FormatCapabilities::SampledImage;
				if (glesFormatSupport.floatColorBuffer)
					capabilities |= FormatCapabilities::ColorAttachment;
				if (glesFormatSupport.floatLinearFiltering)
					capabilities |= FormatCapabilities::SampledImageFilterLinear;
				if (glesFormatSupport.floatBlend)
					capabilities |= FormatCapabilities::ColorAttachmentBlend;
				break;
			default:
				bool supported = true;
				if (IsCompressedFormat(format))
				{
					supported = supported && glesFormatSupport.compressedS3TC;
					if (IsSRGBFormat(format))
						supported = supported && glesFormatSupport.compressedS3TCSRGB;
				}
				if (supported)
				{
					capabilities |= FormatCapabilities::SampledImage | FormatCapabilities::SampledImageFilterLinear |
						FormatCapabilities::ColorAttachment | FormatCapabilities::ColorAttachmentBlend;
				}
				break;
			}
#else
			auto GetFormatParameter = [&] (GLenum pname, GLenum target = GL_TEXTURE_2D)
			{
				GLint value;
				glGetInternalformativ(target, textureFormat, pname, sizeof(value), &value);
				return value;
			};
			
			capabilities |= FormatCapabilities::SampledImage;
			
			if (GetFormatParameter(GL_FILTER))
				capabilities |= FormatCapabilities::SampledImageFilterLinear;
			
			if (GetFormatParameter(GL_FRAMEBUFFER_RENDERABLE))
			{
				if (GetFormatParameter(GL_COLOR_RENDERABLE))
					capabilities |= FormatCapabilities::ColorAttachment;
				if (GetFormatParameter(GL_DEPTH_RENDERABLE))
					capabilities |= FormatCapabilities::DepthStencilAttachment;
				if (GetFormatParameter(GL_FRAMEBUFFER_BLEND))
					capabilities |= FormatCapabilities::ColorAttachmentBlend;
			}
			
			if (GetFormatParameter(GL_SHADER_IMAGE_LOAD) && GetFormatParameter(GL_SHADER_IMAGE_STORE))
			{
				capabilities |= FormatCapabilities::StorageImage;
				if (GetFormatParameter(GL_SHADER_IMAGE_ATOMIC))
					capabilities |= FormatCapabilities::StorageImageAtomic;
			}
#endif
		}
		
		return capabilities;
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
