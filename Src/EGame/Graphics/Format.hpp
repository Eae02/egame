#pragma once

#include "../API.hpp"

namespace eg
{
	enum class Format
	{
		Undefined,
		DefaultColor,
		DefaultDepthStencil,
		R8_UNorm,
		R8_UInt,
		R8_SInt,
		R16_UInt,
		R16_SInt,
		R16_Float,
		R32_UInt,
		R32_SInt,
		R32_Float,
		R8G8_UNorm,
		R8G8_UInt,
		R8G8_SInt,
		R16G16_UInt,
		R16G16_SInt,
		R16G16_Float,
		R32G32_UInt,
		R32G32_SInt,
		R32G32_Float,
		R8G8B8_sRGB,
		R16G16B16_UInt,
		R16G16B16_SInt,
		R16G16B16_Float,
		R32G32B32_UInt,
		R32G32B32_SInt,
		R32G32B32_Float,
		R8G8B8A8_sRGB,
		R8G8B8A8_UNorm,
		R8G8B8A8_UInt,
		R8G8B8A8_SInt,
		R16G16B16A16_UInt,
		R16G16B16A16_SInt,
		R16G16B16A16_Float,
		R32G32B32A32_UInt,
		R32G32B32A32_SInt,
		R32G32B32A32_Float,
		BC1_RGBA_UNorm,
		BC1_RGBA_sRGB,
		BC1_RGB_UNorm,
		BC1_RGB_sRGB,
		BC3_UNorm,
		BC3_sRGB,
		BC4_UNorm,
		BC5_UNorm,
		Depth16,
		Depth32,
		Depth24Stencil8,
		Depth32Stencil8
	};
	
	enum class FormatTypes
	{
		UNorm,
		UInt,
		SInt,
		Float,
		DepthStencil
	};
	
	EG_API FormatTypes GetFormatType(Format format);
	EG_API bool IsCompressedFormat(Format format);
	EG_API int GetFormatComponentCount(Format format);
	EG_API int GetFormatSize(Format format);
	EG_API bool IsSRGBFormat(Format format);
	
	EG_API uint32_t GetImageByteSize(uint32_t width, uint32_t height, Format format);
	
	enum class DataType
	{
		Float32,
		UInt8Norm,
		UInt16Norm,
		SInt8Norm,
		SInt16Norm,
		UInt8,
		UInt16,
		UInt32,
		SInt8,
		SInt16,
		SInt32
	};
}
