#pragma once

#include "../API.hpp"
#include "../Graphics/AbstractionHL.hpp"
#include "AssetFormat.hpp"

namespace eg
{
class EG_API ShaderModuleAsset
{
public:
	ShaderModuleAsset() = default;
	ShaderModuleAsset(const ShaderModuleAsset&) = delete;
	ShaderModuleAsset(ShaderModuleAsset&&) = default;
	ShaderModuleAsset& operator=(const ShaderModuleAsset&) = delete;
	ShaderModuleAsset& operator=(ShaderModuleAsset&&) = default;

	ShaderStageInfo ToStageInfo(std::string_view variantName = {}) const
	{
		return ShaderStageInfo{ .shaderModule = variantName.empty() ? DefaultVariant() : GetVariant(variantName) };
	}

	ShaderModuleHandle DefaultVariant() const;

	ShaderModuleHandle GetVariant(std::string_view name) const;

	static const eg::AssetFormat AssetFormat;

	static bool AssetLoader(const class AssetLoadContext& context);

private:
	struct Variant
	{
		std::string name;
		ShaderModule shaderModule;
	};

	std::vector<Variant> m_variants;
};
} // namespace eg
