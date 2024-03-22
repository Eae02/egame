#include "SpirvCrossUtils.hpp"
#include "Abstraction.hpp"

#include <cstring>
#include <spirv_cross.hpp>
#include <spirv_parser.hpp>

namespace eg
{
void SetSpecializationConstants(const ShaderStageInfo& stageInfo, spirv_cross::Compiler& compiler)
{
	for (spirv_cross::SpecializationConstant& specConst : compiler.get_specialization_constants())
	{
		spirv_cross::SPIRConstant& spirConst = compiler.get_constant(specConst.id);
		if (specConst.constant_id == 500)
		{
			spirConst.m.c[0].r[0].u32 = 1; // TODO
		}
		else
		{
			for (const SpecializationConstantEntry& entry : stageInfo.specConstants)
			{
				if (specConst.constant_id == entry.constantID)
				{
					std::visit(
						[&](auto value) { std::memcpy(spirConst.m.c[0].r, &value, sizeof(uint32_t)); }, entry.value);
					break;
				}
			}
		}
	}
}

uint32_t GetPushConstantBytes(
	const spirv_cross::Compiler& compiler, const spirv_cross::ShaderResources* shaderResources)
{
	spirv_cross::ShaderResources shaderResourcesIfNull;
	if (shaderResources == nullptr)
	{
		shaderResourcesIfNull = compiler.get_shader_resources();
		shaderResources = &shaderResourcesIfNull;
	}

	uint32_t pushConstantBytes = 0;
	for (const spirv_cross::Resource& pcBlock : shaderResources->push_constant_buffers)
	{
		for (const spirv_cross::BufferRange& range : compiler.get_active_buffer_ranges(pcBlock.id))
		{
			pushConstantBytes = std::max(pushConstantBytes, UnsignedNarrow<uint32_t>(range.offset + range.range));
		}
	}

	return pushConstantBytes;
}

void SpirvCrossParsedIRDeleter::operator()(spirv_cross::ParsedIR* parsedIR) const
{
	delete parsedIR;
}

std::unique_ptr<spirv_cross::ParsedIR, SpirvCrossParsedIRDeleter> ParseSpirV(std::span<const uint32_t> spirv)
{
	spirv_cross::Parser parser(spirv.data(), spirv.size());
	parser.parse();
	return std::unique_ptr<spirv_cross::ParsedIR, SpirvCrossParsedIRDeleter>(
		new spirv_cross::ParsedIR(std::move(parser.get_parsed_ir())));
}

void DescriptorSetBindings::AssertAppendOk(AppendResult result, uint32_t set, uint32_t binding)
{
	switch (result)
	{
	case AppendResult::Ok: break;
	case AppendResult::TypeMismatch: EG_PANIC("Descriptor type mismatch for binding " << binding << " in set " << set);
	}
}

DescriptorSetBindings::AppendResult DescriptorSetBindings::Append(uint32_t set, const DescriptorSetBinding& binding)
{
	auto it = std::find_if(
		sets[set].begin(), sets[set].end(),
		[&](const DescriptorSetBinding& b) { return b.binding == binding.binding; });

	if (it != sets[set].end())
	{
		// TODO: Maybe set rwMode to ReadWriteMode::ReadWrite if rwMode mismatches
		if (it->type != binding.type)
			return AppendResult::TypeMismatch;
		it->shaderAccess |= binding.shaderAccess;
	}
	else
	{
		sets[set].push_back(binding);
	}

	return AppendResult::Ok;
}

static TextureViewType GetTextureViewType(const spirv_cross::SPIRType::ImageType& imageType)
{
	switch (imageType.dim)
	{
	case spv::Dim2D: return imageType.arrayed ? TextureViewType::Array2D : TextureViewType::Flat2D;
	case spv::Dim3D: return TextureViewType::Flat3D;
	case spv::DimCube: return imageType.arrayed ? TextureViewType::ArrayCube : TextureViewType::Cube;
	default:
		eg::Log(eg::LogLevel::Error, "spv", "Unsupported SpirV image dimension: {0}", imageType.dim);
		return TextureViewType::Flat2D;
	}
}

static Format TranslateSpvFormat(spv::ImageFormat format)
{
	switch (format)
	{
	case spv::ImageFormatRgba32f: return Format::R32G32B32A32_Float;
	case spv::ImageFormatRgba16f: return Format::R16G16B16A16_Float;
	case spv::ImageFormatR32f: return Format::R32_Float;
	case spv::ImageFormatRgba8: return Format::R8G8B8A8_UNorm;
	case spv::ImageFormatRgba8Snorm: return Format::R8G8B8A8_SNorm;
	case spv::ImageFormatRg32f: return Format::R32G32_Float;
	case spv::ImageFormatRg16f: return Format::R16G16_Float;
	case spv::ImageFormatR11fG11fB10f: return Format::B10G11R11_UFloat;
	case spv::ImageFormatR16f: return Format::R16_Float;
	case spv::ImageFormatRgba16: return Format::R16G16B16A16_UNorm;
	case spv::ImageFormatRg16: return Format::R16G16_UNorm;
	case spv::ImageFormatRg8: return Format::R8G8_UNorm;
	case spv::ImageFormatR16: return Format::R16_UNorm;
	case spv::ImageFormatR8: return Format::R8_UNorm;
	case spv::ImageFormatRgba16Snorm: return Format::R16G16B16A16_SNorm;
	case spv::ImageFormatRg16Snorm: return Format::R16G16_SNorm;
	case spv::ImageFormatRg8Snorm: return Format::R8G8_SNorm;
	case spv::ImageFormatR16Snorm: return Format::R16_SNorm;
	case spv::ImageFormatR8Snorm: return Format::R8_SNorm;
	case spv::ImageFormatRgba32i: return Format::R32G32B32A32_SInt;
	case spv::ImageFormatRgba16i: return Format::R16G16B16A16_SInt;
	case spv::ImageFormatRgba8i: return Format::R8G8B8A8_SInt;
	case spv::ImageFormatR32i: return Format::R32_SInt;
	case spv::ImageFormatRg32i: return Format::R32G32_SInt;
	case spv::ImageFormatRg16i: return Format::R16G16_SInt;
	case spv::ImageFormatRg8i: return Format::R8G8_SInt;
	case spv::ImageFormatR16i: return Format::R16_SInt;
	case spv::ImageFormatR8i: return Format::R8_SInt;
	case spv::ImageFormatRgba32ui: return Format::R32G32B32A32_UInt;
	case spv::ImageFormatRgba16ui: return Format::R16G16B16A16_UInt;
	case spv::ImageFormatRgba8ui: return Format::R8G8B8A8_UInt;
	case spv::ImageFormatR32ui: return Format::R32_UInt;
	case spv::ImageFormatRg32ui: return Format::R32G32_UInt;
	case spv::ImageFormatRg16ui: return Format::R16G16_UInt;
	case spv::ImageFormatRg8ui: return Format::R8G8_UInt;
	case spv::ImageFormatR16ui: return Format::R16_UInt;
	case spv::ImageFormatR8ui: return Format::R8_UInt;
	default: return Format::Undefined;
	}
	EG_UNREACHABLE
}

static TextureSampleMode GetTextureSampleMode(spirv_cross::SPIRType::BaseType baseType)
{
	switch (baseType)
	{
	case spirv_cross::SPIRType::Int:
	case spirv_cross::SPIRType::Short:
	case spirv_cross::SPIRType::SByte:
	case spirv_cross::SPIRType::Int64: //
		return TextureSampleMode::SInt;
	case spirv_cross::SPIRType::UInt:
	case spirv_cross::SPIRType::UShort:
	case spirv_cross::SPIRType::UByte:
	case spirv_cross::SPIRType::UInt64: //
		return TextureSampleMode::UInt;
	case spirv_cross::SPIRType::Half:
	case spirv_cross::SPIRType::Float:
	case spirv_cross::SPIRType::Double: //
		return TextureSampleMode::Float;
	default: //
		eg::Log(eg::LogLevel::Error, "spv", "Unsupported SpirV image type: {0}", baseType);
		return TextureSampleMode::Float;
	}
}

static ReadWriteMode GetReadWriteMode(const spirv_cross::Compiler& compiler, spirv_cross::ID id)
{
	const bool canWrite = !compiler.get_decoration(id, spv::DecorationNonWritable);
	const bool canRead = !compiler.get_decoration(id, spv::DecorationNonReadable);

	if (canRead && !canWrite)
		return ReadWriteMode::ReadOnly;
	if (!canRead && canWrite)
		return ReadWriteMode::WriteOnly;
	return ReadWriteMode::ReadWrite;
}

void DescriptorSetBindings::AppendFromReflectionInfo(
	ShaderStage stage, const spirv_cross::Compiler& compiler, const spirv_cross::ShaderResources& shaderResources)
{
	ShaderAccessFlags accessFlags = static_cast<ShaderAccessFlags>(1 << static_cast<int>(stage));

	auto ProcessResources = [&](const spirv_cross::SmallVector<spirv_cross::Resource>& resources, BindingType type)
	{
		for (const spirv_cross::Resource& resource : resources)
		{
			const uint32_t set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			const uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);

			const bool useDynamicOffset = resource.name.ends_with("_UseDynamicOffset");

			DescriptorSetBinding descriptorSetBinding = {
				.binding = binding,
				.shaderAccess = accessFlags,
			};

			switch (type)
			{
			case BindingType::UniformBuffer:
				descriptorSetBinding.type = BindingTypeUniformBuffer{ .dynamicOffset = useDynamicOffset };
				break;
			case BindingType::StorageBuffer:
			{
				auto flags = compiler.get_buffer_block_flags(resource.id);

				// TODO: Handle if we have both flags
				ReadWriteMode rwMode = ReadWriteMode::ReadWrite;
				if (flags.get(spv::DecorationNonWritable))
					rwMode = ReadWriteMode::ReadOnly;
				else if (flags.get(spv::DecorationNonReadable))
					rwMode = ReadWriteMode::WriteOnly;

				descriptorSetBinding.type = BindingTypeStorageBuffer{
					.dynamicOffset = useDynamicOffset,
					.rwMode = rwMode,
				};
				break;
			}
			case BindingType::Texture:
			{
				const auto& imageType = compiler.get_type(resource.type_id).image;
				descriptorSetBinding.type = BindingTypeTexture{
					.viewType = GetTextureViewType(imageType),
					.sampleMode = resource.name.ends_with("_UF")
					                  ? TextureSampleMode::UnfilterableFloat
					                  : GetTextureSampleMode(compiler.get_type(imageType.type).basetype),
					.multisample = imageType.ms,
				};
				break;
			}
			case BindingType::StorageImage:
			{
				const auto& imageType = compiler.get_type(resource.type_id).image;
				descriptorSetBinding.type = BindingTypeStorageImage{
					.viewType = GetTextureViewType(imageType),
					.format = TranslateSpvFormat(imageType.format),
					.rwMode = GetReadWriteMode(compiler, resource.id),
				};
				break;
			}

			case BindingType::Sampler:
				if (resource.name.ends_with("_ReflShadow"))
					descriptorSetBinding.type = BindingTypeSampler::Compare;
				else if (resource.name.ends_with("_UF"))
					descriptorSetBinding.type = BindingTypeSampler::Nearest;
				else
					descriptorSetBinding.type = BindingTypeSampler::Default;
				break;
			}

			AppendResult result = Append(set, descriptorSetBinding);
			AssertAppendOk(result, set, binding);
		}
	};

	if (!shaderResources.sampled_images.empty())
		EG_PANIC("Shader resources contains combined image sampler");

	ProcessResources(shaderResources.uniform_buffers, BindingType::UniformBuffer);
	ProcessResources(shaderResources.storage_buffers, BindingType::StorageBuffer);
	ProcessResources(shaderResources.separate_images, BindingType::Texture);
	ProcessResources(shaderResources.storage_images, BindingType::StorageImage);
	ProcessResources(shaderResources.separate_samplers, BindingType::Sampler);
}

void DescriptorSetBindings::AppendFrom(const DescriptorSetBindings& other)
{
	for (uint32_t set = 0; set < MAX_DESCRIPTOR_SETS; set++)
	{
		for (const DescriptorSetBinding& binding : other.sets[set])
		{
			AssertAppendOk(Append(set, binding), set, binding.binding);
		}
	}
}

void DescriptorSetBindings::SortByBinding()
{
	for (uint32_t set = 0; set < MAX_DESCRIPTOR_SETS; set++)
	{
		std::sort(sets[set].begin(), sets[set].end(), DescriptorSetBinding::BindingCmp());
	}
}
} // namespace eg
