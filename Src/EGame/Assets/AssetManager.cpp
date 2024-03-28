#include "AssetManager.hpp"
#include "../Compression.hpp"
#include "../IOUtils.hpp"
#include "../Log.hpp"
#include "../Platform/DynamicLibrary.hpp"
#include "../Platform/FileSystem.hpp"
#include "AssetGenerator.hpp"
#include "EAPFile.hpp"
#include "EGame/Assets/AssetLoad.hpp"
#include "WebAssetDownload.hpp"
#include "YAMLUtils.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <regex>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <yaml-cpp/yaml.h>

namespace eg
{
AssetDirectory* AssetDirectory::FindDirectory(std::string_view path, bool create, eg::LinearAllocator* allocator)
{
	if (path.empty())
		return this;

	size_t firstSlash = path.find('/');
	if (firstSlash == 0)
	{
		// The path starts with a slash, so try again from the same directory with the slash removed
		return FindDirectory(path.substr(1), create, allocator);
	}

	std::string_view entryName;
	if (firstSlash == std::string_view::npos)
		entryName = path;
	else
		entryName = path.substr(0, firstSlash);

	std::string_view remPath = path.substr(std::min(entryName.size() + 1, path.size()));

	// Searches for the next directory in the path
	for (AssetDirectory* dir = firstChildDir; dir != nullptr; dir = dir->nextSiblingDir)
	{
		if (dir->name == entryName)
			return dir->FindDirectory(remPath, create, allocator);
	}

	if (!create)
		return nullptr;
	EG_ASSERT(allocator != nullptr);

	AssetDirectory* newDir = allocator->New<AssetDirectory>();
	newDir->name = allocator->MakeStringCopy(entryName);

	// Adds the new directory to the linked list
	newDir->nextSiblingDir = firstChildDir;
	firstChildDir = newDir;

	return newDir->FindDirectory(remPath, create, allocator);
}

static const char cachedAssetMagic[] = { -2, 'E', 'A', 'C' };

static void SaveAssetToCache(const GeneratedAsset& asset, uint64_t yamlParamsHash, const std::string& cachePath)
{
	eg::CreateDirectories(ParentPath(cachePath, false));

	std::ofstream stream(cachePath, std::ios::binary);

	stream.write(cachedAssetMagic, sizeof(cachedAssetMagic));
	BinWrite(stream, yamlParamsHash);
	BinWrite(stream, asset.format.nameHash);
	BinWrite(stream, asset.format.version);
	BinWrite(stream, static_cast<uint32_t>(asset.flags));
	BinWrite(stream, static_cast<uint64_t>(time(nullptr)));

	BinWrite(stream, UnsignedNarrow<uint32_t>(asset.fileDependencies.size()));
	for (const std::string& dep : asset.fileDependencies)
		BinWriteString(stream, dep);

	BinWrite(stream, UnsignedNarrow<uint32_t>(asset.loadDependencies.size()));
	for (const std::string& dep : asset.loadDependencies)
		BinWriteString(stream, dep);

	BinWrite(stream, UnsignedNarrow<uint32_t>(asset.sideStreamsData.size()));

	BinWrite<uint32_t>(stream, UnsignedNarrow<uint32_t>(asset.data.size()));
	stream.write(asset.data.data(), static_cast<std::streamsize>(asset.data.size()));

	for (const GeneratedAssetSideStreamData& sideStreamData : asset.sideStreamsData)
	{
		BinWriteString(stream, sideStreamData.streamName);
		BinWrite<uint32_t>(stream, UnsignedNarrow<uint32_t>(sideStreamData.data.size()));
		stream.write(sideStreamData.data.data(), static_cast<std::streamsize>(sideStreamData.data.size()));
	}
}

static std::optional<GeneratedAsset> TryReadAssetFromCache(
	const std::string& currentDirPath, const AssetFormat& expectedFormat, uint64_t expectedYamlHash,
	const std::string& cachePath)
{
	std::ifstream stream(cachePath, std::ios::binary);
	if (!stream)
		return {};

	char magic[sizeof(cachedAssetMagic)];
	stream.read(magic, sizeof(magic));
	if (std::memcmp(cachedAssetMagic, magic, sizeof(magic)) != 0)
		return {};

	uint64_t yamlHash = BinRead<uint64_t>(stream);
	if (yamlHash != expectedYamlHash && yamlHash != 0)
		return {};

	GeneratedAsset asset;

	asset.format.nameHash = BinRead<uint32_t>(stream);
	asset.format.version = BinRead<uint32_t>(stream);
	if (asset.format.nameHash != expectedFormat.nameHash || asset.format.version != expectedFormat.version)
		return {};

	asset.flags = static_cast<AssetFlags>(BinRead<uint32_t>(stream));
	const auto generateTime = std::chrono::system_clock::from_time_t(static_cast<time_t>(BinRead<uint64_t>(stream)));

	const uint32_t numFileDependencies = BinRead<uint32_t>(stream);
	asset.fileDependencies.reserve(numFileDependencies);
	for (uint32_t i = 0; i < numFileDependencies; i++)
	{
		std::string dependency = BinReadString(stream);
		std::string dependencyFullPath = currentDirPath + dependency;
		if (LastWriteTime(dependencyFullPath.c_str()) > generateTime)
			return {};
		asset.fileDependencies.push_back(std::move(dependency));
	}

	const uint32_t numLoadDependencies = BinRead<uint32_t>(stream);
	asset.loadDependencies.reserve(numLoadDependencies);
	for (uint32_t i = 0; i < numLoadDependencies; i++)
	{
		asset.loadDependencies.push_back(BinReadString(stream));
	}

	const uint32_t numSideStreams = BinRead<uint32_t>(stream);

	const uint32_t dataSize = BinRead<uint32_t>(stream);
	asset.data.resize(dataSize);
	stream.read(asset.data.data(), dataSize);

	asset.sideStreamsData.resize(numSideStreams);
	for (uint32_t i = 0; i < numSideStreams; i++)
	{
		asset.sideStreamsData[i].streamName = BinReadString(stream);
		const uint32_t sideStreamDataSize = BinRead<uint32_t>(stream);
		asset.sideStreamsData[i].data.resize(sideStreamDataSize);
		stream.read(asset.sideStreamsData[i].data.data(), sideStreamDataSize);
	}

	return asset;
}

struct AssetToLoad
{
	enum State
	{
		STATE_INITIAL,
		STATE_PROCESSING,
		STATE_LOADED,
		STATE_FAILED
	};

