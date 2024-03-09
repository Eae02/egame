#include "Asset.hpp"
#include "../Compression.hpp"
#include "../Console.hpp"
#include "../IOUtils.hpp"
#include "../Platform/DynamicLibrary.hpp"
#include "../Platform/FileSystem.hpp"
#include "AssetGenerator.hpp"
#include "AssetLoad.hpp"
#include "EAPFile.hpp"
#include "WebAssetDownload.hpp"
#include "YAMLUtils.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <queue>
#include <regex>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <yaml-cpp/yaml.h>

namespace eg
{
LinearAllocator detail::assetAllocator;

bool detail::createAssetPackage;
bool detail::disableAssetPackageCompression;

struct AssetDirectory
{
	std::string_view name;
	Asset* firstAsset = nullptr;
	AssetDirectory* firstChildDir = nullptr;
	AssetDirectory* nextSiblingDir = nullptr;
};

static AssetDirectory assetRootDir;

static AssetDirectory* FindDirectory(AssetDirectory* current, std::string_view path, bool create)
{
	if (path.empty())
		return current;

	size_t firstSlash = path.find('/');
	if (firstSlash == 0)
	{
		// The path starts with a slash, so try again from the same directory with the slash removed
		return FindDirectory(current, path.substr(1), create);
	}

	std::string_view entryName;
	if (firstSlash == std::string_view::npos)
		entryName = path;
	else
		entryName = path.substr(0, firstSlash);

	std::string_view remPath = path.substr(std::min(entryName.size() + 1, path.size()));

	// Searches for the next directory in the path
	for (AssetDirectory* dir = current->firstChildDir; dir != nullptr; dir = dir->nextSiblingDir)
	{
		if (dir->name == entryName)
			return FindDirectory(dir, remPath, create);
	}

	if (!create)
		return nullptr;

	AssetDirectory* newDir = detail::assetAllocator.New<AssetDirectory>();

	// Initializes the name of the new directory
	char* nameBuffer = reinterpret_cast<char*>(detail::assetAllocator.Allocate(entryName.size()));
	std::memcpy(nameBuffer, entryName.data(), entryName.size());
	newDir->name = std::string_view(nameBuffer, entryName.size());

	// Adds the new directory to the linked list
	newDir->nextSiblingDir = current->firstChildDir;
	current->firstChildDir = newDir;

	return FindDirectory(newDir, remPath, create);
}

struct BoundAssetExtension
{
	std::string_view loader;
	std::string_view generator;
};

std::unordered_map<std::string_view, BoundAssetExtension> assetExtensions = {
	{ "glsl", { .loader = "Shader", .generator = "Shader" } },
	{ "png", { .loader = "Texture2D", .generator = "Texture2D" } },
	{ "obj", { .loader = "Model", .generator = "OBJModel" } },
	{ "gltf", { .loader = "Model", .generator = "GLTFModel" } },
	{ "glb", { .loader = "Model", .generator = "GLTFModel" } },
	{ "ype", { .loader = "ParticleEmitter", .generator = "ParticleEmitter" } },
	{ "ttf", { .loader = "SpriteFont", .generator = "Font" } },
	{ "ogg", { .loader = "AudioClip", .generator = "OGGVorbis" } },
};

void BindAssetExtension(std::string_view extension, std::string_view loader, std::string_view generator)
{
	BoundAssetExtension newEntry = {
		.loader = detail::assetAllocator.MakeStringCopy(loader),
		.generator = detail::assetAllocator.MakeStringCopy(generator),
	};

	if (assetExtensions.contains(extension))
	{
		assetExtensions[extension] = newEntry;
		Log(LogLevel::Warning, "as", "Re-binding asset extension '{0}'", extension);
	}
	else
	{
		assetExtensions.emplace(detail::assetAllocator.MakeStringCopy(extension), newEntry);
	}
}

static const char cachedAssetMagic[] = { -1, 'E', 'A', 'C' };

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

	BinWrite(stream, UnsignedNarrow<uint32_t>(asset.data.size()));
	stream.write(asset.data.data(), asset.data.size());
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
	const auto generateTime = std::chrono::system_clock::from_time_t(BinRead<uint64_t>(stream));

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

	const uint32_t dataSize = BinRead<uint32_t>(stream);
	asset.data.resize(dataSize);
	stream.read(asset.data.data(), dataSize);

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

// Generates and loads an asset recursively so that all it's load-time dependencies are satisfied.
static bool ProcessAsset(
	AssetToLoad& assetToLoad, AssetDirectory& destinationDir,
	const std::unordered_map<std::string_view, AssetToLoad*>& assetsToLoadByName,
	std::vector<AssetToLoad*>* assetsToposortOut)
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
		auto it = assetsToLoadByName.find(fullDepPathCanonical);

		if (it == assetsToLoadByName.end())
		{
			eg::Log(
				LogLevel::Warning, "as",
				"Load-time dependency '{0}' of asset '{1}' not found, "
				"this dependency will be ignored",
				dep, assetToLoad.name);
			continue;
		}

		if (!ProcessAsset(*it->second, destinationDir, assetsToLoadByName, assetsToposortOut))
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

	// Loads the asset
	Asset* asset = LoadAsset(*assetToLoad.loader, assetToLoad.name, assetToLoad.generatedAsset.data, nullptr);
	if (asset == nullptr)
	{
		// The asset failed to load
		assetToLoad.state = AssetToLoad::STATE_FAILED;
		return false;
	}

	if (assetsToposortOut != nullptr)
	{
		assetsToposortOut->push_back(&assetToLoad);
	}

