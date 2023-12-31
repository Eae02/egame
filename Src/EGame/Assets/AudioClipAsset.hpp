#pragma once

#include "AssetFormat.hpp"
#include "../Audio/AudioClip.hpp"
#include "../API.hpp"
#include "../IOUtils.hpp"

#include <cstdint>

namespace eg
{
	struct __attribute__ ((__packed__)) AudioClipAssetHeader
	{
		uint32_t channelCount;
		uint64_t frequency;
		uint64_t samples;
	};
	
	EG_API extern const AssetFormat AudioClipAssetFormat;
	
	EG_API bool AudioClipAssetLoader(const class AssetLoadContext& loadContext);
}