	State state = STATE_INITIAL;
	std::string name;
	GeneratedAsset generatedAsset;
	const AssetLoader* loader = nullptr;
};

struct ProcessAssetArgs
{
	AssetDirectory* destinationDir;
	const std::unordered_map<std::string_view, AssetToLoad*>* assetsToLoadByName;
	std::vector<AssetToLoad*>* assetsToposortOut;
	std::vector<Asset*>* allAssetsOut;
	eg::LinearAllocator* allocator;
	GraphicsLoadContext* graphicsLoadContext;
};

// Generates and loads an asset recursively so that all it's load-time dependencies are satisfied.
static bool ProcessAsset(AssetToLoad& assetToLoad, const ProcessAssetArgs& processAssetArgs)
{
	switch (assetToLoad.state)
	{
	case AssetToLoad::STATE_INITIAL: assetToLoad.state = AssetToLoad::STATE_PROCESSING; break;
	case AssetToLoad::STATE_PROCESSING:
		eg::Log(LogLevel::Error, "as", "Circular load-time dependency involving '{0}'", assetToLoad.name);
		return false;
	case AssetToLoad::STATE_LOADED: return true;
	case AssetToLoad::STATE_FAILED: return false;
	}

	// Processes dependencies
	for (const std::string& dep : assetToLoad.generatedAsset.loadDependencies)
	{
		std::string fullDepPath;
		std::string_view fullDepPathView;
		if (dep.at(0) == '/')
		{
			fullDepPathView = dep;
		}
		else
		{
			fullDepPath = Concat({ ParentPath(assetToLoad.name), dep });
			fullDepPathView = fullDepPath;
		}
		std::string fullDepPathCanonical = CanonicalPath(fullDepPathView);
		auto it = processAssetArgs.assetsToLoadByName->find(fullDepPathCanonical);

		if (it == processAssetArgs.assetsToLoadByName->end())
		{
			eg::Log(
				LogLevel::Warning, "as",
				"Load-time dependency '{0}' of asset '{1}' not found, "
				"this dependency will be ignored",
				dep, assetToLoad.name);
			continue;
		}

		if (!ProcessAsset(*it->second, processAssetArgs))
		{
			eg::Log(
				LogLevel::Warning, "as",
				"Cannot load asset '{0}' because load-time dependency '{1}' "
				"failed to load.",
				assetToLoad.name, dep);
			assetToLoad.state = AssetToLoad::STATE_FAILED;
			return false;
		}
	}

	std::vector<SideStreamData> sideStreamsData;
	for (const GeneratedAssetSideStreamData& data : assetToLoad.generatedAsset.sideStreamsData)
	{
		sideStreamsData.push_back({ .streamName = data.streamName, .data = data.data });
	}

	// Loads the asset
	const AssetLoadArgs assetLoadArgs = {
		.asset = nullptr,
		.assetPath = assetToLoad.name,
		.generatedData = assetToLoad.generatedAsset.data,
		.sideStreamsData = sideStreamsData,
		.allocator = processAssetArgs.allocator,
		.graphicsLoadContext = processAssetArgs.graphicsLoadContext,
	};
	Asset* asset = LoadAsset(*assetToLoad.loader, assetLoadArgs);
	if (asset == nullptr)
	{
		// The asset failed to load
		assetToLoad.state = AssetToLoad::STATE_FAILED;
		return false;
	}

	if (processAssetArgs.assetsToposortOut != nullptr)
	{
		processAssetArgs.assetsToposortOut->push_back(&assetToLoad);
	}

	processAssetArgs.allAssetsOut->push_back(asset);

	asset->fullName = processAssetArgs.allocator->MakeStringCopy(assetToLoad.name);
	asset->name = BaseName(asset->fullName);

	AssetDirectory* directory =
		processAssetArgs.destinationDir->FindDirectory(ParentPath(assetToLoad.name), true, processAssetArgs.allocator);
	asset->next = directory->firstAsset;
	directory->firstAsset = asset;

	assetToLoad.state = AssetToLoad::STATE_LOADED;
	return true;
}

static void FindAllFilesInDirectory(
	const std::filesystem::path& path, const std::string& prefix, std::vector<std::string>& output)
{
	for (const auto& directoryEntry : std::filesystem::directory_iterator(path))
	{
		std::string fileName = directoryEntry.path().filename().string();
		if (fileName[0] == '.')
			continue;
		if (directoryEntry.is_directory())
		{
			FindAllFilesInDirectory(directoryEntry.path(), prefix + fileName + "/", output);
		}
		else if (directoryEntry.is_regular_file())
		{
			output.push_back(prefix + fileName);
		}
	}
}

std::optional<std::vector<YAMLAssetInfo>> DetectAndGenerateYAMLAssets(
	std::string_view path, const AssetLoaderRegistry& loaderRegistry)
{
	std::string yamlPath = Concat({ path, "/Assets.yaml" });
	std::ifstream yamlStream(yamlPath, std::ios::binary);
	if (!yamlStream)
		return std::nullopt;

	std::string cachePath = Concat({ path, "/.AssetCache/" });
	std::string dirPath = yamlPath.substr(0, path.size() + 1);

	YAML::Node node = YAML::Load(yamlStream);

	std::vector<YAMLAssetInfo> assetsToLoad;
	std::unordered_set<std::string> assetsAlreadyAdded;
	auto LoadAsset = [&](std::string name, const YAML::Node& assetNode)
	{
		if (assetsAlreadyAdded.count(name))
			return;

		YAMLAssetInfo& asset = assetsToLoad.emplace_back(YAMLAssetInfo{ .name = std::move(name) });

		// Tries to find the generator and loader names
		std::string generatorName;
		if (const YAML::Node& loaderNode = assetNode["loader"])
		{
			asset.loaderName = loaderNode.as<std::string>();
			generatorName = assetNode["generator"].as<std::string>("Default");
		}
		else
		{
			std::optional<LoaderGeneratorPair> loaderAndGenerator =
				loaderRegistry.GetLoaderAndGeneratorForFileExtension(PathExtension(asset.name));

			if (loaderAndGenerator.has_value())
			{
				asset.loaderName = loaderAndGenerator->loader;
				generatorName = loaderAndGenerator->generator;
			}
			else
			{
				asset.status = YAMLAssetStatus::ErrorUnknownExtension;
				return;
			}
		}

		asset.loader = loaderRegistry.FindLoader(asset.loaderName);
		if (asset.loader == nullptr)
		{
			asset.status = YAMLAssetStatus::ErrorLoaderNotFound;
			return;
		}

		const uint64_t yamlHash = HashYAMLNode(assetNode);

		// Tries to load the asset from the cache
		std::string assetCachePath = Concat({ cachePath, asset.name, ".eab" });
		std::optional<GeneratedAsset> generated =
			TryReadAssetFromCache(dirPath, *asset.loader->format, yamlHash, assetCachePath);

		// Generates the asset if loading from the cache failed
		if (!generated.has_value())
		{
			int64_t timeBegin = NanoTime();
			generated = GenerateAsset(dirPath, generatorName, asset.name, assetNode, node);
			int64_t genDuration = NanoTime() - timeBegin;

			if (!generated.has_value())
			{
				asset.status = YAMLAssetStatus::ErrorGenerate;
				return;
			}

			asset.status = YAMLAssetStatus::Generated;

			std::ostringstream msg;
			msg << "Generated asset '" << asset.name << "' in " << std::setprecision(2) << std::fixed
				<< (static_cast<double>(genDuration) * 1E-6) << "ms";
			eg::Log(LogLevel::Info, "as", "{0}", msg.str());

			// Don't cache if the resource generated in less than 0.5ms
			constexpr int64_t CACHE_TIME_THRESHOLD = 500000;
			if (genDuration > CACHE_TIME_THRESHOLD && !HasFlag(generated->flags, AssetFlags::NeverCache))
			{
				SaveAssetToCache(*generated, yamlHash, assetCachePath);
			}
		}
		else
		{
			asset.status = YAMLAssetStatus::Cached;
		}

		asset.generatedAsset = std::move(*generated);

		assetsAlreadyAdded.insert(asset.name);
	};

	std::vector<std::string> allFilesInDirectory;
	bool hasFoundAllFilesInDirectory = false;

	for (const YAML::Node& assetNode : node["assets"])
	{
		if (const YAML::Node& patternNode = assetNode["regex"])
		{
			if (!hasFoundAllFilesInDirectory)
			{
				FindAllFilesInDirectory(path, "", allFilesInDirectory);
				hasFoundAllFilesInDirectory = true;
			}
			std::string pattern = patternNode.as<std::string>();
			std::regex regex(pattern, std::regex::extended | std::regex::optimize);
			for (const std::string& file : allFilesInDirectory)
			{
				if (std::regex_match(file, regex))
				{
					LoadAsset(file, assetNode);
				}
			}
		}
		if (const YAML::Node& nameNode = assetNode["name"])
		{
			LoadAsset(nameNode.as<std::string>(), assetNode);
		}
	}

	return assetsToLoad;
}

bool AssetManager::LoadAssetsYAML(const LoadArgs& loadArgs)
{
#if defined(__EMSCRIPTEN__)
	return false;
#else
	std::optional<std::vector<YAMLAssetInfo>> yamlAssets =
		DetectAndGenerateYAMLAssets(loadArgs.path, *loadArgs.loaderRegistry);
	if (!yamlAssets.has_value())
		return false;

	std::vector<AssetToLoad> assetsToLoad;
	assetsToLoad.reserve(yamlAssets->size());
	for (YAMLAssetInfo& asset : *yamlAssets)
	{
		switch (asset.status)
		{
		case YAMLAssetStatus::ErrorGenerate:
			Log(LogLevel::Error, "as", "Asset failed to generate: '{0}'", asset.name);
			break;
		case YAMLAssetStatus::ErrorUnknownExtension:
			Log(LogLevel::Error, "as", "Unrecognized asset extension for '{0}'", asset.name);
			break;
		case YAMLAssetStatus::ErrorLoaderNotFound:
			Log(LogLevel::Error, "as", "Asset loader not found: '{0}'", asset.loaderName);
			break;
		case YAMLAssetStatus::Cached:
		case YAMLAssetStatus::Generated:
			assetsToLoad.push_back(AssetToLoad{
				.state = AssetToLoad::STATE_INITIAL,
				.name = std::move(asset.name),
				.generatedAsset = std::move(asset.generatedAsset),
				.loader = asset.loader,
			});
			break;
		}
	}

	std::unordered_map<std::string_view, AssetToLoad*> assetsToLoadByName;
	for (AssetToLoad& asset : assetsToLoad)
	{
		assetsToLoadByName.emplace(asset.name, &asset);
	}

	AssetDirectory& mountDir = *m_rootDirectory.FindDirectory(loadArgs.mountPath, true, &m_allocator);

	std::vector<AssetToLoad*> assetsToposorted;

	for (AssetToLoad& asset : assetsToLoad)
	{
		ProcessAssetArgs processAssetArgs = {
			.destinationDir = &mountDir,
			.assetsToLoadByName = &assetsToLoadByName,
			.assetsToposortOut = loadArgs.createAssetPackage ? &assetsToposorted : nullptr,
			.allAssetsOut = &m_allAssets,
			.allocator = &m_allocator,
			.graphicsLoadContext = loadArgs.graphicsLoadContext,
		};
		ProcessAsset(asset, processAssetArgs);
	}

	if (loadArgs.createAssetPackage)
	{
		std::vector<EAPAsset> eapAssets(assetsToposorted.size());
		std::transform(
			assetsToposorted.begin(), assetsToposorted.end(), eapAssets.begin(),
			[&](const AssetToLoad* asset)
			{
				EAPAsset eapAsset;
				eapAsset.assetName = asset->name;
				eapAsset.loaderName = asset->loader->name;
				eapAsset.format = *asset->loader->format;
				eapAsset.generatedAssetData = { asset->generatedAsset.data.data(), asset->generatedAsset.data.size() };
				eapAsset.compress = !HasFlag(asset->generatedAsset.flags, AssetFlags::DisableEAPCompression) &&
			                        !loadArgs.disableAssetPackageCompression;

				for (const GeneratedAssetSideStreamData& data : asset->generatedAsset.sideStreamsData)
					eapAsset.sideStreamsData.push_back({ .streamName = data.streamName, .data = data.data });

				return eapAsset;
			});

		std::string eapPath = Concat({ loadArgs.path, ".eap" });
		WriteEAPFile(eapAssets, eapPath);
	}

	return true;
#endif
}

bool AssetManager::LoadAssetsEAP(
	std::span<const struct EAPAsset> assets, std::string_view mountPath,
	const class AssetLoaderRegistry& loaderRegistry, GraphicsLoadContext& graphicsLoadContext)
{
	AssetDirectory& mountDir = *m_rootDirectory.FindDirectory(mountPath, true, &m_allocator);

	for (const EAPAsset& eapAsset : assets)
	{
		if (eapAsset.loader == nullptr)
		{
			eg::Log(
				LogLevel::Error, "as", "EAP file references unknown loader '{0}' (by the asset '{1}')",
				eapAsset.loaderName, eapAsset.assetName);
		}

		// Checks that the format version is supported by the loader
		if (*eapAsset.loader->format != eapAsset.format)
		{
			eg::Log(
				LogLevel::Error, "as", "EAP asset '{0}' uses a format not supported by it's loader ({1})",
				eapAsset.assetName, eapAsset.loaderName);
		}

		// Loads the asset
		const AssetLoadArgs assetLoadArgs = {
			.asset = nullptr,
			.assetPath = eapAsset.assetName,
			.generatedData = eapAsset.generatedAssetData,
			.sideStreamsData = eapAsset.sideStreamsData,
			.allocator = &m_allocator,
			.graphicsLoadContext = &graphicsLoadContext,
		};
		Asset* asset = LoadAsset(*eapAsset.loader, assetLoadArgs);
		if (asset == nullptr)
		{
			eg::Log(
				LogLevel::Error, "as", "EAP asset '{0}' failed to load (with loader '{1}').", eapAsset.assetName,
				eapAsset.loaderName);
			return false;
		}

		asset->fullName = m_allocator.MakeStringCopy(eapAsset.assetName);
		asset->name = BaseName(asset->fullName);

		m_allAssets.push_back(asset);

		AssetDirectory* directory = mountDir.FindDirectory(ParentPath(eapAsset.assetName), true, &m_allocator);
		asset->next = directory->firstAsset;
		directory->firstAsset = asset;
	}

	return true;
}

bool AssetManager::LoadAssets(const LoadArgs& loadArgs)
{
	EG_ASSERT(loadArgs.loaderRegistry != nullptr);
	EG_ASSERT(loadArgs.graphicsLoadContext != nullptr);

	LoadAssetGenLibrary();

	// First, tries to load assets from a YAML list. If that fails, attempts to load from an EAP.
	if (LoadAssetsYAML(loadArgs))
	{
		Log(LogLevel::Info, "as", "Loaded asset list '{0}/Assets.yaml'.", loadArgs.path);
		return true;
	}

	LinearAllocator allocator;

	ReadEAPFileArgs readEAPFileArgs = { .allocator = &m_allocator, .loaderRegistry = loadArgs.loaderRegistry };

	std::vector<MemoryMappedFile> mappedFiles;
	std::optional<std::vector<EAPAsset>> eapAssets;
	std::string eapPath = Concat({ loadArgs.path, ".eap" });
	if (auto downloadedPackageData = detail::WebGetDownloadedAssetPackage(eapPath))
	{
		OpenSideStreamFn openSideStream = [&](std::string_view sideStreamName) -> std::span<const char>
		{ return detail::WebGetDownloadedAssetPackage(eapPath).value_or(std::span<const char>()); };

		eapAssets = ReadEAPFile(*downloadedPackageData, openSideStream, readEAPFileArgs);
	}
	else
	{
		ShouldLoadSideStreamFn shouldLoadSideStream = [&](std::string_view sideStreamName)
		{ return Contains(loadArgs.enabledSideStreams, sideStreamName); };

		auto readFromFSResult = ReadEAPFileFromFileSystem(eapPath, shouldLoadSideStream, readEAPFileArgs);

		if (readFromFSResult.has_value())
		{
			eapAssets = std::move(readFromFSResult->assets);
			mappedFiles = std::move(readFromFSResult->mappedFiles);
		}
	}

	if (eapAssets.has_value() &&
	    LoadAssetsEAP(*eapAssets, loadArgs.mountPath, *loadArgs.loaderRegistry, *loadArgs.graphicsLoadContext))
	{
		Log(LogLevel::Info, "as", "Loaded asset package '{0}'.", eapPath);
		return true;
	}
	else
	{
		Log(LogLevel::Error, "as", "Failed to load assets from '{0}'. Both '{1}' and '{0}/Assets.yaml' failed to load.",
		    loadArgs.path, eapPath);
		return false;
	}
}

#ifdef __EMSCRIPTEN__
void LoadAssetGenLibrary() {}
#else
static std::atomic_bool hasLoadedAssetGenLibrary = false;
static std::mutex assetGenLibraryLoadMutex;
static DynamicLibrary assetGenLibrary;

void LoadAssetGenLibrary()
{
	if (hasLoadedAssetGenLibrary)
		return;
	std::unique_lock<std::mutex> lock(assetGenLibraryLoadMutex);
	if (hasLoadedAssetGenLibrary)
		return;

	std::string libraryName = DynamicLibrary::PlatformFormat("EGameAssetGen");

	if (!assetGenLibrary.Open(libraryName.c_str()))
	{
		Log(LogLevel::Warning, "as", "Could not load asset generator library: {0}", DynamicLibrary::FailureReason());
		return;
	}

	void* initSym = assetGenLibrary.GetSymbol("Init");
	if (initSym == nullptr)
	{
		Log(LogLevel::Warning, "as", "Could not load asset generator library: missing Init.");
		return;
	}

	reinterpret_cast<void (*)()>(initSym)();

	hasLoadedAssetGenLibrary = true;
}
#endif

std::vector<std::string_view> GetDefaultEnabledAssetSideStreams()
{
	return {};
}

const Asset* AssetManager::FindAssetImpl(std::string_view name) const
{
	std::string cPath = CanonicalPath(name);
	AssetDirectory* dir = const_cast<AssetDirectory&>(m_rootDirectory).FindDirectory(ParentPath(cPath), false, nullptr);
	if (dir == nullptr)
		return nullptr;

	std::string_view baseName = BaseName(name);
	for (Asset* asset = dir->firstAsset; asset != nullptr; asset = asset->next)
	{
		if (asset->name == baseName)
			return asset;
	}

	return nullptr;
}

static void IterateAssetsR(const AssetDirectory& dir, const std::function<void(const Asset&)>& callback)
{
	for (const Asset* asset = dir.firstAsset; asset != nullptr; asset = asset->next)
	{
		callback(*asset);
	}

	for (const AssetDirectory* subDir = dir.firstChildDir; subDir != nullptr; subDir = subDir->nextSiblingDir)
	{
		IterateAssetsR(*subDir, callback);
	}
}

void AssetManager::IterateAssets(const std::function<void(const Asset&)>& callback) const
{
	IterateAssetsR(m_rootDirectory, callback);
}

AssetManager::~AssetManager()
{
	for (Asset* asset : m_allAssets)
		asset->DestroyInstance();
}
} // namespace eg
