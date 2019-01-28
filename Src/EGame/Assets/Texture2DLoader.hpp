#pragma once

#include <EGame/EG.hpp>
#include "AssetFormat.hpp"
#include "../API.hpp"

namespace eg
{
	EG_API extern const AssetFormat Texture2DAssetFormat;
	
	EG_API bool Texture2DLoader(const eg::AssetLoadContext& loadContext);
}
