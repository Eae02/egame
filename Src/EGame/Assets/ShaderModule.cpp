#include "ShaderModule.hpp"
#include "../Assert.hpp"
#include "../Graphics/AbstractionHL.hpp"
#include "AssetLoad.hpp"

#include <string>

namespace eg
{
const eg::AssetFormat ShaderModuleAsset::AssetFormat{ "EG::Shader", 3 };

bool ShaderModuleAsset::AssetLoader(const AssetLoadContext& context)
{
	ShaderModuleAsset& result = context.CreateResult<ShaderModuleAsset>();

	MemoryReader reader(context.Data());

	result.m_stage = static_cast<ShaderStage>(reader.Read<uint32_t>());
	const uint32_t numVariants = reader.Read<uint32_t>();
	result.m_createOnDemand = reader.Read<uint8_t>() != 0;

	std::vector<char> codeBuffer;

	for (uint32_t i = 0; i < numVariants; i++)
	{
		std::string variantName(reader.ReadString());

		uint32_t codeLen = reader.Read<uint32_t>();
		if (codeBuffer.size() < static_cast<size_t>(codeLen))
			codeBuffer.resize(codeLen);

		std::span<char> codeSpan(codeBuffer.data(), codeLen);
		reader.ReadToSpan(codeSpan);

		std::string label(context.AssetPath());
		if (numVariants > 1)
			label += " [" + variantName + "]";

		Variant variant = { .name = std::move(variantName) };

		if (result.m_createOnDemand)
		{
			variant.code.assign(codeSpan.begin(), codeSpan.end());
			variant.label = std::move(label);
		}
		else
		{
			variant.shaderModule = ShaderModule(result.m_stage, codeSpan, label.c_str());
		}

		result.m_variants.push_back(std::move(variant));
	}

	return true;
}

ShaderModuleHandle ShaderModuleAsset::GetVariant(std::string_view name) const
{
	for (const Variant& variant : m_variants)
	{
		if (variant.name == name)
		{
			if (!m_createOnDemand)
				return variant.shaderModule->Handle();

			std::lock_guard<std::mutex> lock(m_mutex);

			if (!variant.shaderModule.has_value())
			{
				variant.shaderModule = ShaderModule(m_stage, variant.code, variant.label.c_str());
			}

			return variant.shaderModule->Handle();
		}
	}
	EG_PANIC("Shader module variant not found: '" << name << "'");
}

ShaderModuleHandle ShaderModuleAsset::DefaultVariant() const
{
	return GetVariant("_VDEFAULT");
}
} // namespace eg
