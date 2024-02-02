#pragma once

#include "../API.hpp"
#include "AssetFormat.hpp"

#include <span>

namespace eg
{
enum class TextureQuality
{
	Low = 0,
	Medium = 1,
	High = 2
};

EG_API extern TextureQuality TextureAssetQuality;

EG_API extern const AssetFormat Texture2DAssetFormat;

EG_API bool Texture2DLoader(const class AssetLoadContext& loadContext);

EG_API void Texture2DLoaderPrintInfo(std::span<const char> data, std::ostream& outStream);
} // namespace eg
