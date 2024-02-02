#pragma once

#include "../API.hpp"
#include "AssetFormat.hpp"

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace eg
{
struct EAPAsset
{
	std::string assetName;
	std::string loaderName;
	AssetFormat format;
	std::span<const char> generatedAssetData;
	bool compress = true;

	// Not needed for WriteEAPFile, may be nullptr if no loader was found
	const class AssetLoader* loader = nullptr;

	// Not needed for WriteEAPFile, will be zero if the asset was not compressed
	uint64_t compressedSize = 0;
};

EG_API void WriteEAPFile(std::span<const EAPAsset> assets, std::ostream& stream);
EG_API std::optional<std::vector<EAPAsset>> ReadEAPFile(std::istream& stream, class LinearAllocator& allocator);
} // namespace eg
