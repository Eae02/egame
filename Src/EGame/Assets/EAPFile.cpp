#include "EAPFile.hpp"
#include "../Alloc/LinearAllocator.hpp"
#include "../Compression.hpp"
#include "../IOUtils.hpp"
#include "../Platform/FileSystem.hpp"
#include "AssetLoad.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace eg
{
static const char EAPMagic[] = { -1, 'E', 'A', 'P' };

static void WriteAssetDataSection(std::ofstream& stream, std::span<const char> data, bool compress)
{
	uint64_t dataBytesAndCompressedBit = data.size();
	EG_ASSERT(dataBytesAndCompressedBit < 0x8000000000000000ULL);
	if (compress)
		dataBytesAndCompressedBit |= 0x8000000000000000ULL;
	BinWrite(stream, dataBytesAndCompressedBit);

	if (compress)
	{
		WriteCompressedSection(stream, data.data(), data.size());
	}
	else
	{
		stream.write(data.data(), data.size());
	}
}

struct AssetDataSection
{
	std::span<const char> data;
	std::optional<uint64_t> compressedSize;
	size_t bytesRead;
};

static std::optional<AssetDataSection> ReadAssetDataSection(std::span<const char> section, LinearAllocator& allocator)
{
	uint64_t dataBytes = ReadFromSpan<uint64_t>(section, 0);
	const bool compressed = (dataBytes & 0x8000000000000000ULL) != 0;
	dataBytes &= ~0x8000000000000000ULL;

	if (compressed)
	{
		uint64_t compressedSize = ReadFromSpan<uint64_t>(section, sizeof(uint64_t));
		std::span<const char> compressedData = section.subspan(sizeof(uint64_t) * 2, compressedSize);

		char* uncompressedDataPtr = static_cast<char*>(allocator.Allocate(dataBytes));
		if (!Decompress(compressedData.data(), compressedData.size(), uncompressedDataPtr, dataBytes))
			return std::nullopt;

		return AssetDataSection{
			.data = { uncompressedDataPtr, dataBytes },
			.compressedSize = compressedSize,
			.bytesRead = sizeof(uint64_t) * 2 + compressedSize,
		};
	}
	else
	{
		return AssetDataSection{
			.data = section.subspan(sizeof(uint64_t), dataBytes),
			.bytesRead = sizeof(uint64_t) + dataBytes,
		};
	}
}

std::string GetEAPSideStreamPath(std::string_view eapPath, std::string_view sideStreamName)
{
	constexpr std::string_view EAP_EXTENSION = ".eap";
	if (eapPath.ends_with(EAP_EXTENSION))
	{
		eapPath = eapPath.substr(0, eapPath.size() - EAP_EXTENSION.size());
	}
	return Concat({ eapPath, "_", sideStreamName });
}

void WriteEAPFile(std::span<const EAPAsset> assets, std::string_view path)
{
	if (assets.size() > UINT32_MAX)
	{
		EG_PANIC("Too many assets for WriteEAPFile")
	}

	std::ofstream stream(path, std::ios::binary);
	stream.write(EAPMagic, sizeof(EAPMagic));
	BinWrite(stream, UnsignedNarrow<uint32_t>(assets.size()));

	// Extracts loader names and deduplicates them
	std::vector<std::string_view> loaderNames;
	for (const EAPAsset& asset : assets)
		loaderNames.emplace_back(asset.loaderName);
	std::sort(loaderNames.begin(), loaderNames.end());
	loaderNames.erase(std::unique(loaderNames.begin(), loaderNames.end()), loaderNames.end());

	// Writes loader names
	BinWrite(stream, UnsignedNarrow<uint32_t>(loaderNames.size()));
	for (std::string_view loader : loaderNames)
		BinWriteString(stream, loader);

	// Extracts side stream names and deduplicates them
	std::vector<std::string_view> sideStreamNames;
	for (const EAPAsset& asset : assets)
	{
		for (const SideStreamData& sideStreamData : asset.sideStreamsData)
			sideStreamNames.emplace_back(sideStreamData.streamName);
	}
	std::sort(sideStreamNames.begin(), sideStreamNames.end());
	sideStreamNames.erase(std::unique(sideStreamNames.begin(), sideStreamNames.end()), sideStreamNames.end());

	const uint32_t numSideStreams = UnsignedNarrow<uint32_t>(sideStreamNames.size());

	// Creates side streams and writes their names to the main stream
	BinWrite(stream, numSideStreams);
	std::vector<std::ofstream> sideStreams;
	for (std::string_view sideStreamName : sideStreamNames)
	{
		std::string sideStreamPath = GetEAPSideStreamPath(path, sideStreamName);
		sideStreams.emplace_back(sideStreamPath, std::ios::binary);

		BinWriteString(stream, sideStreamName);
	}

	std::vector<uint64_t> assetSideStreamOffsets(numSideStreams);

	for (const EAPAsset& asset : assets)
	{
		int64_t loaderIndex =
			std::lower_bound(loaderNames.begin(), loaderNames.end(), asset.loaderName) - loaderNames.begin();
		EG_ASSERT(loaderIndex >= 0 && loaderIndex < ToInt64(loaderNames.size()));

		BinWriteString(stream, asset.assetName);
		BinWrite(stream, UnsignedNarrow<uint32_t>(ToUnsigned(loaderIndex)));
		BinWrite(stream, asset.format.nameHash);
		BinWrite(stream, asset.format.version);

		std::fill(assetSideStreamOffsets.begin(), assetSideStreamOffsets.end(), UINT64_MAX);
		for (const SideStreamData& sideStreamData : asset.sideStreamsData)
		{
			auto streamIt = std::lower_bound(sideStreamNames.begin(), sideStreamNames.end(), sideStreamData.streamName);
			EG_ASSERT(streamIt != sideStreamNames.end());
			const size_t streamIndex = streamIt - sideStreamNames.begin();

			assetSideStreamOffsets[streamIndex] = static_cast<uint64_t>(sideStreams[streamIndex].tellp());
			WriteAssetDataSection(sideStreams[streamIndex], sideStreamData.data, asset.compress);
		}

		for (uint32_t i = 0; i < numSideStreams; i++)
		{
			BinWrite<uint64_t>(stream, assetSideStreamOffsets[i]);
		}

		WriteAssetDataSection(stream, asset.generatedAssetData, asset.compress);
	}
}

std::optional<std::vector<EAPAsset>> ReadEAPFile(const ReadEAPFileArgs& args, LinearAllocator& allocator)
{
	if (args.eapFileData.size() < sizeof(EAPMagic) ||
	    std::memcmp(args.eapFileData.data(), EAPMagic, sizeof(EAPMagic)) != 0)
	{
		return std::nullopt;
	}

	MemoryReader reader(args.eapFileData.subspan(sizeof(EAPMagic)));

	const uint32_t numAssets = reader.Read<uint32_t>();
	const uint32_t numLoaderNames = reader.Read<uint32_t>();
	std::vector<std::string_view> loaderNames(numLoaderNames);
	std::vector<const AssetLoader*> loaders(numLoaderNames);
	for (uint32_t i = 0; i < numLoaderNames; i++)
	{
		loaderNames[i] = reader.ReadString();
		loaders[i] = FindAssetLoader(loaderNames[i]);
	}

	const uint32_t numSideStreams = reader.Read<uint32_t>();
	std::vector<std::span<const char>> sideStreams(numSideStreams);
	std::vector<std::string_view> sideStreamNames(numSideStreams);
	for (uint32_t i = 0; i < numSideStreams; i++)
	{
		std::string_view sideStreamName = reader.ReadString();
		auto it = std::find_if(
			args.sideStreamsData.begin(), args.sideStreamsData.end(),
			[&](const SideStreamData& data) { return data.streamName == sideStreamName; });

		sideStreamNames[i] = sideStreamName;

		if (it != args.sideStreamsData.end())
		{
			sideStreams[i] = it->data;
		}
		else if (args.openSideStreamCallback.has_value())
		{
			sideStreams[i] = (*args.openSideStreamCallback)(sideStreamName);
		}
	}

	std::vector<EAPAsset> assets(numAssets);

	for (uint32_t i = 0; i < numAssets; i++)
	{
		assets[i].assetName = reader.ReadString();

		uint32_t loaderIndex = reader.Read<uint32_t>();
		if (loaderIndex >= numLoaderNames)
			return {};
		assets[i].loaderName = loaderNames[loaderIndex];
		assets[i].loader = loaders[loaderIndex];

		assets[i].format.nameHash = reader.Read<uint32_t>();
		assets[i].format.version = reader.Read<uint32_t>();

		for (uint32_t j = 0; j < numSideStreams; j++)
		{
			uint64_t sideStreamDataOffset = reader.Read<uint64_t>();
			if (sideStreamDataOffset == UINT64_MAX && !sideStreams[j].empty())
				continue;

			std::optional<AssetDataSection> sideDataSection =
				ReadAssetDataSection(sideStreams[j].subspan(sideStreamDataOffset), allocator);
			if (!sideDataSection.has_value())
				return std::nullopt;

			assets[i].sideStreamsData.push_back(SideStreamData{
				.streamName = sideStreamNames[j],
				.data = sideDataSection->data,
			});
			assets[i].compressedSize += sideDataSection->compressedSize.value_or(0);
		}

		std::optional<AssetDataSection> dataSection =
			ReadAssetDataSection(reader.data.subspan(reader.dataOffset), allocator);
		if (!dataSection.has_value())
			return std::nullopt;

		assets[i].generatedAssetData = dataSection->data;
		assets[i].compress = dataSection->compressedSize.has_value();
		assets[i].compressedSize += dataSection->compressedSize.value_or(0);

		reader.dataOffset += dataSection->bytesRead;
	}

	return assets;
}

std::optional<ReadEAPFileFromFileSystemResult> ReadEAPFileFromFileSystem(
	const std::string& path, const std::function<bool(std::string_view)>& shouldLoadSideStreamCallback,
	LinearAllocator& allocator)
{
	std::optional<MemoryMappedFile> eapFile = MemoryMappedFile::OpenRead(path.c_str());
	if (!eapFile.has_value())
		return std::nullopt;

	ReadEAPFileArgs readArgs;
	readArgs.eapFileData = eapFile->data;

	std::vector<MemoryMappedFile> mappedFiles;
	mappedFiles.push_back(std::move(*eapFile));

	readArgs.openSideStreamCallback = [&](std::string_view sideStreamName) -> std::span<const char>
	{
		if (!shouldLoadSideStreamCallback(sideStreamName))
			return {};
		std::string sideStreamPath = GetEAPSideStreamPath(path, sideStreamName);
		std::optional<MemoryMappedFile> mappedFile = MemoryMappedFile::OpenRead(sideStreamPath.c_str());
		if (!mappedFile.has_value())
			return {};
		auto data = mappedFile->data;
		mappedFiles.push_back(std::move(*mappedFile));
		return data;
	};

	auto result = ReadEAPFile(readArgs, allocator);
	if (!result.has_value())
		return std::nullopt;

	return ReadEAPFileFromFileSystemResult{
		.assets = std::move(*result),
		.mappedFiles = std::move(mappedFiles),
	};
}
} // namespace eg
