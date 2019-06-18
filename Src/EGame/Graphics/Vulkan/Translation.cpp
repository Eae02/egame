#ifndef EG_NO_VULKAN
#include "Translation.hpp"

namespace eg::graphics_api::vk
{
	VkFormat GetAttribFormat(DataType dataType, uint32_t components)
	{
		switch (dataType)
		{
		case DataType::Float32:
			switch (components)
			{
			case 1: return VK_FORMAT_R32_SFLOAT;
			case 2: return VK_FORMAT_R32G32_SFLOAT;
			case 3: return VK_FORMAT_R32G32B32_SFLOAT;
			case 4: return VK_FORMAT_R32G32B32A32_SFLOAT;
			}
			break;
		case DataType::UInt8Norm:
			switch (components)
			{
			case 1: return VK_FORMAT_R8_UNORM;
			case 2: return VK_FORMAT_R8G8_UNORM;
			case 3: return VK_FORMAT_R8G8B8_UNORM;
			case 4: return VK_FORMAT_R8G8B8A8_UNORM;
			}
			break;
		case DataType::UInt16Norm:
			switch (components)
			{
			case 1: return VK_FORMAT_R16_UNORM;
			case 2: return VK_FORMAT_R16G16_UNORM;
			case 3: return VK_FORMAT_R16G16B16_UNORM;
			case 4: return VK_FORMAT_R16G16B16A16_UNORM;
			}
			break;
		case DataType::SInt8Norm:
			switch (components)
			{
			case 1: return VK_FORMAT_R8_SNORM;
			case 2: return VK_FORMAT_R8G8_SNORM;
			case 3: return VK_FORMAT_R8G8B8_SNORM;
			case 4: return VK_FORMAT_R8G8B8A8_SNORM;
			}
			break;
		case DataType::SInt16Norm:
			switch (components)
			{
			case 1: return VK_FORMAT_R16_SNORM;
			case 2: return VK_FORMAT_R16G16_SNORM;
			case 3: return VK_FORMAT_R16G16B16_SNORM;
			case 4: return VK_FORMAT_R16G16B16A16_SNORM;
			}
			break;
		case DataType::UInt8:
			switch (components)
			{
			case 1: return VK_FORMAT_R8_UINT;
			case 2: return VK_FORMAT_R8G8_UINT;
			case 3: return VK_FORMAT_R8G8B8_UINT;
			case 4: return VK_FORMAT_R8G8B8A8_UINT;
			}
			break;
		case DataType::UInt16:
			switch (components)
			{
			case 1: return VK_FORMAT_R16_UINT;
			case 2: return VK_FORMAT_R16G16_UINT;
			case 3: return VK_FORMAT_R16G16B16_UINT;
			case 4: return VK_FORMAT_R16G16B16A16_UINT;
			}
			break;
		case DataType::UInt32:
			switch (components)
			{
			case 1: return VK_FORMAT_R32_UINT;
			case 2: return VK_FORMAT_R32G32_UINT;
			case 3: return VK_FORMAT_R32G32B32_UINT;
			case 4: return VK_FORMAT_R32G32B32A32_UINT;
			}
			break;
		case DataType::SInt8:
			switch (components)
			{
			case 1: return VK_FORMAT_R8_SINT;
			case 2: return VK_FORMAT_R8G8_SINT;
			case 3: return VK_FORMAT_R8G8B8_SINT;
			case 4: return VK_FORMAT_R8G8B8A8_SINT;
			}
			break;
		case DataType::SInt16:
			switch (components)
			{
			case 1: return VK_FORMAT_R16_SINT;
			case 2: return VK_FORMAT_R16G16_SINT;
			case 3: return VK_FORMAT_R16G16B16_SINT;
			case 4: return VK_FORMAT_R16G16B16A16_SINT;
			}
			break;
		case DataType::SInt32:
			switch (components)
			{
			case 1: return VK_FORMAT_R32_SINT;
			case 2: return VK_FORMAT_R32G32_SINT;
			case 3: return VK_FORMAT_R32G32B32_SINT;
			case 4: return VK_FORMAT_R32G32B32A32_SINT;
			}
			break;
		}
		EG_PANIC("Invalid vertex attribute format");
	}
	
	VkBlendOp TranslateBlendFunc(BlendFunc blendFunc)
	{
		switch (blendFunc)
		{
		case BlendFunc::Add: return VK_BLEND_OP_ADD;
		case BlendFunc::Subtract: return VK_BLEND_OP_SUBTRACT;
		case BlendFunc::ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
		case BlendFunc::Min: return VK_BLEND_OP_MIN;
		case BlendFunc::Max: return VK_BLEND_OP_MAX;
		}
		EG_UNREACHABLE
	}
	
