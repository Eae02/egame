#pragma once

#include "../API.hpp"
#include "../Graphics/AbstractionHL.hpp"
#include "AssetFormat.hpp"

#include <mutex>

namespace eg
{
class EG_API ShaderModuleAsset
{
public:
	ShaderModuleAsset() = default;

	ShaderStageInfo ToStageInfo(std::string_view variantName = {}) const
	{
		return ShaderStageInfo{ .shaderModule = variantName.empty() ? DefaultVariant() : GetVariant(variantName) };
	}

	ShaderModuleHandle DefaultVariant() const;

	ShaderModuleHandle GetVariant(std::string_view name) const;

	ShaderStage Stage() const { return m_stage; }

	static const eg::AssetFormat AssetFormat;

	static bool AssetLoader(const class AssetLoadContext& context);

private:
	struct Variant
	{
		std::string name;

		std::string label;
		std::vector<char> code;

		mutable std::optional<ShaderModule> shaderModule;
	};

	ShaderStage m_stage;

	bool m_createOnDemand = false;
	mutable std::mutex m_mutex;

	std::vector<Variant> m_variants;
};
} // namespace eg
