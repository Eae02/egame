#pragma once

#include "AssetFormat.hpp"
#include "../API.hpp"

namespace eg
{
	EG_API extern const AssetFormat Texture2DArrayAssetFormat;
	
	EG_API bool Texture2DArrayLoader(const class AssetLoadContext& loadContext);
}