	VkBlendFactor TranslateBlendFactor(BlendFactor blendFactor)
	{
		switch (blendFactor)
		{
		case BlendFactor::Zero: return VK_BLEND_FACTOR_ZERO;
		case BlendFactor::One: return VK_BLEND_FACTOR_ONE;
		case BlendFactor::SrcColor: return VK_BLEND_FACTOR_SRC_COLOR;
		case BlendFactor::OneMinusSrcColor: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		case BlendFactor::DstColor: return VK_BLEND_FACTOR_DST_COLOR;
		case BlendFactor::OneMinusDstColor: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		case BlendFactor::SrcAlpha: return VK_BLEND_FACTOR_SRC_ALPHA;
		case BlendFactor::OneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		case BlendFactor::DstAlpha: return VK_BLEND_FACTOR_DST_ALPHA;
		case BlendFactor::OneMinusDstAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		case BlendFactor::ConstantColor: return VK_BLEND_FACTOR_CONSTANT_COLOR;
		case BlendFactor::OneMinusConstantColor: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
		case BlendFactor::ConstantAlpha: return VK_BLEND_FACTOR_CONSTANT_COLOR;
		case BlendFactor::OneMinusConstantAlpha: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
		}
		EG_UNREACHABLE
	}
	
	VkCullModeFlags TranslateCullMode(CullMode mode)
	{
		switch (mode)
		{
		case CullMode::None: return VK_CULL_MODE_NONE;
		case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
		case CullMode::Back: return VK_CULL_MODE_BACK_BIT;
		}
		EG_UNREACHABLE
	}
	
	VkFormat TranslateFormat(Format format)
	{
		switch (format)
		{
		case Format::Undefined: return VK_FORMAT_UNDEFINED;
		case Format::DefaultColor: return ctx.surfaceFormat.format;
		case Format::DefaultDepthStencil: return ctx.defaultDSFormat;
		case Format::R8_UNorm: return VK_FORMAT_R8_UNORM;
		case Format::R8_UInt: return VK_FORMAT_R8_UINT;
		case Format::R8_SInt: return VK_FORMAT_R8_SINT;
		case Format::R16_UInt: return VK_FORMAT_R16_UINT;
		case Format::R16_SInt: return VK_FORMAT_R16_SINT;
		case Format::R16_Float: return VK_FORMAT_R16_SFLOAT;
		case Format::R32_UInt: return VK_FORMAT_R32_UINT;
		case Format::R32_SInt: return VK_FORMAT_R32_SINT;
		case Format::R32_Float: return VK_FORMAT_R32_SFLOAT;
		case Format::R8G8_UNorm: return VK_FORMAT_R8G8_UNORM;
		case Format::R8G8_UInt: return VK_FORMAT_R8G8_UINT;
		case Format::R8G8_SInt: return VK_FORMAT_R8G8_SINT;
		case Format::R16G16_UInt: return VK_FORMAT_R16G16_UINT;
		case Format::R16G16_SInt: return VK_FORMAT_R16G16_SINT;
		case Format::R16G16_Float: return VK_FORMAT_R16G16_SFLOAT;
		case Format::R32G32_UInt: return VK_FORMAT_R32G32_UINT;
		case Format::R32G32_SInt: return VK_FORMAT_R32G32_SINT;
		case Format::R32G32_Float: return VK_FORMAT_R32G32_SFLOAT;
		case Format::R8G8B8_sRGB: return VK_FORMAT_R8G8B8_SRGB;
		case Format::R16G16B16_UInt: return VK_FORMAT_R16G16B16_UINT;
		case Format::R16G16B16_SInt: return VK_FORMAT_R16G16B16_SINT;
		case Format::R16G16B16_Float: return VK_FORMAT_R16G16B16_SFLOAT;
		case Format::R32G32B32_UInt: return VK_FORMAT_R32G32B32_UINT;
		case Format::R32G32B32_SInt: return VK_FORMAT_R32G32B32_SINT;
		case Format::R32G32B32_Float: return VK_FORMAT_R32G32B32_SFLOAT;
		case Format::R8G8B8A8_sRGB: return VK_FORMAT_R8G8B8A8_SRGB;
		case Format::R8G8B8A8_UNorm: return VK_FORMAT_R8G8B8A8_UNORM;
		case Format::R8G8B8A8_UInt: return VK_FORMAT_R8G8B8A8_UINT;
		case Format::R8G8B8A8_SInt: return VK_FORMAT_R8G8B8A8_SINT;
		case Format::R16G16B16A16_UInt: return VK_FORMAT_R16G16B16A16_UINT;
		case Format::R16G16B16A16_SInt: return VK_FORMAT_R16G16B16A16_SINT;
		case Format::R16G16B16A16_Float: return VK_FORMAT_R16G16B16A16_SFLOAT;
		case Format::R32G32B32A32_UInt: return VK_FORMAT_R32G32B32A32_UINT;
		case Format::R32G32B32A32_SInt: return VK_FORMAT_R32G32B32A32_SINT;
		case Format::R32G32B32A32_Float: return VK_FORMAT_R32G32B32A32_SFLOAT;
		case Format::BC1_RGBA_UNorm: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
		case Format::BC1_RGBA_sRGB: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
		case Format::BC1_RGB_UNorm: return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
		case Format::BC1_RGB_sRGB: return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
		case Format::BC3_UNorm: return VK_FORMAT_BC3_UNORM_BLOCK;
		case Format::BC3_sRGB: return VK_FORMAT_BC3_SRGB_BLOCK;
		case Format::BC4_UNorm: return VK_FORMAT_BC4_UNORM_BLOCK;
		case Format::BC5_UNorm: return VK_FORMAT_BC5_UNORM_BLOCK;
		case Format::Depth16: return VK_FORMAT_D16_UNORM;
		case Format::Depth32: return VK_FORMAT_D32_SFLOAT;
		case Format::Depth24Stencil8: return VK_FORMAT_D24_UNORM_S8_UINT;
		case Format::Depth32Stencil8: return VK_FORMAT_D32_SFLOAT_S8_UINT;
		}
		EG_UNREACHABLE
	}
	
