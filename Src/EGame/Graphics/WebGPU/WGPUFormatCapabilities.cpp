#include "../Format.hpp"
#include "WGPU.hpp"
#include "WGPUTranslation.hpp"

#include <unordered_set>

namespace eg::graphics_api::webgpu
{
static const std::unordered_set<Format> FORMATS_FILTER_LINEAR = {
	Format::R8_UNorm,          Format::R8G8_UNorm,       Format::R8G8_SNorm,      Format::R8G8B8A8_UNorm,
	Format::R8G8B8A8_sRGB,     Format::R16_Float,        Format::R16G16_Float,    Format::R16G16B16A16_Float,
	Format::A2R10G10B10_UNorm, Format::B10G11R11_UFloat,

	Format::BC1_RGBA_UNorm,    Format::BC1_RGBA_sRGB,    Format::BC3_RGBA_UNorm,  Format::BC3_RGBA_sRGB,
	Format::BC4_R_UNorm,       Format::BC5_RG_UNorm,     Format::BC6H_RGB_UFloat, Format::BC6H_RGB_Float,
	Format::BC7_RGBA_UNorm,    Format::BC7_RGBA_sRGB,
};

static const std::unordered_set<Format> FORMATS_RENDER_ATTACHMENT = {
	Format::R8_UNorm,           Format::R8_UInt,           Format::R8_SInt,
	Format::R8G8_UNorm,         Format::R8G8_UInt,         Format::R8G8_SInt,
	Format::R8G8B8A8_UNorm,     Format::R8G8B8A8_sRGB,     Format::R8G8B8A8_UInt,
	Format::R8G8B8A8_SInt,      Format::R16_UInt,          Format::R16_SInt,
	Format::R16_Float,          Format::R16G16_UInt,       Format::R16G16_SInt,
	Format::R16G16_Float,       Format::R16G16B16A16_UInt, Format::R16G16B16A16_SInt,
	Format::R16G16B16A16_Float, Format::R32_UInt,          Format::R32_SInt,
	Format::R32_Float,          Format::R32G32_UInt,       Format::R32G32_SInt,
	Format::R32G32_Float,       Format::R32G32B32A32_UInt, Format::R32G32B32A32_SInt,
	Format::R32G32B32A32_Float, Format::A2R10G10B10_UInt,  Format::A2R10G10B10_UNorm,
};

static const std::unordered_set<Format> FORMATS_BLENDABLE = {
	Format::R8_UNorm,  Format::R8G8_UNorm,   Format::R8G8B8A8_UNorm,     Format::R8G8B8A8_sRGB,
	Format::R16_Float, Format::R16G16_Float, Format::R16G16B16A16_Float, Format::A2R10G10B10_UNorm,
};

static const std::unordered_set<Format> FORMATS_STORAGE_IMAGE = {
	Format::R32_UInt,
	Format::R32_SInt,
	Format::R32_Float,
};

FormatCapabilities GetFormatCapabilities(Format format)
{
	FormatCapabilities capabilities{};

	if (TranslateTextureFormat(format, true) != WGPUTextureFormat_Undefined)
	{
		capabilities |= FormatCapabilities::SampledImage;

		if (FORMATS_FILTER_LINEAR.contains(format))
			capabilities |= FormatCapabilities::SampledImageFilterLinear;

		if (FORMATS_STORAGE_IMAGE.contains(format))
			capabilities |= FormatCapabilities::StorageImage | FormatCapabilities::StorageImageAtomic;

		if (FORMATS_RENDER_ATTACHMENT.contains(format))
			capabilities |= FormatCapabilities::ColorAttachment;
		if (FORMATS_BLENDABLE.contains(format))
			capabilities |= FormatCapabilities::ColorAttachmentBlend;

		if ((format == Format::R32_Float || format == Format::R32G32_Float || format == Format::R32G32B32A32_Float) &&
		    IsDeviceFeatureEnabled(WGPUFeatureName_Float32Filterable))
		{
			capabilities |= FormatCapabilities::SampledImageFilterLinear;
		}

		if (format == Format::B10G11R11_UFloat && IsDeviceFeatureEnabled(WGPUFeatureName_RG11B10UfloatRenderable))
		{
			capabilities |= FormatCapabilities::ColorAttachment | FormatCapabilities::ColorAttachmentBlend;
		}

		if (GetFormatType(format) == FormatType::DepthStencil)
		{
			capabilities |= FormatCapabilities::DepthStencilAttachment;
		}
	}

	if (TranslateVertexFormat(format, true) != WGPUVertexFormat_Undefined)
		capabilities |= FormatCapabilities::VertexAttribute;

	return capabilities;
}
} // namespace eg::graphics_api::webgpu
