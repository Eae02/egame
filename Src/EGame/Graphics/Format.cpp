#include "Format.hpp"
#include "../Assert.hpp"
#include "../Utils.hpp"

namespace eg
{
FormatType GetFormatType(Format format)
{
	switch (format)
	{
	case Format::Undefined:
	case Format::DefaultColor: break;
	case Format::R8_UNorm:
	case Format::R8G8_UNorm:
	case Format::R8G8B8A8_UNorm:
	case Format::R8G8B8A8_sRGB:
	case Format::R16_UNorm:
	case Format::R16G16_UNorm:
	case Format::R16G16B16A16_UNorm:
	case Format::BC1_RGBA_UNorm:
	case Format::BC1_RGBA_sRGB:
	case Format::BC3_RGBA_UNorm:
	case Format::BC3_RGBA_sRGB:
	case Format::BC4_R_UNorm:
	case Format::BC5_RG_UNorm:
	case Format::BC7_RGBA_UNorm:
	case Format::BC7_RGBA_sRGB:
	case Format::A2R10G10B10_UNorm: return FormatType::UNorm;
	case Format::R8_SNorm:
	case Format::R16_SNorm:
	case Format::R8G8_SNorm:
	case Format::R16G16_SNorm:
	case Format::R8G8B8A8_SNorm:
	case Format::R16G16B16A16_SNorm:
	case Format::A2R10G10B10_SNorm: return FormatType::SNorm;
	case Format::R8_UInt:
	case Format::R16_UInt:
	case Format::R32_UInt:
	case Format::R8G8_UInt:
	case Format::R16G16_UInt:
	case Format::R32G32_UInt:
	case Format::R32G32B32_UInt:
	case Format::R8G8B8A8_UInt:
	case Format::R16G16B16A16_UInt:
	case Format::R32G32B32A32_UInt:
	case Format::A2R10G10B10_UInt: return FormatType::UInt;
	case Format::R16_Float:
	case Format::R32_Float:
	case Format::R16G16_Float:
	case Format::R32G32_Float:
	case Format::R32G32B32_Float:
	case Format::R16G16B16A16_Float:
	case Format::BC6H_RGB_UFloat:
	case Format::BC6H_RGB_Float:
	case Format::B10G11R11_UFloat:
	case Format::R32G32B32A32_Float: return FormatType::Float;
	case Format::R8_SInt:
	case Format::R16_SInt:
	case Format::R32_SInt:
	case Format::R8G8_SInt:
	case Format::R16G16_SInt:
	case Format::R32G32_SInt:
	case Format::R32G32B32_SInt:
	case Format::R8G8B8A8_SInt:
	case Format::R16G16B16A16_SInt:
	case Format::R32G32B32A32_SInt:
	case Format::A2R10G10B10_SInt: return FormatType::SInt;
	case Format::Depth16:
	case Format::Depth32:
	case Format::Depth24Stencil8:
	case Format::Depth32Stencil8:
	case Format::DefaultDepthStencil: return FormatType::DepthStencil;
	}

	EG_UNREACHABLE
}

uint32_t GetFormatComponentCount(Format format)
{
	switch (format)
	{
	case Format::Undefined:
	case Format::DefaultColor:
	case Format::DefaultDepthStencil: return 0;
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
	case Format::BC4_R_UNorm:
	case Format::Depth16:
	case Format::Depth32:
	case Format::Depth24Stencil8:
	case Format::Depth32Stencil8: return 1;
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
	case Format::BC5_RG_UNorm: return 2;
	case Format::R32G32B32_UInt:
	case Format::R32G32B32_SInt:
	case Format::R32G32B32_Float:
	case Format::BC6H_RGB_UFloat:
	case Format::BC6H_RGB_Float:
	case Format::B10G11R11_UFloat: return 3;
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
	case Format::BC3_RGBA_UNorm:
	case Format::BC3_RGBA_sRGB:
	case Format::BC7_RGBA_UNorm:
	case Format::BC7_RGBA_sRGB:
	case Format::A2R10G10B10_UInt:
	case Format::A2R10G10B10_SInt:
	case Format::A2R10G10B10_UNorm:
	case Format::A2R10G10B10_SNorm: return 4;
	}

	return 0;
}

std::optional<uint32_t> GetFormatBytesPerPixel(Format format)
{
	switch (format)
	{
	case Format::Undefined: return 0;
	case Format::DefaultColor: return 0;
	case Format::DefaultDepthStencil: return 0;
	case Format::R8_SNorm: return 1;
	case Format::R8_UNorm: return 1;
	case Format::R8_UInt: return 1;
	case Format::R8_SInt: return 1;
	case Format::R16_UNorm: return 2;
	case Format::R16_SNorm: return 2;
	case Format::R16_UInt: return 2;
	case Format::R16_SInt: return 2;
	case Format::R16_Float: return 2;
	case Format::R32_UInt: return 4;
	case Format::R32_SInt: return 4;
	case Format::R32_Float: return 4;
	case Format::Depth16: return 2;
	case Format::Depth32: return 4;
	case Format::Depth24Stencil8: return 4;
	case Format::Depth32Stencil8: return 5;
	case Format::R8G8_SNorm: return 2;
	case Format::R8G8_UNorm: return 2;
	case Format::R8G8_UInt: return 2;
	case Format::R8G8_SInt: return 2;
	case Format::R16G16_UNorm: return 4;
	case Format::R16G16_SNorm: return 4;
	case Format::R16G16_UInt: return 4;
	case Format::R16G16_SInt: return 4;
	case Format::R16G16_Float: return 4;
	case Format::R32G32_UInt: return 8;
	case Format::R32G32_SInt: return 8;
	case Format::R32G32_Float: return 8;
	case Format::R32G32B32_UInt: return 12;
	case Format::R32G32B32_SInt: return 12;
	case Format::R32G32B32_Float: return 12;
	case Format::R8G8B8A8_sRGB: return 4;
	case Format::R8G8B8A8_SNorm: return 4;
	case Format::R8G8B8A8_UNorm: return 4;
	case Format::R8G8B8A8_UInt: return 4;
	case Format::R8G8B8A8_SInt: return 4;
	case Format::R16G16B16A16_UNorm: return 8;
	case Format::R16G16B16A16_SNorm: return 8;
	case Format::R16G16B16A16_UInt: return 8;
	case Format::R16G16B16A16_SInt: return 8;
	case Format::R16G16B16A16_Float: return 8;
	case Format::R32G32B32A32_UInt: return 16;
	case Format::R32G32B32A32_SInt: return 16;
	case Format::R32G32B32A32_Float: return 16;
	case Format::A2R10G10B10_UInt: return 4;
	case Format::A2R10G10B10_SInt: return 4;
	case Format::A2R10G10B10_UNorm: return 4;
	case Format::A2R10G10B10_SNorm: return 4;
	case Format::B10G11R11_UFloat: return 4;

	case Format::BC1_RGBA_UNorm:
	case Format::BC1_RGBA_sRGB:
	case Format::BC3_RGBA_UNorm:
	case Format::BC3_RGBA_sRGB:
	case Format::BC4_R_UNorm:
	case Format::BC5_RG_UNorm:
	case Format::BC6H_RGB_UFloat:
	case Format::BC6H_RGB_Float:
	case Format::BC7_RGBA_UNorm:
	case Format::BC7_RGBA_sRGB: return std::nullopt;
	}
	EG_UNREACHABLE
}

bool IsSRGBFormat(Format format)
{
	return format == Format::R8G8B8A8_sRGB || format == Format::BC1_RGBA_sRGB || format == Format::BC3_RGBA_sRGB;
}

uint32_t GetFormatBlockWidth(Format format)
{
	switch (format)
	{
	case Format::BC1_RGBA_UNorm:
	case Format::BC1_RGBA_sRGB:
	case Format::BC3_RGBA_UNorm:
	case Format::BC3_RGBA_sRGB:
	case Format::BC4_R_UNorm:
	case Format::BC5_RG_UNorm: return 4;

	default: return 1;
	}
}

uint32_t GetFormatBytesPerBlock(Format format)
{
	switch (format)
	{
	case Format::BC1_RGBA_UNorm:
	case Format::BC1_RGBA_sRGB:
	case Format::BC4_R_UNorm: return 8;

	case Format::BC3_RGBA_UNorm:
	case Format::BC3_RGBA_sRGB:
	case Format::BC5_RG_UNorm:
	case Format::BC6H_RGB_UFloat:
	case Format::BC6H_RGB_Float:
	case Format::BC7_RGBA_UNorm:
	case Format::BC7_RGBA_sRGB: return 16;

	default: return GetFormatBytesPerPixel(format).value();
	}
}

uint32_t GetImageByteSize(uint32_t width, uint32_t height, Format format)
{
	uint32_t blockWidth = GetFormatBlockWidth(format);
	uint32_t numBlocks = ((width + blockWidth - 1) / blockWidth) * ((height + blockWidth - 1) / blockWidth);
	return GetFormatBytesPerBlock(format) * numBlocks;
}

std::string_view FormatToString(Format format)
{
	switch (format)
	{
	case Format::Undefined: return "Undefined";
	case Format::DefaultColor: return "DefaultColor";
	case Format::DefaultDepthStencil: return "DefaultDepthStencil";
	case Format::R8_SNorm: return "R8_SNorm";
	case Format::R8_UNorm: return "R8_UNorm";
	case Format::R8_UInt: return "R8_UInt";
	case Format::R8_SInt: return "R8_SInt";
	case Format::R16_UNorm: return "R16_UNorm";
	case Format::R16_SNorm: return "R16_SNorm";
	case Format::R16_UInt: return "R16_UInt";
	case Format::R16_SInt: return "R16_SInt";
	case Format::R16_Float: return "R16_Float";
	case Format::R32_UInt: return "R32_UInt";
	case Format::R32_SInt: return "R32_SInt";
	case Format::R32_Float: return "R32_Float";
	case Format::R8G8_UNorm: return "R8G8_UNorm";
	case Format::R8G8_SNorm: return "R8G8_SNorm";
	case Format::R8G8_UInt: return "R8G8_UInt";
	case Format::R8G8_SInt: return "R8G8_SInt";
	case Format::R16G16_UNorm: return "R16G16_UNorm";
	case Format::R16G16_SNorm: return "R16G16_SNorm";
	case Format::R16G16_UInt: return "R16G16_UInt";
	case Format::R16G16_SInt: return "R16G16_SInt";
	case Format::R16G16_Float: return "R16G16_Float";
	case Format::R32G32_UInt: return "R32G32_UInt";
	case Format::R32G32_SInt: return "R32G32_SInt";
	case Format::R32G32_Float: return "R32G32_Float";
	case Format::R32G32B32_UInt: return "R32G32B32_UInt";
	case Format::R32G32B32_SInt: return "R32G32B32_SInt";
	case Format::R32G32B32_Float: return "R32G32B32_Float";
	case Format::R8G8B8A8_sRGB: return "R8G8B8A8_sRGB";
	case Format::R8G8B8A8_UNorm: return "R8G8B8A8_UNorm";
	case Format::R8G8B8A8_SNorm: return "R8G8B8A8_SNorm";
	case Format::R8G8B8A8_UInt: return "R8G8B8A8_UInt";
	case Format::R8G8B8A8_SInt: return "R8G8B8A8_SInt";
	case Format::R16G16B16A16_UNorm: return "R16G16B16A16_UNorm";
	case Format::R16G16B16A16_SNorm: return "R16G16B16A16_SNorm";
	case Format::R16G16B16A16_UInt: return "R16G16B16A16_UInt";
	case Format::R16G16B16A16_SInt: return "R16G16B16A16_SInt";
	case Format::R16G16B16A16_Float: return "R16G16B16A16_Float";
	case Format::R32G32B32A32_UInt: return "R32G32B32A32_UInt";
	case Format::R32G32B32A32_SInt: return "R32G32B32A32_SInt";
	case Format::R32G32B32A32_Float: return "R32G32B32A32_Float";
	case Format::A2R10G10B10_UInt: return "A2R10G10B10_UInt";
	case Format::A2R10G10B10_SInt: return "A2R10G10B10_SInt";
	case Format::A2R10G10B10_UNorm: return "A2R10G10B10_UNorm";
	case Format::A2R10G10B10_SNorm: return "A2R10G10B10_SNorm";
	case Format::B10G11R11_UFloat: return "B10G11R11_UFloat";
	case Format::BC1_RGBA_UNorm: return "BC1_RGBA_UNorm";
	case Format::BC1_RGBA_sRGB: return "BC1_RGBA_sRGB";
	case Format::BC3_RGBA_UNorm: return "BC3_RGBA_UNorm";
	case Format::BC3_RGBA_sRGB: return "BC3_RGBA_sRGB";
	case Format::BC4_R_UNorm: return "BC4_R_UNorm";
	case Format::BC5_RG_UNorm: return "BC5_RG_UNorm";
	case Format::BC6H_RGB_UFloat: return "BC6H_RGB_UFloat";
	case Format::BC6H_RGB_Float: return "BC6H_RGB_Float";
	case Format::BC7_RGBA_UNorm: return "BC7_RGBA_UNorm";
	case Format::BC7_RGBA_sRGB: return "BC7_RGBA_sRGB";
	case Format::Depth16: return "Depth16";
	case Format::Depth32: return "Depth32";
	case Format::Depth24Stencil8: return "Depth24Stencil8";
	case Format::Depth32Stencil8: return "Depth32Stencil8";
	}
	EG_UNREACHABLE
}

const std::array<std::array<Format, 4>, 11> detail::formatFromDataTypeAndComponentCount{ {
	// Float32
	{ Format::R32_Float, Format::R32G32_Float, Format::R32G32B32_Float, Format::R32G32B32A32_Float },
	// UInt8Norm
	{ Format::R8_UNorm, Format::R8G8_UNorm, Format::Undefined, Format::R8G8B8A8_UNorm },
	// UInt16Norm
	{ Format::R16_UNorm, Format::R16G16_UNorm, Format::Undefined, Format::R16G16B16A16_UNorm },
	// SInt8Norm
	{ Format::R8_SNorm, Format::R8G8_SNorm, Format::Undefined, Format::R8G8B8A8_SNorm },
	// SInt16Norm
	{ Format::R16_SNorm, Format::R16G16_SNorm, Format::Undefined, Format::R16G16B16A16_SNorm },
	// UInt8
	{ Format::R8_UNorm, Format::R8G8_UNorm, Format::Undefined, Format::R8G8B8A8_UNorm },
	// UInt16
	{ Format::R16_UInt, Format::R16G16_UInt, Format::Undefined, Format::R16G16B16A16_UInt },
	// UInt32
	{ Format::R32_UInt, Format::R16G16_UInt, Format::Undefined, Format::R16G16B16A16_UInt },
	// SInt8
	{ Format::R8_SInt, Format::R8G8_SInt, Format::Undefined, Format::R8G8B8A8_SInt },
	// SInt16
	{ Format::R16_SInt, Format::R16G16_SInt, Format::Undefined, Format::R16G16B16A16_SInt },
	// SInt32
	{ Format::R32_SInt, Format::R32G32_SInt, Format::R32G32B32_SInt, Format::R32G32B32A32_SInt },
} };

const std::array<std::string_view, 8> FormatCapabilityNames = {
	"SampledImage",    "SampledImageFilterLinear", "StorageImage",           "StorageImageAtomic",
	"ColorAttachment", "ColorAttachmentBlend",     "DepthStencilAttachment", "VertexAttribute",
};
} // namespace eg
