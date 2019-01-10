#include "Format.hpp"
#include "../Utils.hpp"

namespace eg
{
	FormatTypes GetFormatType(Format format)
	{
		switch (format)
		{
		case Format::Undefined: break;
		case Format::R8_UNorm:
		case Format::R8G8_UNorm:
		case Format::R8G8B8A8_UNorm:
		case Format::R8G8B8A8_sRGB:
		case Format::R8G8B8_sRGB:
		case Format::BC1_UNorm:
		case Format::BC1_sRGB:
		case Format::BC3_UNorm:
		case Format::BC3_sRGB:
		case Format::BC4_UNorm:
		case Format::BC5_UNorm:
			return FormatTypes::UNorm;
		case Format::R8_UInt:
		case Format::R16_UInt:
		case Format::R32_UInt:
		case Format::R8G8_UInt:
		case Format::R16G16_UInt:
		case Format::R32G32_UInt:
		case Format::R16G16B16_UInt:
		case Format::R32G32B32_UInt:
		case Format::R8G8B8A8_UInt:
		case Format::R16G16B16A16_UInt:
		case Format::R32G32B32A32_UInt:
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
		case Format::R16G16B16_SInt:
		case Format::R32G32B32_SInt:
		case Format::R8G8B8A8_SInt:
		case Format::R16G16B16A16_SInt:
		case Format::R32G32B32A32_SInt:
			return FormatTypes::SInt;
		case Format::Depth16:
		case Format::Depth32:
		case Format::Depth24Stencil8:
		case Format::Depth32Stencil8:
			return FormatTypes::DepthStencil;
		}
		
		EG_UNREACHABLE
	}
	
	int GetFormatComponentCount(Format format)
	{
		switch (format)
		{
		case Format::Undefined:
			return 0;
		case Format::R8_UNorm:
		case Format::R8_UInt:
		case Format::R8_SInt:
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
		case Format::R8G8_UInt:
		case Format::R8G8_SInt:
		case Format::R16G16_UInt:
		case Format::R16G16_SInt:
		case Format::R16G16_Float:
		case Format::R32G32_UInt:
		case Format::R32G32_SInt:
		case Format::R32G32_Float:
		case Format::BC5_UNorm:
			return 2;
		case Format::R8G8B8_sRGB:
		case Format::R16G16B16_UInt:
		case Format::R16G16B16_SInt:
		case Format::R16G16B16_Float:
		case Format::R32G32B32_UInt:
		case Format::R32G32B32_SInt:
		case Format::R32G32B32_Float:
		case Format::BC1_UNorm:
		case Format::BC1_sRGB:
			return 3;
		case Format::R8G8B8A8_sRGB:
		case Format::R8G8B8A8_UNorm:
		case Format::R8G8B8A8_UInt:
		case Format::R8G8B8A8_SInt:
		case Format::R16G16B16A16_UInt:
		case Format::R16G16B16A16_SInt:
		case Format::R16G16B16A16_Float:
		case Format::R32G32B32A32_UInt:
		case Format::R32G32B32A32_SInt:
		case Format::R32G32B32A32_Float:
		case Format::BC3_UNorm:
		case Format::BC3_sRGB:
			return 4;
		}
		
		return 0;
	}
}
