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

	const char* data = context.Data().data();

	const ShaderStage stage = static_cast<ShaderStage>(reinterpret_cast<const uint32_t*>(data)[0]);
	const uint32_t numVariants = reinterpret_cast<const uint32_t*>(data)[1];
	data += sizeof(uint32_t) * 2;

	std::string label(context.AssetPath());

	for (uint32_t i = 0; i < numVariants; i++)
	{
		uint32_t variantHash = reinterpret_cast<const uint32_t*>(data)[0];
		uint32_t codeSize = reinterpret_cast<const uint32_t*>(data)[1];
		data += sizeof(uint32_t) * 2;

		result.m_variants.push_back(Variant{
			.hash = variantHash,
			.shaderModule = ShaderModule(stage, std::span<const char>(data, codeSize), label.c_str()),
		});

		data += codeSize;
	}

	return true;
}

ShaderModuleHandle ShaderModuleAsset::GetVariant(std::string_view name) const
{
	uint32_t hash = HashFNV1a32(name);
	for (const Variant& variant : m_variants)
	{
		if (variant.hash == hash)
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
