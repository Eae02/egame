#pragma once

#include "AssetFormat.hpp"
#include "../API.hpp"

namespace eg
{
	EG_API extern const AssetFormat SpriteFontAssetFormat;
	
	EG_API bool SpriteFontLoader(const class AssetLoadContext& loadContext);
}
