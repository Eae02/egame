#pragma once

#include "AssetFormat.hpp"
#include "../Audio/AudioClip.hpp"
#include "../API.hpp"
#include "../IOUtils.hpp"

#include <cstdint>

namespace eg
{
	EG_API extern const AssetFormat AudioClipAssetFormat;
	
	EG_API bool AudioClipAssetLoader(const class AssetLoadContext& loadContext);
}
