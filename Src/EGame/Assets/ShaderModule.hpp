#pragma once

#include "AssetFormat.hpp"
#include "../API.hpp"
#include "../Graphics/Abstraction.hpp"

namespace eg
{
	EG_API extern const eg::AssetFormat ShaderModuleAssetFormat;
	
	EG_API bool ShaderModuleLoader(const class AssetLoadContext& context);
}
