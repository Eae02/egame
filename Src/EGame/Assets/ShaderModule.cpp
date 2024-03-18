#include "ShaderModule.hpp"
#include "../Assert.hpp"
#include "../Graphics/AbstractionHL.hpp"
#include "AssetLoad.hpp"

#include <string>

namespace eg
{
const eg::AssetFormat ShaderModuleAsset::AssetFormat{ "EG::Shader", 2 };

bool ShaderModuleAsset::AssetLoader(const AssetLoadContext& context)
{
	ShaderModuleAsset& result = context.CreateResult<ShaderModuleAsset>();

	MemoryReader reader(context.Data());

	const ShaderStage stage = static_cast<ShaderStage>(reader.Read<uint32_t>());
	const uint32_t numVariants = reader.Read<uint32_t>();

	std::vector<char> codeBuffer;

	for (uint32_t i = 0; i < numVariants; i++)
	{
		std::string variantName(reader.ReadString());

		uint64_t codeLen = reader.Read<uint64_t>();
		if (codeBuffer.size() < codeLen)
			codeBuffer.resize(codeLen);

		std::span<char> codeSpan(codeBuffer.data(), codeLen);
		reader.ReadToSpan(codeSpan);

		std::string label(context.AssetPath());
		if (numVariants > 1)
			label += " [" + variantName + "]";

		result.m_variants.push_back(Variant{
			.name = std::move(variantName),
			.shaderModule = ShaderModule(stage, codeSpan, label.c_str()),
		});
	}

	return true;
}

ShaderModuleHandle ShaderModuleAsset::GetVariant(std::string_view name) const
{
	for (const Variant& variant : m_variants)
	{
		if (variant.name == name)
		{
			return variant.shaderModule.Handle();
		}
	}
	EG_PANIC("Shader module variant not found: '" << name << "'");
}

ShaderModuleHandle ShaderModuleAsset::DefaultVariant() const
{
	return GetVariant("_VDEFAULT");
}
} // namespace eg
