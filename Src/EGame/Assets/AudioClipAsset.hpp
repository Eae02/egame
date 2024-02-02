#pragma once

#include "../API.hpp"
#include "../Audio/AudioClip.hpp"
#include "../IOUtils.hpp"
#include "AssetFormat.hpp"

#include <cstdint>

namespace eg
{
EG_API extern const AssetFormat AudioClipAssetFormat;

EG_API bool AudioClipAssetLoader(const class AssetLoadContext& loadContext);
} // namespace eg
