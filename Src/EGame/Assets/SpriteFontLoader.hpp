#pragma once

#include "../API.hpp"
#include "AssetFormat.hpp"

namespace eg
{
EG_API extern const AssetFormat SpriteFontAssetFormat;

EG_API bool SpriteFontLoader(const class AssetLoadContext& loadContext);
} // namespace eg
