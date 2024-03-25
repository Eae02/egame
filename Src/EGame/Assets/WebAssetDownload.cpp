#include "WebAssetDownload.hpp"

#include <iomanip>
#include <iosfwd>
#include <sstream>

std::string eg::DownloadProgress::CreateMessage() const
{
	std::ostringstream messageStream;
	messageStream << "Downloading assets... (" << std::setprecision(1) << std::fixed << downloadedMiB;
	if (totalMiB)
	{
		messageStream << " / " << std::setprecision(1) << std::fixed << *totalMiB;
	}
	messageStream << " MiB)";
	return messageStream.str();
}

#ifndef __EMSCRIPTEN__
void eg::DownloadAssetPackageASync(eg::DownloadAssetPackageArgs args) {}
void eg::detail::WebDownloadAssetPackages(std::function<void()> onComplete)
{
	onComplete();
}
std::optional<std::span<const char>> eg::detail::WebGetDownloadedAssetPackage(std::string_view name)
{
	return std::nullopt;
}
void eg::detail::PruneDownloadedAssetPackages() {}
#else

#include <emscripten/emscripten.h>
#include <emscripten/fetch.h>
#include <fstream>
#include <iostream>

#include "../IOUtils.hpp"
#include "../String.hpp"

namespace eg
{
static std::vector<DownloadAssetPackageArgs> assetPackagesToDownload;
static size_t currentDownloadIndex;

void DownloadAssetPackageASync(DownloadAssetPackageArgs args)
{
	assetPackagesToDownload.push_back(std::move(args));
}

struct FetchClose
{
	void operator()(emscripten_fetch_t* fetch) const
	{
		if (fetch != nullptr)
			emscripten_fetch_close(fetch);
	}
};

struct DownloadedAssetBinary
{
	std::string name;
	std::unique_ptr<emscripten_fetch_t, FetchClose> fetch;
	std::vector<char> cachedAssetBinary;
	bool freeAfterInit = false;
};

static std::vector<DownloadedAssetBinary> assetBinaries;

std::optional<std::span<const char>> detail::WebGetDownloadedAssetPackage(std::string_view name)
{
	for (const DownloadedAssetBinary& binary : assetBinaries)
	{
		if (binary.name == name)
		{
			if (binary.fetch != nullptr)
				return std::span<const char>(binary.fetch->data, binary.fetch->numBytes);
			else
				return std::span<const char>(binary.cachedAssetBinary);
		}
	}
	return std::nullopt;
}

void detail::PruneDownloadedAssetPackages()
{
	auto endIt =
		std::remove_if(assetBinaries.begin(), assetBinaries.end(), [&](const auto& b) { return b.freeAfterInit; });
	assetBinaries.erase(endIt, assetBinaries.end());
}

static std::ostream& CacheBeginPrint(const DownloadAssetPackageArgs& args)
{
	return std::cout << "[assetcache] (" << args.eapName << ") ";
}

static std::string GetCachePath(const DownloadAssetPackageArgs& args, bool isIdFile)
{
	std::string path = "/asset_cache/" + args.eapName;
	if (isIdFile)
		path += ".id";
	return path;
}

static void FetchNextAssetPackage();

static void WriteAssetsToCache(std::span<const char> data, const DownloadAssetPackageArgs& args)
{
	if (args.cacheID.empty())
		return;

	std::string cachePath = GetCachePath(args, false);

	std::ofstream cachedAssetsStream(cachePath, std::ios::binary);
	if (!cachedAssetsStream)
	{
		CacheBeginPrint(args) << "Failed to open " << cachePath << " for writing" << std::endl;
		return;
	}
	cachedAssetsStream.write(data.data(), data.size());
	cachedAssetsStream.close();

	std::string idPath = GetCachePath(args, true);

	std::ofstream cachedAssetsIdStream(idPath, std::ios::binary);
	if (!cachedAssetsIdStream)
	{
		std::cout << "[assetcache] Failed to open " << idPath << " for writing" << std::endl;
		return;
	}

	cachedAssetsIdStream.write(args.cacheID.data(), args.cacheID.size());
	cachedAssetsIdStream.close();

	EM_ASM(FS.syncfs(false, function(err){}););
}

static void AssetDownloadCompleted(emscripten_fetch_t* fetch)
{
	WriteAssetsToCache(
		std::span<const char>(fetch->data, fetch->numBytes), assetPackagesToDownload.at(currentDownloadIndex));

	emscripten_async_call(
		[](void* userdata)
		{
			const auto& packageArgs = assetPackagesToDownload.at(currentDownloadIndex);

			assetBinaries.push_back(DownloadedAssetBinary{
				.name = packageArgs.eapName,
				.fetch = std::unique_ptr<emscripten_fetch_t, FetchClose>(static_cast<emscripten_fetch_t*>(userdata)),
				.freeAfterInit = packageArgs.freeAfterInit,
			});

			currentDownloadIndex++;
			FetchNextAssetPackage();
		},
		fetch, 0);
}

static void AssetDownloadFailed(emscripten_fetch_t* fetch)
{
	std::ostringstream msgStream;
	msgStream << "Failed to download assets (" << fetch->status << ")";
	emscripten_fetch_close(fetch);
	detail::PanicImpl(msgStream.str());
}

static void AssetDownloadProgress(emscripten_fetch_t* fetch)
{
	const DownloadAssetPackageArgs& args = assetPackagesToDownload.at(currentDownloadIndex);
	if (!args.progressCallback)
		return;

	DownloadProgress progress = { .eapName = args.eapName };
	auto ToMiB = [](uint64_t bytes) { return static_cast<double>(bytes) / 1048576.0; };
	if (fetch->totalBytes)
	{
		progress.downloadedMiB = ToMiB(fetch->dataOffset);
		progress.totalMiB = ToMiB(fetch->totalBytes);
	}
	else
	{
		progress.downloadedMiB = ToMiB(fetch->dataOffset + fetch->numBytes);
	}
	args.progressCallback(progress);
}

static std::optional<std::vector<char>> TryLoadCachedAssets(const DownloadAssetPackageArgs& args)
{
	if (args.cacheID.empty())
		return std::nullopt;

	std::string idPath = GetCachePath(args, true);

	std::ifstream cachedAssetsIdStream(idPath, std::ios::binary);
	if (!cachedAssetsIdStream)
	{
		CacheBeginPrint(args) << "Failed to open " << idPath << std::endl;
		return std::nullopt;
	}

	std::string cachedId;
	std::getline(cachedAssetsIdStream, cachedId);
	if (TrimString(cachedId) != args.cacheID)
	{
		CacheBeginPrint(args) << "Version mismatch (got '" << cachedId << "' expected '" << args.cacheID << "')"
							  << std::endl;
		return std::nullopt;
	}
	cachedAssetsIdStream.close();

	std::string cachePath = GetCachePath(args, false);

	std::ifstream cachedAssetsStream(cachePath, std::ios::binary);
	if (!cachedAssetsStream)
	{
		CacheBeginPrint(args) << "Failed to open " << cachePath << std::endl;
		return std::nullopt;
	}

	std::vector<char> streamData = ReadStreamContents(cachedAssetsStream);

	cachedAssetsStream.close();

	static const char EAP_MAGIC[4] = { -1, 'E', 'A', 'P' };
	if (streamData.size() < 4 || std::memcmp(streamData.data(), EAP_MAGIC, 4) != 0)
	{
		CacheBeginPrint(args) << "Package corrupted " << std::hex << *reinterpret_cast<uint32_t*>(streamData.data())
							  << std::dec << std::endl;
		return std::nullopt;
	}

	return streamData;
}

std::function<void()> allDownloadsCompleteCallback;

static void FetchNextAssetPackage()
{
	if (currentDownloadIndex >= assetPackagesToDownload.size())
	{
		allDownloadsCompleteCallback();
		return;
	}

	const DownloadAssetPackageArgs& args = assetPackagesToDownload[currentDownloadIndex];

	EG_ASSERT(!args.eapName.empty());

	if (auto cachedAssetBinary = TryLoadCachedAssets(args))
	{
		assetBinaries.push_back(DownloadedAssetBinary{
			.name = args.eapName,
			.cachedAssetBinary = std::move(*cachedAssetBinary),
			.freeAfterInit = args.freeAfterInit,
		});

		CacheBeginPrint(args) << "Cache valid, loading assets from cache" << std::endl;
		currentDownloadIndex++;
		FetchNextAssetPackage();
		return;
	}

	emscripten_fetch_attr_t attr;
	emscripten_fetch_attr_init(&attr);
	strcpy(attr.requestMethod, "GET");
	attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
	attr.onsuccess = AssetDownloadCompleted;
	attr.onerror = AssetDownloadFailed;
	attr.onprogress = AssetDownloadProgress;
	emscripten_fetch(&attr, args.url.empty() ? args.eapName.c_str() : args.url.c_str());
}

void detail::WebDownloadAssetPackages(std::function<void()> onComplete)
{
	currentDownloadIndex = 0;
	allDownloadsCompleteCallback = std::move(onComplete);

	bool anyPackageCache = std::any_of(
		assetPackagesToDownload.begin(), assetPackagesToDownload.end(),
		[&](const auto& p) { return !p.cacheID.empty(); });

	if (anyPackageCache)
	{
		EM_ASM(FS.mkdir('/asset_cache'); FS.mount(IDBFS, {}, '/asset_cache'); FS.syncfs(
			true, function(err) { Module.cwrap("AssetCacheCreated", "", [])(); }););
	}
	else
	{
		FetchNextAssetPackage();
	}
}
} // namespace eg

extern "C" void EMSCRIPTEN_KEEPALIVE AssetCacheCreated()
{
	eg::FetchNextAssetPackage();
}

#endif
