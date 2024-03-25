#pragma once

#include "../API.hpp"

#include <functional>
#include <optional>
#include <string>
#include <span>

namespace eg
{
struct EG_API DownloadProgress
{
	std::string_view eapName;
	double downloadedMiB;
	std::optional<double> totalMiB;

	std::string CreateMessage() const;
};

struct DownloadAssetPackageArgs
{
	std::string eapName;

	/**
	 * URL to fetch the asset package from. If empty, will use the same as eapName.
	 */
	std::string url;

	/**
	 * ID for detecting cache invalidation. If empty, caching will not be used.
	 */
	std::string cacheID;

	/**
	 * Whether to remove the package data from memory after EGame has finished initializing.
	 */
	bool freeAfterInit = true;

	std::function<void(const DownloadProgress&)> progressCallback;
};

EG_API void DownloadAssetPackageASync(DownloadAssetPackageArgs args);

namespace detail
{
void WebDownloadAssetPackages(std::function<void()> onComplete);
std::optional<std::span<const char>> WebGetDownloadedAssetPackage(std::string_view name);
void PruneDownloadedAssetPackages();
} // namespace detail
} // namespace eg
