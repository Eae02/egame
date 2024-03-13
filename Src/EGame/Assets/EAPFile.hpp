#pragma once

#include "../API.hpp"
#include "../Platform/FileSystem.hpp"
#include "AssetFormat.hpp"

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace eg
{
struct SideStreamData
{
	std::string_view streamName;
	std::span<const char> data;
};

struct EAPAsset
{
	std::string assetName;
	std::string loaderName;
	AssetFormat format;
	std::span<const char> generatedAssetData;
	std::vector<SideStreamData> sideStreamsData;
	bool compress = true;

	// Not needed for WriteEAPFile, may be nullptr if no loader was found
	const class AssetLoader* loader = nullptr;

	// Not needed for WriteEAPFile, will be zero if the asset was not compressed
	uint64_t compressedSize = 0;
};

EG_API void WriteEAPFile(std::span<const EAPAsset> assets, std::string_view path);

EG_API std::string GetEAPSideStreamPath(std::string_view eapPath, std::string_view sideStreamName);

struct ReadEAPFileArgs
{
	std::span<const char> eapFileData;
	std::span<const SideStreamData> sideStreamsData;
	std::optional<std::function<std::span<const char>(std::string_view)>> openSideStreamCallback;
};

EG_API std::optional<std::vector<EAPAsset>> ReadEAPFile(const ReadEAPFileArgs& args, class LinearAllocator& allocator);

struct ReadEAPFileFromFileSystemResult
{
	std::vector<EAPAsset> assets;

	// The mapped files need to stay open for the memory referenced by EAPAsset to stay valid
	std::vector<MemoryMappedFile> mappedFiles;
};

EG_API std::optional<ReadEAPFileFromFileSystemResult> ReadEAPFileFromFileSystem(
	const std::string& path, const std::function<bool(std::string_view)>& shouldLoadSideStreamCallback,
	class LinearAllocator& allocator);
} // namespace eg
