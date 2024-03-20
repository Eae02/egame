#include "MetalTranslation.hpp"
#include <Metal/MTLVertexDescriptor.hpp>

#include "../../Assert.hpp"
#include "EGame/Log.hpp"

namespace eg::graphics_api::mtl
{
MTL::PixelFormat defaultColorPixelFormat;
MTL::PixelFormat defaultDepthPixelFormat;

MTL::PixelFormat TranslatePixelFormat(Format format)
{
	// clang-format off
	switch (format)
	{
	case Format::DefaultColor:         return defaultColorPixelFormat;
	case Format::DefaultDepthStencil:  return defaultDepthPixelFormat;
	case Format::R8_SNorm:             return MTL::PixelFormatR8Snorm;
	case Format::R8_UNorm:             return MTL::PixelFormatR8Unorm;
	case Format::R8_UInt:              return MTL::PixelFormatR8Uint;
	case Format::R8_SInt:              return MTL::PixelFormatR8Sint;
	case Format::R16_UNorm:            return MTL::PixelFormatR16Unorm;
	case Format::R16_SNorm:            return MTL::PixelFormatR16Snorm;
	case Format::R16_UInt:             return MTL::PixelFormatR16Uint;
	case Format::R16_SInt:             return MTL::PixelFormatR16Sint;
	case Format::R16_Float:            return MTL::PixelFormatR16Float;
	case Format::R32_UInt:             return MTL::PixelFormatR32Uint;
	case Format::R32_SInt:             return MTL::PixelFormatR32Sint;
	case Format::R32_Float:            return MTL::PixelFormatR32Float;
	case Format::R8G8_UNorm:           return MTL::PixelFormatRG8Unorm;
	case Format::R8G8_SNorm:           return MTL::PixelFormatRG8Snorm;
	case Format::R8G8_UInt:            return MTL::PixelFormatRG8Uint;
	case Format::R8G8_SInt:            return MTL::PixelFormatRG8Sint;
	case Format::R16G16_UNorm:         return MTL::PixelFormatRG16Unorm;
	case Format::R16G16_SNorm:         return MTL::PixelFormatRG16Snorm;
	case Format::R16G16_UInt:          return MTL::PixelFormatRG16Uint;
	case Format::R16G16_SInt:          return MTL::PixelFormatRG16Sint;
	case Format::R16G16_Float:         return MTL::PixelFormatRG16Float;
	case Format::R32G32_UInt:          return MTL::PixelFormatRG32Uint;
	case Format::R32G32_SInt:          return MTL::PixelFormatRG32Sint;
	case Format::R32G32_Float:         return MTL::PixelFormatRG32Float;
	case Format::R32G32B32_UInt:       return MTL::PixelFormatRGBA32Uint;
	case Format::R32G32B32_SInt:       return MTL::PixelFormatRGBA32Sint;
	case Format::R32G32B32_Float:      return MTL::PixelFormatRGBA32Float;
	case Format::R8G8B8A8_sRGB:        return MTL::PixelFormatRGBA8Unorm_sRGB;
	case Format::R8G8B8A8_UNorm:       return MTL::PixelFormatRGBA8Unorm;
	case Format::R8G8B8A8_SNorm:       return MTL::PixelFormatRGBA8Snorm;
	case Format::R8G8B8A8_UInt:        return MTL::PixelFormatRGBA8Uint;
	case Format::R8G8B8A8_SInt:        return MTL::PixelFormatRGBA8Sint;
	case Format::R16G16B16A16_UNorm:   return MTL::PixelFormatRGBA16Unorm;
	case Format::R16G16B16A16_SNorm:   return MTL::PixelFormatRGBA16Snorm;
	case Format::R16G16B16A16_UInt:    return MTL::PixelFormatRGBA16Uint;
	case Format::R16G16B16A16_SInt:    return MTL::PixelFormatRGBA16Sint;
	case Format::R16G16B16A16_Float:   return MTL::PixelFormatRGBA16Float;
	case Format::R32G32B32A32_UInt:    return MTL::PixelFormatRGBA32Uint;
	case Format::R32G32B32A32_SInt:    return MTL::PixelFormatRGBA32Sint;
	case Format::R32G32B32A32_Float:   return MTL::PixelFormatRGBA32Float;
	case Format::A2R10G10B10_UInt:     return MTL::PixelFormatRGB10A2Uint;
	case Format::A2R10G10B10_UNorm:    return MTL::PixelFormatRGB10A2Unorm;
	case Format::B10G11R11_UFloat:     return MTL::PixelFormatRG11B10Float;
	case Format::BC1_RGBA_UNorm:       return MTL::PixelFormatBC1_RGBA;
	case Format::BC1_RGBA_sRGB:        return MTL::PixelFormatBC1_RGBA_sRGB;
	case Format::BC1_RGB_UNorm:        return MTL::PixelFormatBC1_RGBA;     //?
	case Format::BC1_RGB_sRGB:         return MTL::PixelFormatBC1_RGBA_sRGB; //?
	case Format::BC3_UNorm:            return MTL::PixelFormatBC3_RGBA;
	case Format::BC3_sRGB:             return MTL::PixelFormatBC3_RGBA_sRGB;
	case Format::BC4_UNorm:            return MTL::PixelFormatBC4_RUnorm;
	case Format::BC5_UNorm:            return MTL::PixelFormatBC5_RGUnorm;
	case Format::Depth16:              return MTL::PixelFormatDepth16Unorm;
	case Format::Depth32:              return MTL::PixelFormatDepth32Float;
	case Format::Depth24Stencil8:      return MTL::PixelFormatDepth24Unorm_Stencil8;
	case Format::Depth32Stencil8:      return MTL::PixelFormatDepth32Float_Stencil8;
	
	case Format::A2R10G10B10_SInt:
	case Format::A2R10G10B10_SNorm:
	case Format::Undefined:
		eg::Log(eg::LogLevel::Warning, "mtl", "Attempted to translate an unknown format: {0}", FormatToString(format));
		return MTL::PixelFormatInvalid;
}
	// clang-format on
}

MTL::VertexFormat TranslateVertexFormat(Format format)
{
	// clang-format off
	switch (format)
	{
	case Format::R8_SNorm:             return MTL::VertexFormatCharNormalized;
	case Format::R8_UNorm:             return MTL::VertexFormatUCharNormalized;
	case Format::R8_UInt:              return MTL::VertexFormatChar;
	case Format::R8_SInt:              return MTL::VertexFormatUChar;
	case Format::R16_UNorm:            return MTL::VertexFormatUShortNormalized;
	case Format::R16_SNorm:            return MTL::VertexFormatShortNormalized;
	case Format::R16_UInt:             return MTL::VertexFormatUShort;
	case Format::R16_SInt:             return MTL::VertexFormatShort;
	case Format::R16_Float:            return MTL::VertexFormatHalf;
	case Format::R32_UInt:             return MTL::VertexFormatUInt;
	case Format::R32_SInt:             return MTL::VertexFormatInt;
	case Format::R32_Float:            return MTL::VertexFormatFloat;
	case Format::R8G8_UNorm:           return MTL::VertexFormatUChar2Normalized;
	case Format::R8G8_SNorm:           return MTL::VertexFormatChar2Normalized;
	case Format::R8G8_UInt:            return MTL::VertexFormatUChar2;
	case Format::R8G8_SInt:            return MTL::VertexFormatChar2;
	case Format::R16G16_UNorm:         return MTL::VertexFormatUShort2Normalized;
	case Format::R16G16_SNorm:         return MTL::VertexFormatShort2Normalized;
	case Format::R16G16_UInt:          return MTL::VertexFormatUShort2;
	case Format::R16G16_SInt:          return MTL::VertexFormatShort2;
	case Format::R16G16_Float:         return MTL::VertexFormatHalf2;
	case Format::R32G32_UInt:          return MTL::VertexFormatUInt2;
	case Format::R32G32_SInt:          return MTL::VertexFormatInt2;
	case Format::R32G32_Float:         return MTL::VertexFormatFloat2;
	case Format::R32G32B32_UInt:       return MTL::VertexFormatUInt3;
	case Format::R32G32B32_SInt:       return MTL::VertexFormatInt3;
	case Format::R32G32B32_Float:      return MTL::VertexFormatFloat3;
	case Format::R8G8B8A8_UNorm:       return MTL::VertexFormatUChar4Normalized;
	case Format::R8G8B8A8_SNorm:       return MTL::VertexFormatChar4Normalized;
	case Format::R8G8B8A8_UInt:        return MTL::VertexFormatUChar4;
	case Format::R8G8B8A8_SInt:        return MTL::VertexFormatChar4;
	case Format::R16G16B16A16_UNorm:   return MTL::VertexFormatUShort4Normalized;
	case Format::R16G16B16A16_SNorm:   return MTL::VertexFormatShort4Normalized;
	case Format::R16G16B16A16_UInt:    return MTL::VertexFormatUShort4;
	case Format::R16G16B16A16_SInt:    return MTL::VertexFormatShort4;
	case Format::R16G16B16A16_Float:   return MTL::VertexFormatHalf4;
	case Format::R32G32B32A32_UInt:    return MTL::VertexFormatUInt4;
	case Format::R32G32B32A32_SInt:    return MTL::VertexFormatInt4;
	case Format::R32G32B32A32_Float:   return MTL::VertexFormatFloat4;
	case Format::A2R10G10B10_UNorm:    return MTL::VertexFormatUInt1010102Normalized;
	case Format::A2R10G10B10_SNorm:    return MTL::VertexFormatInt1010102Normalized;
	case Format::B10G11R11_UFloat:     return MTL::VertexFormatFloatRG11B10;
	
	case Format::A2R10G10B10_UInt:
	case Format::A2R10G10B10_SInt:
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
	case Format::Undefined:
	case Format::DefaultColor:
	case Format::DefaultDepthStencil:
		EG_PANIC("Unsupported vertex format");
}
	// clang-format on

	EG_UNREACHABLE
}

MTL::CompareFunction TranslateCompareOp(CompareOp compareOp)
{
	switch (compareOp)
	{
	case CompareOp::Never: return MTL::CompareFunctionNever;
	case CompareOp::Less: return MTL::CompareFunctionLess;
	case CompareOp::Equal: return MTL::CompareFunctionEqual;
	case CompareOp::LessOrEqual: return MTL::CompareFunctionLessEqual;
	case CompareOp::Greater: return MTL::CompareFunctionGreater;
	case CompareOp::NotEqual: return MTL::CompareFunctionNotEqual;
	case CompareOp::GreaterOrEqual: return MTL::CompareFunctionGreaterEqual;
	case CompareOp::Always: return MTL::CompareFunctionAlways;
	}
	EG_UNREACHABLE
}

MTL::BlendOperation TranslateBlendFunc(BlendFunc blendFunc)
{
	switch (blendFunc)
	{
	case BlendFunc::Add: return MTL::BlendOperationAdd;
	case BlendFunc::Subtract: return MTL::BlendOperationSubtract;
	case BlendFunc::ReverseSubtract: return MTL::BlendOperationReverseSubtract;
	case BlendFunc::Min: return MTL::BlendOperationMin;
	case BlendFunc::Max: return MTL::BlendOperationMax;
	}
	EG_UNREACHABLE
}

MTL::BlendFactor TranslateBlendFactor(BlendFactor blendFactor)
{
	switch (blendFactor)
	{
	case BlendFactor::Zero: return MTL::BlendFactorZero;
	case BlendFactor::One: return MTL::BlendFactorOne;
	case BlendFactor::SrcColor: return MTL::BlendFactorSourceColor;
	case BlendFactor::OneMinusSrcColor: return MTL::BlendFactorOneMinusSourceColor;
	case BlendFactor::DstColor: return MTL::BlendFactorDestinationColor;
	case BlendFactor::OneMinusDstColor: return MTL::BlendFactorOneMinusDestinationColor;
	case BlendFactor::SrcAlpha: return MTL::BlendFactorSourceAlpha;
	case BlendFactor::OneMinusSrcAlpha: return MTL::BlendFactorOneMinusSourceAlpha;
	case BlendFactor::DstAlpha: return MTL::BlendFactorDestinationAlpha;
	case BlendFactor::OneMinusDstAlpha: return MTL::BlendFactorOneMinusDestinationAlpha;
	case BlendFactor::ConstantColor: return MTL::BlendFactorBlendColor;
	case BlendFactor::OneMinusConstantColor: return MTL::BlendFactorOneMinusBlendColor;
	case BlendFactor::ConstantAlpha: return MTL::BlendFactorBlendAlpha;
	case BlendFactor::OneMinusConstantAlpha: return MTL::BlendFactorOneMinusBlendAlpha;
	}
	EG_UNREACHABLE
}

MTL::CullMode TranslateCullMode(CullMode cullMode)
{
	switch (cullMode)
	{
	case CullMode::None: return MTL::CullModeNone;
	case CullMode::Front: return MTL::CullModeFront;
	case CullMode::Back: return MTL::CullModeBack;
	}
}
} // namespace eg::graphics_api::mtl
