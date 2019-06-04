#include "Utils.hpp"
#include "../../Utils.hpp"

namespace eg::graphics_api::gl
{
	GLenum TranslateFormat(Format format)
	{
		switch (format)
		{
		case Format::DefaultColor: return GL_RGBA8;
		case Format::DefaultDepthStencil: return GL_DEPTH_COMPONENT32F;
		case Format::R8_UNorm: return GL_R8;
		case Format::R8_UInt: return GL_R8UI;
		case Format::R8_SInt: return GL_R8I;
		case Format::R16_UInt: return GL_R16UI;
		case Format::R16_SInt: return GL_R16I;
		case Format::R16_Float: return GL_R16F;
		case Format::R32_UInt: return GL_R32UI;
		case Format::R32_SInt: return GL_R32I;
		case Format::R32_Float: return GL_R32F;
		case Format::R8G8_UNorm: return GL_RG8;
		case Format::R8G8_UInt: return GL_RG8UI;
		case Format::R8G8_SInt: return GL_RG8I;
		case Format::R16G16_UInt: return GL_RG16UI;
		case Format::R16G16_SInt: return GL_RG16I;
		case Format::R16G16_Float: return GL_RG16F;
		case Format::R32G32_UInt: return GL_RG32UI;
		case Format::R32G32_SInt: return GL_RG32I;
		case Format::R32G32_Float: return GL_RG32F;
		case Format::R8G8B8_sRGB: return GL_SRGB8;
		case Format::R16G16B16_UInt: return GL_RGB16UI;
		case Format::R16G16B16_SInt: return GL_RGB16I;
		case Format::R16G16B16_Float: return GL_RGB16F;
		case Format::R32G32B32_UInt: return GL_RGB32UI;
		case Format::R32G32B32_SInt: return GL_RGB32I;
		case Format::R32G32B32_Float: return GL_RGB32F;
		case Format::R8G8B8A8_sRGB: return GL_SRGB8_ALPHA8;
		case Format::R8G8B8A8_UNorm: return GL_RGBA8;
		case Format::R8G8B8A8_UInt: return GL_RGBA8UI;
		case Format::R8G8B8A8_SInt: return GL_RGBA8I;
		case Format::R16G16B16A16_UInt: return GL_RGBA16UI;
		case Format::R16G16B16A16_SInt: return GL_RGBA16I;
		case Format::R16G16B16A16_Float: return GL_RGBA16F;
		case Format::R32G32B32A32_UInt: return GL_RGBA32UI;
		case Format::R32G32B32A32_SInt: return GL_RGBA32I;
		case Format::R32G32B32A32_Float: return GL_RGBA32F;
		case Format::BC1_RGBA_UNorm: return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		case Format::BC1_RGBA_sRGB: return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
		case Format::BC1_RGB_UNorm: return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
		case Format::BC1_RGB_sRGB: return GL_COMPRESSED_SRGB_S3TC_DXT1_EXT;
		case Format::BC3_UNorm: return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		case Format::BC3_sRGB: return GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
		case Format::BC4_UNorm: return GL_COMPRESSED_RED_RGTC1;
		case Format::BC5_UNorm: return GL_COMPRESSED_RG_RGTC2;
		case Format::Depth16: return GL_DEPTH_COMPONENT16;
		case Format::Depth32: return GL_DEPTH_COMPONENT32F;
		case Format::Depth24Stencil8: return GL_DEPTH24_STENCIL8;
		case Format::Depth32Stencil8: return GL_DEPTH32F_STENCIL8;
		}
		EG_UNREACHABLE
	}
	
	GLenum TranslateDataType(DataType type)
	{
		switch (type)
		{
		case DataType::Float32:
			return GL_FLOAT;
		case DataType::UInt8:
		case DataType::UInt8Norm:
			return GL_UNSIGNED_BYTE;
		case DataType::UInt16:
		case DataType::UInt16Norm:
			return GL_UNSIGNED_SHORT;
		case DataType::UInt32:
			return GL_UNSIGNED_INT;
		case DataType::SInt8:
		case DataType::SInt8Norm:
			return GL_BYTE;
		case DataType::SInt16:
		case DataType::SInt16Norm:
			return GL_SHORT;
		case DataType::SInt32:
			return GL_INT;
		}
		
		EG_UNREACHABLE
	}
	
	GLenum TranslateCompareOp(CompareOp compareOp)
	{
		switch (compareOp)
		{
		case CompareOp::Never: return GL_NEVER;
		case CompareOp::Less: return GL_LESS;
		case CompareOp::Equal: return GL_EQUAL;
		case CompareOp::LessOrEqual: return GL_LEQUAL;
		case CompareOp::Greater: return GL_GREATER;
		case CompareOp::NotEqual: return GL_NOTEQUAL;
		case CompareOp::GreaterOrEqual: return GL_GEQUAL;
		case CompareOp::Always: return GL_ALWAYS;
		}
		EG_UNREACHABLE
	}
	
	std::optional<UniformType> GetUniformType(GLenum glType)
	{
		switch (glType)
		{
		case GL_INT: return UniformType::Int;
		case GL_FLOAT: return UniformType::Float;
		case GL_FLOAT_VEC2: return UniformType::Vec2;
		case GL_FLOAT_VEC3: return UniformType::Vec3;
		case GL_FLOAT_VEC4: return UniformType::Vec4;
		case GL_INT_VEC2: return UniformType::IVec2;
		case GL_INT_VEC3: return UniformType::IVec3;
		case GL_INT_VEC4: return UniformType::IVec4;
		case GL_FLOAT_MAT3: return UniformType::Mat3;
		case GL_FLOAT_MAT4: return UniformType::Mat4;
		default: return { };
		}
	}
	
	std::vector<GLenum> insertedBarriers;
	
	void MaybeInsertBarrier(GLenum barrier)
	{
#ifndef EG_WEB
		if (!Contains(insertedBarriers, barrier))
		{
			glMemoryBarrier(barrier);
			insertedBarriers.push_back(barrier);
		}
#endif
	}
}
