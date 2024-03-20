#include "../Abstraction.hpp"
#include "Framebuffer.hpp"
#include "OpenGL.hpp"
#include "PipelineGraphics.hpp"
#include "PlatformSpecific.hpp"
#include "Utils.hpp"

#include <sstream>
#include <vector>

#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
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

	const char* vendorName = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
	if (strstr(vendorName, "Intel"))
		glVendor = GLVendor::Intel;
	else if (strstr(vendorName, "NVIDIA"))
		glVendor = GLVendor::Nvidia;
	else
		glVendor = GLVendor::Unknown;

	rendererName = reinterpret_cast<const char*>(glGetString(GL_RENDERER));

	Log(LogLevel::Info, "gl", "Using OpenGL renderer: '{0}', by vendor: '{1}'", rendererName, vendorName);

	return true;
}

void GetDeviceInfo(GraphicsDeviceInfo& deviceInfo)
{
	deviceInfo = GraphicsDeviceInfo{
		.uniformBufferOffsetAlignment = ToUnsigned(GetIntegerLimit(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT)),
		.depthRange = depthRange,
		.features = DeviceFeatureFlags::GeometryShader | DeviceFeatureFlags::DynamicResourceBind,
		.timerTicksPerNS = 1.0f,
		.deviceName = rendererName,
	};

	PlatformSpecificGetDeviceInfo(deviceInfo);
}

FormatCapabilities GetFormatCapabilities(Format format)
{
	FormatCapabilities capabilities = {};

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
		default: bool supported = true;
#ifndef __APPLE__
			if (IsCompressedFormat(format))
			{
				supported = supported && glesFormatSupport.compressedS3TC;
				if (IsSRGBFormat(format))
					supported = supported && glesFormatSupport.compressedS3TCSRGB;
			}
#endif
			if (supported)
			{
				capabilities |= FormatCapabilities::SampledImage | FormatCapabilities::SampledImageFilterLinear |
				                FormatCapabilities::ColorAttachment | FormatCapabilities::ColorAttachmentBlend;
			}
			break;
		}
#else
		auto GetFormatParameter = [&](GLenum pname, GLenum target = GL_TEXTURE_2D)
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
#ifndef EG_GLES
	if (glPushDebugGroup != nullptr)
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, label);
#endif
}

void DebugLabelEnd(CommandContextHandle)
{
#ifndef EG_GLES
	if (glPopDebugGroup != nullptr)
		glPopDebugGroup();
#endif
}

void DebugLabelInsert(CommandContextHandle, const char* label, const float* color)
{
#ifndef EG_GLES
	if (glDebugMessageInsert != nullptr)
		glDebugMessageInsert(
			GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER, 0, GL_DEBUG_SEVERITY_NOTIFICATION, -1, label);
#endif
}

// clang-format off
CommandContextHandle CreateCommandContext(Queue) { EG_PANIC("unsupported") }
void DestroyCommandContext(CommandContextHandle) { EG_PANIC("unsupported") }
void BeginRecordingCommandContext(CommandContextHandle, CommandContextBeginFlags) { EG_PANIC("unsupported") }
void FinishRecordingCommandContext(CommandContextHandle context) { EG_PANIC("unsupported") }
void SubmitCommandContext(CommandContextHandle context, const CommandContextSubmitArgs& args) { EG_PANIC("unsupported") }
FenceHandle CreateFence() { EG_PANIC("unsupported") }
void DestroyFence(FenceHandle) { EG_PANIC("unsupported") }
FenceStatus WaitForFence(FenceHandle, uint64_t) { EG_PANIC("unsupported") }
// clang-format on
} // namespace eg::graphics_api::gl