	asset->fullName = detail::assetAllocator.MakeStringCopy(assetToLoad.name);
	asset->name = BaseName(asset->fullName);

	AssetDirectory* directory = FindDirectory(&destinationDir, ParentPath(assetToLoad.name), true);
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

std::optional<std::vector<YAMLAssetInfo>> DetectAndGenerateYAMLAssets(std::string_view path)
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
			std::string_view extension = PathExtension(asset.name);
			auto it = assetExtensions.find(extension);
			if (it != assetExtensions.end())
			{
				asset.loaderName = it->second.loader;
				generatorName = it->second.generator;
			}
			else
			{
				asset.status = YAMLAssetStatus::ErrorUnknownExtension;
				return;
			}
		}

		asset.loader = FindAssetLoader(asset.loaderName);
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

static bool LoadAssetsYAML(const std::string& path, std::string_view mountPath)
{
#if defined(__EMSCRIPTEN__)
	return false;
#else
	std::optional<std::vector<YAMLAssetInfo>> yamlAssets = DetectAndGenerateYAMLAssets(path);
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

	AssetDirectory& mountDir = *FindDirectory(&assetRootDir, mountPath, true);

	std::vector<AssetToLoad*> assetsToposorted;

	for (AssetToLoad& asset : assetsToLoad)
	{
		ProcessAsset(asset, mountDir, assetsToLoadByName, detail::createAssetPackage ? &assetsToposorted : nullptr);
	}

	if (detail::createAssetPackage)
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
			                        !detail::disableAssetPackageCompression;
				return eapAsset;
			});

		std::ofstream eapStream(path + ".eap", std::ios::binary);
		WriteEAPFile(eapAssets, eapStream);
	}

	return true;
#endif
}

bool LoadAssetsFromEAPStream(std::istream& stream, std::string_view mountPath)
{
	AssetDirectory& mountDir = *FindDirectory(&assetRootDir, mountPath, true);

	LinearAllocator allocator;
	auto eapReadResult = ReadEAPFile(stream, allocator);
	if (!eapReadResult)
	{
		eg::Log(LogLevel::Error, "as", "Invalid EAP file");
		return false;
	}

	for (const EAPAsset& eapAsset : *eapReadResult)
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
		Asset* asset = LoadAsset(*eapAsset.loader, eapAsset.assetName, eapAsset.generatedAssetData, nullptr);
		if (asset == nullptr)
		{
			eg::Log(
				LogLevel::Error, "as", "EAP asset '{0}' failed to load (with loader '{1}').", eapAsset.assetName,
				eapAsset.loaderName);
			return false;
		}

		asset->fullName = detail::assetAllocator.MakeStringCopy(eapAsset.assetName);
		asset->name = BaseName(asset->fullName);

		AssetDirectory* directory = FindDirectory(&mountDir, ParentPath(eapAsset.assetName), true);
		asset->next = directory->firstAsset;
		directory->firstAsset = asset;
	}

	return true;
}

bool LoadAssets(const std::string& path, std::string_view mountPath)
{
	// First, tries to load assets from a YAML list. If that fails, attempts to load from an EAP.
	if (LoadAssetsYAML(path, mountPath))
	{
		Log(LogLevel::Info, "as", "Loaded asset list '{0}/Assets.yaml'.", path);
		return true;
	}

	bool okEap = false;
	std::string eapPath = path + ".eap";
	if (std::istream* downloadedStream = detail::WebGetDownloadedAssetPackageStream(eapPath))
	{
		okEap = LoadAssetsFromEAPStream(*downloadedStream, mountPath);
	}
	else
	{
		std::ifstream stream(eapPath, std::ios::binary);
		okEap = stream && LoadAssetsFromEAPStream(stream, mountPath);
	}

	if (okEap)
	{
		Log(LogLevel::Info, "as", "Loaded asset package '{0}'.", eapPath);
	}
	else
	{
		Log(LogLevel::Error, "as",
		    "Failed to load assets from '{0}'. "
		    "Both '{1}' and '{0}/Assets.yaml' failed to load.",
		    path, eapPath);
	}
	return okEap;
}

#ifdef __EMSCRIPTEN__
void detail::LoadAssetGenLibrary() {}
#else
static DynamicLibrary assetGenLibrary;
static bool hasLoadedAssetGenLibrary = false;

void LoadAssetGenLibrary()
{
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

	hasLoadedAssetGenLibrary = true;

	reinterpret_cast<void (*)()>(initSym)();
}
#endif

const Asset* detail::FindAsset(std::string_view name)
{
	std::string cPath = CanonicalPath(name);
	AssetDirectory* dir = FindDirectory(&assetRootDir, ParentPath(cPath), false);
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

static void UnloadAssetsR(AssetDirectory* dir)
{
	for (Asset* asset = dir->firstAsset; asset != nullptr; asset = asset->next)
	{
		asset->DestroyInstance();
	}

	for (AssetDirectory* subDir = dir->firstChildDir; subDir != nullptr; subDir = subDir->nextSiblingDir)
	{
		UnloadAssetsR(subDir);
	}
}

void UnloadAssets()
{
	UnloadAssetsR(&assetRootDir);
	detail::assetAllocator.Reset();
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

void IterateAssets(const std::function<void(const Asset&)>& callback)
{
	IterateAssetsR(assetRootDir, callback);
}

void AssetCommandCompletionProvider(console::CompletionsList& list, const std::type_index* assetType)
{
	IterateAssets(
		[&](const Asset& asset)
		{
			if (assetType == nullptr || asset.assetType == *assetType)
			{
				list.Add(asset.fullName);
			}
		});
}
} // namespace eg