	VkCompareOp TranslateCompareOp(CompareOp op)
	{
		switch (op)
		{
		case CompareOp::Never: return VK_COMPARE_OP_NEVER;
		case CompareOp::Less: return VK_COMPARE_OP_LESS;
		case CompareOp::Equal: return VK_COMPARE_OP_EQUAL;
		case CompareOp::LessOrEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
		case CompareOp::Greater: return VK_COMPARE_OP_GREATER;
		case CompareOp::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
		case CompareOp::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
		case CompareOp::Always: return VK_COMPARE_OP_ALWAYS;
		}
		EG_UNREACHABLE
	}
	
	VkStencilOp TranslateStencilOp(StencilOp op)
	{
		switch (op)
		{
		case StencilOp::Keep: return VK_STENCIL_OP_KEEP;
		case StencilOp::Zero: return VK_STENCIL_OP_ZERO;
		case StencilOp::Replace: return VK_STENCIL_OP_REPLACE;
		case StencilOp::IncrementAndClamp: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
		case StencilOp::DecrementAndClamp: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
		case StencilOp::Invert: return VK_STENCIL_OP_INVERT;
		case StencilOp::IncrementAndWrap: return VK_STENCIL_OP_INCREMENT_AND_WRAP;
		case StencilOp::DecrementAndWrap: return VK_STENCIL_OP_DECREMENT_AND_WRAP;
		}
		EG_UNREACHABLE
	}
	
	VkAccessFlags TranslateShaderAccess(ShaderAccessFlags accessFlags)
	{
		VkAccessFlags flags = 0;
		if (HasFlag(accessFlags, ShaderAccessFlags::Vertex))
			flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
		if (HasFlag(accessFlags, ShaderAccessFlags::Fragment))
			flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		if (HasFlag(accessFlags, ShaderAccessFlags::Geometry))
			flags |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
		if (HasFlag(accessFlags, ShaderAccessFlags::TessControl))
			flags |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
		if (HasFlag(accessFlags, ShaderAccessFlags::TessEvaluation))
			flags |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
		if (HasFlag(accessFlags, ShaderAccessFlags::Compute))
			flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		return flags;
	}
	
	VkShaderStageFlags TranslateShaderStage(ShaderAccessFlags accessFlags)
	{
		VkShaderStageFlags flags = 0;
		if (HasFlag(accessFlags, ShaderAccessFlags::Vertex))
			flags |= VK_SHADER_STAGE_VERTEX_BIT;
		if (HasFlag(accessFlags, ShaderAccessFlags::Fragment))
			flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
		if (HasFlag(accessFlags, ShaderAccessFlags::Geometry))
			flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
		if (HasFlag(accessFlags, ShaderAccessFlags::TessControl))
			flags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
		if (HasFlag(accessFlags, ShaderAccessFlags::TessEvaluation))
			flags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
		if (HasFlag(accessFlags, ShaderAccessFlags::Compute))
			flags |= VK_SHADER_STAGE_COMPUTE_BIT;
		return flags;
	}
	
	VkDescriptorType TranslateBindingType(BindingType bindingType)
	{
		switch (bindingType)
		{
		case BindingType::UniformBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		case BindingType::Texture: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		case BindingType::StorageImage: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		}
		EG_UNREACHABLE;
	}
}

#endif
