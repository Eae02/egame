#include "SpirvCrossUtils.hpp"
#include "Abstraction.hpp"

#include <cstring>
#include <spirv_cross.hpp>
#include <spirv_parser.hpp>

namespace eg
{
void SetSpecializationConstants(const ShaderStageInfo& stageInfo, spirv_cross::Compiler& compiler)
{
	const char* dataChar = reinterpret_cast<const char*>(stageInfo.specConstantsData);

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
					std::memcpy(spirConst.m.c[0].r, dataChar + entry.offset, entry.size);
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
	case AppendResult::CountMismatch:
		EG_PANIC("Descriptor count mismatch for binding " << binding << " in set " << set);
	}
}

DescriptorSetBindings::AppendResult DescriptorSetBindings::Append(uint32_t set, const DescriptorSetBinding& binding)
{
	auto it = std::find_if(
		sets[set].begin(), sets[set].end(),
		[&](const DescriptorSetBinding& b) { return b.binding == binding.binding; });

	if (it != sets[set].end())
	{
		if (it->type != binding.type)
			return AppendResult::TypeMismatch;
		if (it->count != binding.count)
			return AppendResult::CountMismatch;
		if (it->rwMode != binding.rwMode)
			it->rwMode = ReadWriteMode::ReadWrite;
		it->shaderAccess |= binding.shaderAccess;
	}
	else
	{
		sets[set].push_back(binding);
	}

	return AppendResult::Ok;
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

			const bool canWrite = !compiler.get_decoration(resource.id, spv::DecorationNonWritable);
			const bool canRead = !compiler.get_decoration(resource.id, spv::DecorationNonReadable);

			ReadWriteMode rwMode = ReadWriteMode::ReadWrite;
			if (canRead && !canWrite)
				rwMode = ReadWriteMode::ReadOnly;
			else if (!canRead && canWrite)
				rwMode = ReadWriteMode::WriteOnly;

			AppendResult result = Append(
				set, DescriptorSetBinding{
						 .binding = binding,
						 .type = type,
						 .shaderAccess = accessFlags,
						 .rwMode = rwMode,
					 });
			AssertAppendOk(result, set, binding);
		}
	};

	ProcessResources(shaderResources.uniform_buffers, BindingType::UniformBuffer);
	ProcessResources(shaderResources.storage_buffers, BindingType::StorageBuffer);
	ProcessResources(shaderResources.sampled_images, BindingType::Texture);
	ProcessResources(shaderResources.storage_images, BindingType::StorageImage);
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
