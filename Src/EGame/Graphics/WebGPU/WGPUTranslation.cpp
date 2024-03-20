#include "WGPUTranslation.hpp"
#include "../../Log.hpp"

namespace eg::graphics_api::webgpu
{
WGPUTextureFormat TranslateTextureFormat(Format format)
{
	// clang-format off
	switch (format)
	{
	case Format::DefaultColor:         return wgpuctx.swapchainFormat;
	case Format::DefaultDepthStencil:  return WGPUTextureFormat_Undefined;
	case Format::R8_SNorm:             return WGPUTextureFormat_R8Snorm;
	case Format::R8_UNorm:             return WGPUTextureFormat_R8Unorm;
	case Format::R8_UInt:              return WGPUTextureFormat_R8Uint;
	case Format::R8_SInt:              return WGPUTextureFormat_R8Sint;
	case Format::R16_UNorm:            return WGPUTextureFormat_R16Unorm;
	case Format::R16_SNorm:            return WGPUTextureFormat_R16Snorm;
	case Format::R16_UInt:             return WGPUTextureFormat_R16Uint;
	case Format::R16_SInt:             return WGPUTextureFormat_R16Sint;
	case Format::R16_Float:            return WGPUTextureFormat_R16Float;
	case Format::R32_UInt:             return WGPUTextureFormat_R32Uint;
	case Format::R32_SInt:             return WGPUTextureFormat_R32Sint;
	case Format::R32_Float:            return WGPUTextureFormat_R32Float;
	case Format::R8G8_UNorm:           return WGPUTextureFormat_RG8Unorm;
	case Format::R8G8_SNorm:           return WGPUTextureFormat_RG8Snorm;
	case Format::R8G8_UInt:            return WGPUTextureFormat_RG8Uint;
	case Format::R8G8_SInt:            return WGPUTextureFormat_RG8Sint;
	case Format::R16G16_UNorm:         return WGPUTextureFormat_RG16Unorm;
	case Format::R16G16_SNorm:         return WGPUTextureFormat_RG16Snorm;
	case Format::R16G16_UInt:          return WGPUTextureFormat_RG16Uint;
	case Format::R16G16_SInt:          return WGPUTextureFormat_RG16Sint;
	case Format::R16G16_Float:         return WGPUTextureFormat_RG16Float;
	case Format::R32G32_UInt:          return WGPUTextureFormat_RG32Uint;
	case Format::R32G32_SInt:          return WGPUTextureFormat_RG32Sint;
	case Format::R32G32_Float:         return WGPUTextureFormat_RG32Float;
	case Format::R8G8B8A8_sRGB:        return WGPUTextureFormat_RGBA8UnormSrgb;
	case Format::R8G8B8A8_UNorm:       return WGPUTextureFormat_RGBA8Unorm;
	case Format::R8G8B8A8_SNorm:       return WGPUTextureFormat_RGBA8Snorm;
	case Format::R8G8B8A8_UInt:        return WGPUTextureFormat_RGBA8Uint;
	case Format::R8G8B8A8_SInt:        return WGPUTextureFormat_RGBA8Sint;
	case Format::R16G16B16A16_UNorm:   return WGPUTextureFormat_RGBA16Unorm;
	case Format::R16G16B16A16_SNorm:   return WGPUTextureFormat_RGBA16Snorm;
	case Format::R16G16B16A16_UInt:    return WGPUTextureFormat_RGBA16Uint;
	case Format::R16G16B16A16_SInt:    return WGPUTextureFormat_RGBA16Sint;
	case Format::R16G16B16A16_Float:   return WGPUTextureFormat_RGBA16Float;
	case Format::R32G32B32A32_UInt:    return WGPUTextureFormat_RGBA32Uint;
	case Format::R32G32B32A32_SInt:    return WGPUTextureFormat_RGBA32Sint;
	case Format::R32G32B32A32_Float:   return WGPUTextureFormat_RGBA32Float;
	case Format::A2R10G10B10_UInt:     return WGPUTextureFormat_RGB10A2Uint;
	case Format::A2R10G10B10_UNorm:    return WGPUTextureFormat_RGB10A2Unorm;
	case Format::BC1_RGBA_UNorm:       return WGPUTextureFormat_BC1RGBAUnorm;
	case Format::BC1_RGBA_sRGB:        return WGPUTextureFormat_BC1RGBAUnormSrgb;
	case Format::BC1_RGB_UNorm:        return WGPUTextureFormat_BC1RGBAUnorm;
	case Format::BC1_RGB_sRGB:         return WGPUTextureFormat_BC1RGBAUnormSrgb;
	case Format::BC3_UNorm:            return WGPUTextureFormat_BC3RGBAUnorm;
	case Format::BC3_sRGB:             return WGPUTextureFormat_BC3RGBAUnormSrgb;
	case Format::BC4_UNorm:            return WGPUTextureFormat_BC4RUnorm;
	case Format::BC5_UNorm:            return WGPUTextureFormat_BC5RGUnorm;
	case Format::Depth16:              return WGPUTextureFormat_Depth16Unorm;
	case Format::Depth32:              return WGPUTextureFormat_Depth32Float;
	case Format::Depth24Stencil8:      return WGPUTextureFormat_Depth24PlusStencil8;
	case Format::Depth32Stencil8:      return WGPUTextureFormat_Depth32FloatStencil8;
	}
	// clang-format on

	eg::Log(eg::LogLevel::Warning, "wgpu", "Attempted to translate an unknown format: {0}", FormatToString(format));

	return WGPUTextureFormat_Undefined;
}

WGPUVertexFormat TranslateVertexFormat(Format format)
{
	// clang-format off
	switch (format)
	{
	case Format::R32_UInt:             return WGPUVertexFormat_Uint32;
	case Format::R32_SInt:             return WGPUVertexFormat_Sint32;
	case Format::R32_Float:            return WGPUVertexFormat_Float32;
	case Format::R8G8_UNorm:           return WGPUVertexFormat_Unorm8x2;
	case Format::R8G8_SNorm:           return WGPUVertexFormat_Snorm8x2;
	case Format::R8G8_UInt:            return WGPUVertexFormat_Uint8x2;
	case Format::R8G8_SInt:            return WGPUVertexFormat_Sint8x2;
	case Format::R16G16_UNorm:         return WGPUVertexFormat_Unorm16x2;
	case Format::R16G16_SNorm:         return WGPUVertexFormat_Snorm16x2;
	case Format::R16G16_UInt:          return WGPUVertexFormat_Uint16x2;
	case Format::R16G16_SInt:          return WGPUVertexFormat_Sint16x2;
	case Format::R16G16_Float:         return WGPUVertexFormat_Float16x2;
	case Format::R32G32_UInt:          return WGPUVertexFormat_Uint32;
	case Format::R32G32_SInt:          return WGPUVertexFormat_Sint32;
	case Format::R32G32_Float:         return WGPUVertexFormat_Float32;
	case Format::R32G32B32_UInt:       return WGPUVertexFormat_Uint32x3;
	case Format::R32G32B32_SInt:       return WGPUVertexFormat_Sint32x3;
	case Format::R32G32B32_Float:      return WGPUVertexFormat_Float32x3;
	case Format::R8G8B8A8_UNorm:       return WGPUVertexFormat_Unorm8x4;
	case Format::R8G8B8A8_SNorm:       return WGPUVertexFormat_Snorm8x4;
	case Format::R8G8B8A8_UInt:        return WGPUVertexFormat_Uint8x4;
	case Format::R8G8B8A8_SInt:        return WGPUVertexFormat_Sint8x4;
	case Format::R16G16B16A16_UNorm:   return WGPUVertexFormat_Unorm16x4;
	case Format::R16G16B16A16_SNorm:   return WGPUVertexFormat_Snorm16x4;
	case Format::R16G16B16A16_UInt:    return WGPUVertexFormat_Uint16x4;
	case Format::R16G16B16A16_SInt:    return WGPUVertexFormat_Sint16x4;
	case Format::R16G16B16A16_Float:   return WGPUVertexFormat_Float16x4;
	case Format::R32G32B32A32_UInt:    return WGPUVertexFormat_Uint32;
	case Format::R32G32B32A32_SInt:    return WGPUVertexFormat_Sint32;
	case Format::R32G32B32A32_Float:   return WGPUVertexFormat_Float32;
	case Format::A2R10G10B10_UNorm:    return WGPUVertexFormat_Unorm10_10_10_2;
	
	case Format::R8_SNorm:
	case Format::R8_UNorm:
	case Format::R8_UInt:
	case Format::R8_SInt:
	case Format::R16_UNorm:
	case Format::R16_SNorm:
	case Format::R16_UInt:
	case Format::R16_SInt:
	case Format::R16_Float:
	case Format::A2R10G10B10_SNorm:
	case Format::A2R10G10B10_UInt:
	case Format::A2R10G10B10_SInt:
	case Format::B10G11R11_UFloat:
	case Format::R8G8B8A8_sRGB:
	case Format::BC1_RGBA_UNorm:
	case Format::BC1_RGBA_sRGB:
	case Format::BC1_RGB_UNorm:
	case Format::BC1_RGB_sRGB:
	case Format::BC3_UNorm:
	case Format::BC3_sRGB:
	case Format::BC4_UNorm:
	case Format::BC5_UNorm:
	case Format::Depth16:
	case Format::Depth32:
	case Format::Depth24Stencil8:
	case Format::Depth32Stencil8:
		EG_PANIC("Unsupported vertex format");
	}
	// clang-format on

	EG_UNREACHABLE
}

WGPUCompareFunction TranslateCompareOp(CompareOp compareOp)
{
	switch (compareOp)
	{
	case CompareOp::Never: return WGPUCompareFunction_Never;
	case CompareOp::Less: return WGPUCompareFunction_Less;
	case CompareOp::Equal: return WGPUCompareFunction_Equal;
	case CompareOp::LessOrEqual: return WGPUCompareFunction_LessEqual;
	case CompareOp::Greater: return WGPUCompareFunction_Greater;
	case CompareOp::NotEqual: return WGPUCompareFunction_NotEqual;
	case CompareOp::GreaterOrEqual: return WGPUCompareFunction_GreaterEqual;
	case CompareOp::Always: return WGPUCompareFunction_Always;
	}

	EG_UNREACHABLE
}

WGPUShaderStageFlags TranslateShaderStageFlags(ShaderAccessFlags flags)
{
	WGPUShaderStageFlags result = 0;
	if (HasFlag(ShaderAccessFlags::Vertex, flags))
		result |= WGPUShaderStage_Vertex;
	if (HasFlag(ShaderAccessFlags::Fragment, flags))
		result |= WGPUShaderStage_Fragment;
	if (HasFlag(ShaderAccessFlags::Compute, flags))
		result |= WGPUShaderStage_Compute;
	return result;
}

WGPUCullMode TranslateCullMode(CullMode cullMode)
{
	switch (cullMode)
	{
	case CullMode::None: return WGPUCullMode_None;
	case CullMode::Front: return WGPUCullMode_Front;
	case CullMode::Back: return WGPUCullMode_Back;
	}
}
} // namespace eg::graphics_api::webgpu
