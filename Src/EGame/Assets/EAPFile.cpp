#include "EAPFile.hpp"
#include "AssetLoad.hpp"
#include "../Alloc/LinearAllocator.hpp"
#include "../IOUtils.hpp"
#include "../Compression.hpp"

#include <cstring>
#include <istream>
#include <ostream>

namespace eg
{
	static const char EAPMagic[] = { (char)0xFF, 'E', 'A', 'P' };
	
	void WriteEAPFile(std::span<const EAPAsset> assets, std::ostream& stream)
	{
		if (assets.size() > UINT32_MAX)
		{
			EG_PANIC("Too many assets for WriteEAPFile")
		}
		
		stream.write(EAPMagic, sizeof(EAPMagic));
		BinWrite(stream, (uint32_t)assets.size());
		
		//Extracts loader names and deduplicates them
		std::vector<std::string_view> loaderNames;
		for (const EAPAsset& asset : assets)
			loaderNames.emplace_back(asset.loaderName);
		std::sort(loaderNames.begin(), loaderNames.end());
		loaderNames.erase(std::unique(loaderNames.begin(), loaderNames.end()), loaderNames.end());
		
		//Writes loader names
		BinWrite(stream, (uint32_t)loaderNames.size());
		for (std::string_view loader : loaderNames)
			BinWriteString(stream, loader);
		
		for (const EAPAsset& asset : assets)
		{
			int64_t loaderIndex = std::lower_bound(loaderNames.begin(), loaderNames.end(), asset.loaderName) - loaderNames.begin();
			EG_ASSERT(loaderIndex >= 0 && loaderIndex < (int64_t)loaderNames.size());
			
			BinWriteString(stream, asset.assetName);
			BinWrite(stream, (uint32_t)loaderIndex);
			BinWrite(stream, asset.format.nameHash);
			BinWrite(stream, asset.format.version);
			
			uint64_t dataBytes = asset.generatedAssetData.size();
			EG_ASSERT(dataBytes < 0x8000000000000000ULL);
			if (asset.compress)
				dataBytes |= 0x8000000000000000ULL;
			
			BinWrite(stream, dataBytes);
			
			if (asset.compress)
			{
				WriteCompressedSection(stream, asset.generatedAssetData.data(), dataBytes);
			}
			else
			{
				stream.write(asset.generatedAssetData.data(), dataBytes);
			}
		}
	}
	
	std::optional<std::vector<EAPAsset>> ReadEAPFile(std::istream& stream, LinearAllocator& allocator)
	{
		char magic[sizeof(EAPMagic)];
		stream.read(magic, sizeof(magic));
		if (std::memcmp(magic, EAPMagic, sizeof(magic)) != 0)
			return {};
		
		const uint32_t numAssets = BinRead<uint32_t>(stream);
		const uint32_t numLoaderNames = BinRead<uint32_t>(stream);
		std::vector<std::string> loaderNames(numLoaderNames);
		std::vector<const AssetLoader*> loaders(numLoaderNames);
		for (uint32_t i = 0; i < numLoaderNames; i++)
		{
			loaderNames[i] = BinReadString(stream);
			loaders[i] = FindAssetLoader(loaderNames[i]);
		}
		
		std::vector<EAPAsset> assets(numAssets);
		
		for (uint32_t i = 0; i < numAssets; i++)
		{
			assets[i].assetName = BinReadString(stream);
			
			uint32_t loaderIndex = BinRead<uint32_t>(stream);
			if (loaderIndex >= numLoaderNames)
				return {};
			assets[i].loaderName = loaderNames[loaderIndex];
			assets[i].loader = loaders[loaderIndex];
			
			assets[i].format.nameHash = BinRead<uint32_t>(stream);
			assets[i].format.version = BinRead<uint32_t>(stream);
			
			uint64_t dataBytes = BinRead<uint64_t>(stream);
			assets[i].compress = (dataBytes & 0x8000000000000000ULL) != 0;
			dataBytes &= ~0x8000000000000000ULL;
			
			char* generatedAssetData = (char*)allocator.Allocate(dataBytes);
			assets[i].generatedAssetData = std::span<const char>(generatedAssetData, dataBytes);
			
			if (assets[i].compress)
			{
				ReadCompressedSection(stream, generatedAssetData, dataBytes, &assets[i].compressedSize);
			}
			else
			{
				stream.read(generatedAssetData, dataBytes);
			}
		}
		
		return assets;
	}
}
