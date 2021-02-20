#pragma once

#include "AssetFormat.hpp"
#include "../Audio/AudioClip.hpp"
#include "../API.hpp"
#include "../IOUtils.hpp"

namespace eg
{
#pragma pack(push, 1)
	struct AudioClipAssetHeader
	{
		uint8_t channelCount;
		uint64_t frequency;
		uint64_t samples;
	};
#pragma pack(pop)
	
	EG_API extern const AssetFormat AudioClipAssetFormat;
	
	EG_API bool AudioClipAssetLoader(const class AssetLoadContext& loadContext);
}
