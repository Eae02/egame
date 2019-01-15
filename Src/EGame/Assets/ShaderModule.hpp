#pragma once

#include "AssetFormat.hpp"
#include "../API.hpp"
#include "../Graphics/Abstraction.hpp"

namespace eg
{
	struct EG_API ShaderModule
	{
		ShaderStage stage;
		std::vector<char> code;
		
		static bool AssetLoader(const class AssetLoadContext& context);
		
		static const eg::AssetFormat AssetFormat;
	};
}
