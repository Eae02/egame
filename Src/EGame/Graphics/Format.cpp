#include "Format.hpp"
#include "../Assert.hpp"
#include "../Utils.hpp"

namespace eg
{
FormatTypes GetFormatType(Format format)
{
	switch (format)
	{
	case Format::Undefined:
	case Format::DefaultColor:
		break;
	case Format::R8_UNorm:
	case Format::R8G8_UNorm:
	case Format::R8G8B8A8_UNorm:
	case Format::R8G8B8A8_sRGB:
	case Format::R8G8B8_sRGB:
	case Format::R16_UNorm:
	case Format::R16G16_UNorm:
	case Format::R8G8B8_UNorm:
	case Format::R16G16B16_UNorm:
	case Format::R16G16B16A16_UNorm:
	case Format::BC1_RGBA_UNorm:
	case Format::BC1_RGBA_sRGB:
	case Format::BC1_RGB_UNorm:
	case Format::BC1_RGB_sRGB:
	case Format::BC3_UNorm:
	case Format::BC3_sRGB:
	case Format::BC4_UNorm:
	case Format::BC5_UNorm:
	case Format::A2R10G10B10_UNorm:
		return FormatTypes::UNorm;
	case Format::R8_SNorm:
	case Format::R16_SNorm:
	case Format::R8G8_SNorm:
	case Format::R16G16_SNorm:
	case Format::R8G8B8_SNorm:
	case Format::R16G16B16_SNorm:
	case Format::R8G8B8A8_SNorm:
	case Format::R16G16B16A16_SNorm:
	case Format::A2R10G10B10_SNorm:
		return FormatTypes::SNorm;
	case Format::R8_UInt:
	case Format::R16_UInt:
	case Format::R32_UInt:
	case Format::R8G8_UInt:
	case Format::R16G16_UInt:
	case Format::R32G32_UInt:
	case Format::R8G8B8_UInt:
	case Format::R16G16B16_UInt:
	case Format::R32G32B32_UInt:
	case Format::R8G8B8A8_UInt:
	case Format::R16G16B16A16_UInt:
	case Format::R32G32B32A32_UInt:
	case Format::A2R10G10B10_UInt:
		return FormatTypes::UInt;
	case Format::R16_Float:
	case Format::R32_Float:
	case Format::R16G16_Float:
	case Format::R32G32_Float:
	case Format::R16G16B16_Float:
	case Format::R32G32B32_Float:
	case Format::R16G16B16A16_Float:
	case Format::R32G32B32A32_Float:
		return FormatTypes::Float;
	case Format::R8_SInt:
	case Format::R16_SInt:
	case Format::R32_SInt:
	case Format::R8G8_SInt:
	case Format::R16G16_SInt:
	case Format::R32G32_SInt:
	case Format::R8G8B8_SInt:
	case Format::R16G16B16_SInt:
	case Format::R32G32B32_SInt:
	case Format::R8G8B8A8_SInt:
	case Format::R16G16B16A16_SInt:
	case Format::R32G32B32A32_SInt:
	case Format::A2R10G10B10_SInt:
		return FormatTypes::SInt;
	case Format::Depth16:
	case Format::Depth32:
	case Format::Depth24Stencil8:
	case Format::Depth32Stencil8:
	case Format::DefaultDepthStencil:
		return FormatTypes::DepthStencil;
	}

	EG_UNREACHABLE
}

int GetFormatComponentCount(Format format)
{
	switch (format)
	{
	case Format::Undefined:
	case Format::DefaultColor:
	case Format::DefaultDepthStencil:
		return 0;
	case Format::R8_SNorm:
	case Format::R8_UNorm:
	case Format::R8_UInt:
	case Format::R8_SInt:
	case Format::R16_UNorm:
	case Format::R16_SNorm:
	case Format::R16_UInt:
	case Format::R16_SInt:
	case Format::R16_Float:
	case Format::R32_UInt:
	case Format::R32_SInt:
	case Format::R32_Float:
	case Format::BC4_UNorm:
	case Format::Depth16:
	case Format::Depth32:
	case Format::Depth24Stencil8:
	case Format::Depth32Stencil8:
		return 1;
	case Format::R8G8_UNorm:
	case Format::R8G8_SNorm:
	case Format::R8G8_UInt:
	case Format::R8G8_SInt:
	case Format::R16G16_UNorm:
	case Format::R16G16_SNorm:
	case Format::R16G16_UInt:
	case Format::R16G16_SInt:
	case Format::R16G16_Float:
	case Format::R32G32_UInt:
	case Format::R32G32_SInt:
	case Format::R32G32_Float:
	case Format::BC5_UNorm:
		return 2;
	case Format::R8G8B8_sRGB:
	case Format::R8G8B8_UNorm:
	case Format::R8G8B8_SNorm:
	case Format::R8G8B8_UInt:
	case Format::R8G8B8_SInt:
	case Format::R16G16B16_UNorm:
	case Format::R16G16B16_SNorm:
	case Format::R16G16B16_UInt:
	case Format::R16G16B16_SInt:
	case Format::R16G16B16_Float:
	case Format::R32G32B32_UInt:
	case Format::R32G32B32_SInt:
	case Format::R32G32B32_Float:
	case Format::BC1_RGB_UNorm:
	case Format::BC1_RGB_sRGB:
		return 3;
	case Format::R8G8B8A8_sRGB:
	case Format::R8G8B8A8_SNorm:
	case Format::R8G8B8A8_UNorm:
	case Format::R8G8B8A8_UInt:
	case Format::R8G8B8A8_SInt:
	case Format::R16G16B16A16_UNorm:
	case Format::R16G16B16A16_SNorm:
	case Format::R16G16B16A16_UInt:
	case Format::R16G16B16A16_SInt:
	case Format::R16G16B16A16_Float:
	case Format::R32G32B32A32_UInt:
	case Format::R32G32B32A32_SInt:
	case Format::R32G32B32A32_Float:
	case Format::BC1_RGBA_UNorm:
	case Format::BC1_RGBA_sRGB:
	case Format::BC3_UNorm:
	case Format::BC3_sRGB:
	case Format::A2R10G10B10_UInt:
	case Format::A2R10G10B10_SInt:
	case Format::A2R10G10B10_UNorm:
	case Format::A2R10G10B10_SNorm:
		return 4;
	}

	return 0;
}

int GetFormatSize(Format format)
{
	switch (format)
	{
	case Format::Undefined:
		return 0;
	case Format::DefaultColor:
		return 0;
	case Format::DefaultDepthStencil:
		return 0;
	case Format::R8_SNorm:
		return 1;
	case Format::R8_UNorm:
		return 1;
	case Format::R8_UInt:
		return 1;
	case Format::R8_SInt:
		return 1;
	case Format::R16_UNorm:
		return 2;
	case Format::R16_SNorm:
		return 2;
	case Format::R16_UInt:
		return 2;
	case Format::R16_SInt:
		return 2;
	case Format::R16_Float:
		return 2;
	case Format::R32_UInt:
		return 4;
	case Format::R32_SInt:
		return 4;
	case Format::R32_Float:
		return 4;
	case Format::Depth16:
		return 2;
	case Format::Depth32:
		return 4;
	case Format::Depth24Stencil8:
		return 4;
	case Format::Depth32Stencil8:
		return 5;
	case Format::R8G8_SNorm:
		return 2;
	case Format::R8G8_UNorm:
		return 2;
	case Format::R8G8_UInt:
		return 2;
	case Format::R8G8_SInt:
		return 2;
	case Format::R16G16_UNorm:
		return 4;
	case Format::R16G16_SNorm:
		return 4;
	case Format::R16G16_UInt:
		return 4;
	case Format::R16G16_SInt:
		return 4;
	case Format::R16G16_Float:
		return 4;
	case Format::R32G32_UInt:
		return 8;
	case Format::R32G32_SInt:
		return 8;
	case Format::R32G32_Float:
		return 8;
	case Format::R8G8B8_UNorm:
		return 3;
	case Format::R8G8B8_SNorm:
		return 3;
	case Format::R8G8B8_UInt:
		return 3;
	case Format::R8G8B8_SInt:
		return 3;
	case Format::R8G8B8_sRGB:
		return 3;
	case Format::R16G16B16_UNorm:
		return 6;
	case Format::R16G16B16_SNorm:
		return 6;
	case Format::R16G16B16_UInt:
		return 6;
	case Format::R16G16B16_SInt:
		return 6;
	case Format::R16G16B16_Float:
		return 6;
	case Format::R32G32B32_UInt:
		return 12;
	case Format::R32G32B32_SInt:
		return 12;
	case Format::R32G32B32_Float:
		return 12;
	case Format::R8G8B8A8_sRGB:
		return 4;
	case Format::R8G8B8A8_SNorm:
		return 4;
	case Format::R8G8B8A8_UNorm:
		return 4;
	case Format::R8G8B8A8_UInt:
		return 4;
	case Format::R8G8B8A8_SInt:
		return 4;
	case Format::R16G16B16A16_UNorm:
		return 8;
	case Format::R16G16B16A16_SNorm:
		return 8;
	case Format::R16G16B16A16_UInt:
		return 8;
	case Format::R16G16B16A16_SInt:
		return 8;
	case Format::R16G16B16A16_Float:
		return 8;
	case Format::R32G32B32A32_UInt:
		return 16;
	case Format::R32G32B32A32_SInt:
		return 16;
	case Format::R32G32B32A32_Float:
		return 16;
	case Format::A2R10G10B10_UInt:
		return 4;
	case Format::A2R10G10B10_SInt:
		return 4;
	case Format::A2R10G10B10_UNorm:
		return 4;
	case Format::A2R10G10B10_SNorm:
		return 4;

	case Format::BC4_UNorm:
	case Format::BC5_UNorm:
	case Format::BC1_RGBA_UNorm:
	case Format::BC1_RGBA_sRGB:
	case Format::BC1_RGB_UNorm:
	case Format::BC1_RGB_sRGB:
	case Format::BC3_UNorm:
	case Format::BC3_sRGB:
		return 0;
	}
	EG_UNREACHABLE
}

bool IsSRGBFormat(Format format)
{
	return format == Format::R8G8B8A8_sRGB || format == Format::R8G8B8_sRGB || format == Format::BC1_RGB_sRGB ||
	       format == Format::BC1_RGBA_sRGB || format == Format::BC3_sRGB;
}

static const Format compressedFormats[] = { Format::BC1_RGBA_UNorm, Format::BC1_RGBA_sRGB, Format::BC1_RGB_UNorm,
	                                        Format::BC1_RGB_sRGB,   Format::BC3_UNorm,     Format::BC3_sRGB,
	                                        Format::BC4_UNorm,      Format::BC5_UNorm };

bool IsCompressedFormat(Format format)
{
	return Contains(compressedFormats, format);
}

uint32_t GetImageByteSize(uint32_t width, uint32_t height, Format format)
{
	uint32_t numBlocks = ((width + 3) / 4) * ((height + 3) / 4);

	switch (format)
	{
	case eg::Format::BC1_RGB_UNorm:
	case eg::Format::BC1_RGB_sRGB:
	case eg::Format::BC1_RGBA_UNorm:
	case eg::Format::BC1_RGBA_sRGB:
	case eg::Format::BC4_UNorm:
		return numBlocks * 8;
	case eg::Format::BC3_UNorm:
	case eg::Format::BC3_sRGB:
	case eg::Format::BC5_UNorm:
		return numBlocks * 16;
	default:
		return width * height * GetFormatSize(format);
	}
}

std::string_view FormatToString(Format format)
{
	switch (format)
	{
	case Format::Undefined:
		return "Undefined";
	case Format::DefaultColor:
		return "DefaultColor";
	case Format::DefaultDepthStencil:
		return "DefaultDepthStencil";
	case Format::R8_SNorm:
		return "R8_SNorm";
	case Format::R8_UNorm:
		return "R8_UNorm";
	case Format::R8_UInt:
		return "R8_UInt";
	case Format::R8_SInt:
		return "R8_SInt";
	case Format::R16_UNorm:
		return "R16_UNorm";
	case Format::R16_SNorm:
		return "R16_SNorm";
	case Format::R16_UInt:
		return "R16_UInt";
	case Format::R16_SInt:
		return "R16_SInt";
	case Format::R16_Float:
		return "R16_Float";
	case Format::R32_UInt:
		return "R32_UInt";
	case Format::R32_SInt:
		return "R32_SInt";
	case Format::R32_Float:
		return "R32_Float";
	case Format::R8G8_UNorm:
		return "R8G8_UNorm";
	case Format::R8G8_SNorm:
		return "R8G8_SNorm";
	case Format::R8G8_UInt:
		return "R8G8_UInt";
	case Format::R8G8_SInt:
		return "R8G8_SInt";
	case Format::R16G16_UNorm:
		return "R16G16_UNorm";
	case Format::R16G16_SNorm:
		return "R16G16_SNorm";
	case Format::R16G16_UInt:
		return "R16G16_UInt";
	case Format::R16G16_SInt:
		return "R16G16_SInt";
	case Format::R16G16_Float:
		return "R16G16_Float";
	case Format::R32G32_UInt:
		return "R32G32_UInt";
	case Format::R32G32_SInt:
		return "R32G32_SInt";
	case Format::R32G32_Float:
		return "R32G32_Float";
	case Format::R8G8B8_UNorm:
		return "R8G8B8_UNorm";
	case Format::R8G8B8_SNorm:
		return "R8G8B8_SNorm";
	case Format::R8G8B8_UInt:
		return "R8G8B8_UInt";
	case Format::R8G8B8_SInt:
		return "R8G8B8_SInt";
	case Format::R8G8B8_sRGB:
		return "R8G8B8_sRGB";
	case Format::R16G16B16_UNorm:
		return "R16G16B16_UNorm";
	case Format::R16G16B16_SNorm:
		return "R16G16B16_SNorm";
	case Format::R16G16B16_UInt:
		return "R16G16B16_UInt";
	case Format::R16G16B16_SInt:
		return "R16G16B16_SInt";
	case Format::R16G16B16_Float:
		return "R16G16B16_Float";
	case Format::R32G32B32_UInt:
		return "R32G32B32_UInt";
	case Format::R32G32B32_SInt:
		return "R32G32B32_SInt";
	case Format::R32G32B32_Float:
		return "R32G32B32_Float";
	case Format::R8G8B8A8_sRGB:
		return "R8G8B8A8_sRGB";
	case Format::R8G8B8A8_UNorm:
		return "R8G8B8A8_UNorm";
	case Format::R8G8B8A8_SNorm:
		return "R8G8B8A8_SNorm";
	case Format::R8G8B8A8_UInt:
		return "R8G8B8A8_UInt";
	case Format::R8G8B8A8_SInt:
		return "R8G8B8A8_SInt";
	case Format::R16G16B16A16_UNorm:
		return "R16G16B16A16_UNorm";
	case Format::R16G16B16A16_SNorm:
		return "R16G16B16A16_SNorm";
	case Format::R16G16B16A16_UInt:
		return "R16G16B16A16_UInt";
	case Format::R16G16B16A16_SInt:
		return "R16G16B16A16_SInt";
	case Format::R16G16B16A16_Float:
		return "R16G16B16A16_Float";
	case Format::R32G32B32A32_UInt:
		return "R32G32B32A32_UInt";
	case Format::R32G32B32A32_SInt:
		return "R32G32B32A32_SInt";
	case Format::R32G32B32A32_Float:
		return "R32G32B32A32_Float";
	case Format::A2R10G10B10_UInt:
		return "A2R10G10B10_UInt";
	case Format::A2R10G10B10_SInt:
		return "A2R10G10B10_SInt";
	case Format::A2R10G10B10_UNorm:
		return "A2R10G10B10_UNorm";
	case Format::A2R10G10B10_SNorm:
		return "A2R10G10B10_SNorm";
	case Format::BC1_RGBA_UNorm:
		return "BC1_RGBA_UNorm";
	case Format::BC1_RGBA_sRGB:
		return "BC1_RGBA_sRGB";
	case Format::BC1_RGB_UNorm:
		return "BC1_RGB_UNorm";
	case Format::BC1_RGB_sRGB:
		return "BC1_RGB_sRGB";
	case Format::BC3_UNorm:
		return "BC3_UNorm";
	case Format::BC3_sRGB:
		return "BC3_sRGB";
	case Format::BC4_UNorm:
		return "BC4_UNorm";
	case Format::BC5_UNorm:
		return "BC5_UNorm";
	case Format::Depth16:
		return "Depth16";
	case Format::Depth32:
		return "Depth32";
	case Format::Depth24Stencil8:
		return "Depth24Stencil8";
	case Format::Depth32Stencil8:
		return "Depth32Stencil8";
	}
	EG_UNREACHABLE
}

const std::array<std::array<Format, 4>, 11> detail::formatFromDataTypeAndComponentCount{ {
	// Float32
	{ Format::R32_Float, Format::R32G32_Float, Format::R32G32B32_Float, Format::R32G32B32A32_Float },
	// UInt8Norm
	{ Format::R8_UNorm, Format::R8G8_UNorm, Format::R8G8B8_UNorm, Format::R8G8B8A8_UNorm },
	// UInt16Norm
	{ Format::R16_UNorm, Format::R16G16_UNorm, Format::R16G16B16_UNorm, Format::R16G16B16A16_UNorm },
	// SInt8Norm
	{ Format::R8_SNorm, Format::R8G8_SNorm, Format::R8G8B8_SNorm, Format::R8G8B8A8_SNorm },
	// SInt16Norm
	{ Format::R16_SNorm, Format::R16G16_SNorm, Format::R16G16B16_SNorm, Format::R16G16B16A16_SNorm },
	// UInt8
	{ Format::R8_UNorm, Format::R8G8_UNorm, Format::R8G8B8_UNorm, Format::R8G8B8A8_UNorm },
	// UInt16
	{ Format::R16_UInt, Format::R16G16_UInt, Format::R16G16B16_UInt, Format::R16G16B16A16_UInt },
	// UInt32
	{ Format::R32_UInt, Format::R16G16_UInt, Format::R16G16B16_UInt, Format::R16G16B16A16_UInt },
	// SInt8
	{ Format::R8_SInt, Format::R8G8_SInt, Format::R8G8B8_SInt, Format::R8G8B8A8_SInt },
	// SInt16
	{ Format::R16_SInt, Format::R16G16_SInt, Format::R16G16B16_SInt, Format::R16G16B16A16_SInt },
	// SInt32
	{ Format::R32_SInt, Format::R32G32_SInt, Format::R32G32B32_SInt, Format::R32G32B32A32_SInt },
} };

const std::array<std::string_view, 8> FormatCapabilityNames = { "SampledImage",           "SampledImageFilterLinear",
	                                                            "StorageImage",           "StorageImageAtomic",
	                                                            "ColorAttachment",        "ColorAttachmentBlend",
	                                                            "DepthStencilAttachment", "VertexAttribute" };
} // namespace eg
