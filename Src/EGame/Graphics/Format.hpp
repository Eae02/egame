#pragma once

#include "../API.hpp"
#include "../Color.hpp"
#include "../Utils.hpp"

#include <cstdint>
#include <variant>
#include <array>
#include <string_view>

namespace eg
{
	enum class Format
	{
		Undefined,
		DefaultColor,
		DefaultDepthStencil,
		R8_SNorm,
		R8_UNorm,
		R8_UInt,
		R8_SInt,
		R16_UNorm,
		R16_SNorm,
		R16_UInt,
		R16_SInt,
		R16_Float,
		R32_UInt,
		R32_SInt,
		R32_Float,
		
		R8G8_UNorm,
		R8G8_SNorm,
		R8G8_UInt,
		R8G8_SInt,
		R16G16_UNorm,
		R16G16_SNorm,
		R16G16_UInt,
		R16G16_SInt,
		R16G16_Float,
		R32G32_UInt,
		R32G32_SInt,
		R32G32_Float,
		
		R8G8B8_UNorm,
		R8G8B8_SNorm,
		R8G8B8_UInt,
		R8G8B8_SInt,
		R8G8B8_sRGB,
		R16G16B16_UNorm,
		R16G16B16_SNorm,
		R16G16B16_UInt,
		R16G16B16_SInt,
		R16G16B16_Float,
		R32G32B32_UInt,
		R32G32B32_SInt,
		R32G32B32_Float,
		
		R8G8B8A8_sRGB,
		R8G8B8A8_UNorm,
		R8G8B8A8_SNorm,
		R8G8B8A8_UInt,
		R8G8B8A8_SInt,
		R16G16B16A16_UNorm,
		R16G16B16A16_SNorm,
		R16G16B16A16_UInt,
		R16G16B16A16_SInt,
		R16G16B16A16_Float,
		R32G32B32A32_UInt,
		R32G32B32A32_SInt,
		R32G32B32A32_Float,
		
		A2R10G10B10_UInt,
		A2R10G10B10_SInt,
		A2R10G10B10_UNorm,
		A2R10G10B10_SNorm,
		
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
		SNorm,
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
	
	EG_API std::string_view FormatToString(Format format);
	
	template <typename T>
	std::array<T, 4> GetClearValueAs(const std::variant<ColorLin, glm::ivec4, glm::uvec4>& clearValueVariant)
	{
		return std::visit([] (const auto& x) { return std::array<T, 4> { static_cast<T>(x.r), static_cast<T>(x.g), static_cast<T>(x.b), static_cast<T>(x.a) }; }, clearValueVariant);
	}
	
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
	
	namespace detail
	{
		EG_API extern const std::array<std::array<Format, 4>, 11> formatFromDataTypeAndComponentCount;
	}
	
	inline Format FormatFromDataTypeAndComponentCount(DataType dataType, uint32_t numComponents)
	{
		if (numComponents == 0 || numComponents > 4)
			return Format::Undefined;
		return detail::formatFromDataTypeAndComponentCount.at(static_cast<int>(dataType)).at(numComponents - 1);
	}
	
	enum class FormatCapabilities
	{
		SampledImage             = 0x1,
		SampledImageFilterLinear = 0x2,
		StorageImage             = 0x4,
		StorageImageAtomic       = 0x8,
		ColorAttachment          = 0x10,
		ColorAttachmentBlend     = 0x20,
		DepthStencilAttachment   = 0x40,
		VertexAttribute          = 0x80,
	};
	
	EG_API extern const std::array<std::string_view, 8> FormatCapabilityNames;
	
	EG_BIT_FIELD(FormatCapabilities)
}
