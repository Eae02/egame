#include "Asset.hpp"
#include "AssetGenerator.hpp"
#include "AssetLoad.hpp"
#include "YAMLUtils.hpp"
#include "../Console.hpp"
#include "../Platform/DynamicLibrary.hpp"
#include "../Platform/FileSystem.hpp"
#include "../IOUtils.hpp"

#include <queue>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <regex>
#include <span>
#include <unordered_set>
#include <unordered_map>

namespace eg
{
	LinearAllocator detail::assetAllocator;
	
	bool createAssetPackage;
	bool disableAssetPackageCompression;
	
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
			//The path starts with a slash, so try again from the same directory with the slash removed
			return FindDirectory(current, path.substr(1), create);
		}
		
		std::string_view entryName;
		if (firstSlash == std::string_view::npos)
			entryName = path;
		else
			entryName = path.substr(0, firstSlash);
		
		std::string_view remPath = path.substr(std::min(entryName.size() + 1, path.size()));
		
		//Searches for the next directory in the path
		for (AssetDirectory* dir = current->firstChildDir; dir != nullptr; dir = dir->nextSiblingDir)
		{
			if (dir->name == entryName)
				return FindDirectory(dir, remPath, create);
		}
		
		if (!create)
			return nullptr;
		
		AssetDirectory* newDir = detail::assetAllocator.New<AssetDirectory>();
		
		//Initializes the name of the new directory
		char* nameBuffer = reinterpret_cast<char*>(detail::assetAllocator.Allocate(entryName.size()));
		std::memcpy(nameBuffer, entryName.data(), entryName.size());
		newDir->name = std::string_view(nameBuffer, entryName.size());
		
		//Adds the new directory to the linked list
		newDir->nextSiblingDir = current->firstChildDir;
		current->firstChildDir = newDir;
		
		return FindDirectory(newDir, remPath, create);
	}
	
	struct BoundAssetExtension
	{
		std::string_view extension;
		std::string_view loader;
		std::string_view generator;
		
		bool operator<(const std::string_view& other) const
		{
			return extension < other;
		}
	};
	
	std::vector<BoundAssetExtension> assetExtensions;
	
	void BindAssetExtension(std::string_view extension, std::string_view loader, std::string_view generator)
	{
		auto it = std::lower_bound(assetExtensions.begin(), assetExtensions.end(), extension);
		if (it != assetExtensions.end() && it->extension == extension)
		{
			Log(LogLevel::Warning, "as", "Re-binding asset extension '{0}'", extension);
		}
		else
		{
			it = assetExtensions.emplace(it);
			it->extension = detail::assetAllocator.MakeStringCopy(extension);
		}
		it->loader = detail::assetAllocator.MakeStringCopy(loader);
		it->generator = detail::assetAllocator.MakeStringCopy(generator);
	}
	
	static const char cachedAssetMagic[] = { (char)0xFF, 'E', 'A', 'C' };
	
	static void SaveAssetToCache(const GeneratedAsset& asset, uint64_t yamlParamsHash, const std::string& cachePath)
	{
		eg::CreateDirectories(ParentPath(cachePath, false));
		
		std::ofstream stream(cachePath, std::ios::binary);
		
		stream.write(cachedAssetMagic, sizeof(cachedAssetMagic));
		BinWrite(stream, yamlParamsHash);
		BinWrite(stream, asset.format.nameHash);
		BinWrite(stream, asset.format.version);
		BinWrite(stream, (uint32_t)asset.flags);
		BinWrite(stream, (uint64_t)time(nullptr));
		
		BinWrite(stream, (uint32_t)asset.fileDependencies.size());
		for (const std::string& dep : asset.fileDependencies)
			BinWriteString(stream, dep);
		
		BinWrite(stream, (uint32_t)asset.loadDependencies.size());
		for (const std::string& dep : asset.loadDependencies)
			BinWriteString(stream, dep);
		
		BinWrite(stream, (uint32_t)asset.data.size());
		stream.write(asset.data.data(), asset.data.size());
	}
	
	static std::optional<GeneratedAsset> TryReadAssetFromCache(const std::string& currentDirPath,
		const AssetFormat& expectedFormat, uint64_t expectedYamlHash, const std::string& cachePath)
	{
		std::ifstream stream(cachePath, std::ios::binary);
		if (!stream)
			return { };
		
		char magic[sizeof(cachedAssetMagic)];
		stream.read(magic, sizeof(magic));
		if (std::memcmp(cachedAssetMagic, magic, sizeof(magic)))
			return { };
		
		uint64_t yamlHash = BinRead<uint64_t>(stream);
		if (yamlHash != expectedYamlHash)
			return { };
		
		GeneratedAsset asset;
		
		asset.format.nameHash = BinRead<uint32_t>(stream);
		asset.format.version = BinRead<uint32_t>(stream);
		if (asset.format.nameHash != expectedFormat.nameHash || asset.format.version != expectedFormat.version)
			return { };
		
		asset.flags = (AssetFlags)BinRead<uint32_t>(stream);
		const auto generateTime = std::chrono::system_clock::from_time_t(BinRead<uint64_t>(stream));
		
		const uint32_t numFileDependencies = BinRead<uint32_t>(stream);
		asset.fileDependencies.reserve(numFileDependencies);
		for (uint32_t i = 0; i < numFileDependencies; i++)
		{
			std::string dependency = BinReadString(stream);
			std::string dependencyFullPath = currentDirPath + dependency;
			if (LastWriteTime(dependencyFullPath.c_str()) > generateTime)
				return { };
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
		
		State state;
		std::string name;
		GeneratedAsset generatedAsset;
		const AssetLoader* loader;
	};
	
	struct EAPWriteContext
	{
		uint32_t numAssets;
		std::ostringstream outStream;
		bool disableAllCompression = false;
	};
	
	//Generates and loads an asset recursively so that all it's load-time dependencies are satisfied.
	static bool ProcessAsset(AssetToLoad& assetToLoad, AssetDirectory& destinationDir,
	                         const std::unordered_map<std::string_view, AssetToLoad*>& assetsToLoadByName,
	                         std::vector<AssetToLoad*>* assetsToposortOut)
	{
		switch (assetToLoad.state)
		{
		case AssetToLoad::STATE_INITIAL:
			assetToLoad.state = AssetToLoad::STATE_PROCESSING;
			break;
		case AssetToLoad::STATE_PROCESSING:
			eg::Log(LogLevel::Error, "as", "Circular load-time dependency involving '{0}'", assetToLoad.name);
			return false;
		case AssetToLoad::STATE_LOADED: return true;
		case AssetToLoad::STATE_FAILED: return false;
		}
		
		//Processes dependencies
		for (const std::string& dep : assetToLoad.generatedAsset.loadDependencies)
		{
			std::string fullDepPath = Concat({ ParentPath(assetToLoad.name), dep });
			std::string fullDepPathCanonical = CanonicalPath(fullDepPath);
			auto it = assetsToLoadByName.find(fullDepPathCanonical);
			
			if (it == assetsToLoadByName.end())
			{
				eg::Log(LogLevel::Warning, "as", "Load-time dependency '{0}' of asset '{1}' not found, "
				        "this dependency will be ignored", dep, assetToLoad.name);
				continue;
			}
			
			if (!ProcessAsset(*it->second, destinationDir, assetsToLoadByName, assetsToposortOut))
			{
				eg::Log(LogLevel::Warning, "as", "Cannot load asset '{0}' because load-time dependency '{1}' "
				        "failed to load.", assetToLoad.name, dep);
				assetToLoad.state = AssetToLoad::STATE_FAILED;
				return false;
			}
		}
		
		//Loads the asset
		Asset* asset = LoadAsset(*assetToLoad.loader, assetToLoad.name, assetToLoad.generatedAsset.data, nullptr);
		if (asset == nullptr)
		{
			//The asset failed to load
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
	
	static const char EAPMagic[] = { (char)0xFF, 'E', 'A', 'P' };
	
	static void FindAllFilesInDirectory(const std::filesystem::path& path, std::string prefix, std::vector<std::string>& output)
	{
		for (auto directoryEntry : std::filesystem::directory_iterator(path))
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
	
	static bool LoadAssetsYAML(const std::string& path, std::string_view mountPath)
	{
#if defined(__EMSCRIPTEN__) || !defined(EG_HAS_YAML_CPP)
		return false;
#else
		std::string yamlPath = path + "/Assets.yaml";
		std::ifstream yamlStream(yamlPath, std::ios::binary);
		if (!yamlStream)
			return false;
		
		std::string cachePath = path + "/.AssetCache/";
		std::string dirPath = yamlPath.substr(0, path.size() + 1);
		
		YAML::Node node = YAML::Load(yamlStream);
		
		std::vector<AssetToLoad> assetsToLoad;
		std::unordered_map<std::string_view, AssetToLoad*> assetsToLoadByName;
		std::unordered_set<std::string> assetsAlreadyAdded;
		auto LoadAsset = [&] (std::string name, const YAML::Node& assetNode)
		{
			if (assetsAlreadyAdded.count(name))
				return;
			
			AssetToLoad assetToLoad;
			assetToLoad.name = std::move(name);
			assetToLoad.state = AssetToLoad::STATE_INITIAL;
			
			//Tries to find the generator and loader names
			std::string loaderName;
			std::string generatorName;
			if (const YAML::Node& loaderNode = assetNode["loader"])
			{
				loaderName = loaderNode.as<std::string>();
				generatorName = assetNode["generator"].as<std::string>("Default");
			}
			else
			{
				std::string_view extension = PathExtension(assetToLoad.name);
				auto it = std::lower_bound(assetExtensions.begin(), assetExtensions.end(), extension);
				if (it != assetExtensions.end() && it->extension == extension)
				{
					loaderName = it->loader;
					generatorName = it->generator;
				}
				else
				{
					Log(LogLevel::Error, "as", "Unrecognized asset extension for '{0}'", assetToLoad.name);
					return;
				}
			}
			
			assetToLoad.loader = FindAssetLoader(loaderName);
			if (assetToLoad.loader == nullptr)
			{
				Log(LogLevel::Error, "as", "Asset loader not found: '{0}'.", loaderName);
				return;
			}
			
			const uint64_t yamlHash = HashYAMLNode(assetNode);
			
			//Tries to load the asset from the cache
			std::string assetCachePath = Concat({ cachePath, assetToLoad.name, ".eab" });
			std::optional<GeneratedAsset> generated =
				TryReadAssetFromCache(dirPath, *assetToLoad.loader->format, yamlHash, assetCachePath);
			
			//Generates the asset if loading from the cache failed
			if (!generated.has_value())
			{
				int64_t timeBegin = NanoTime();
				generated = GenerateAsset(dirPath, generatorName, assetToLoad.name, assetNode);
				int64_t genDuration = NanoTime() - timeBegin;
				
				if (!generated.has_value())
					return;
				
				std::ostringstream msg;
				msg << "Generated asset '" << assetToLoad.name << "' in " <<
					std::setprecision(2) << std::fixed << ((double)genDuration * 1E-6) << "ms";
				eg::Log(LogLevel::Info, "as", "{0}", msg.str());
				
				//Don't cache if the resource generated in less than 0.5ms
				constexpr int64_t CACHE_TIME_THRESHOLD = 500000;
				if (genDuration > CACHE_TIME_THRESHOLD && !HasFlag(generated->flags, AssetFlags::NeverCache))
				{
					SaveAssetToCache(*generated, yamlHash, assetCachePath);
				}
			}
			
			assetsAlreadyAdded.insert(assetToLoad.name);
			assetToLoad.generatedAsset = std::move(*generated);
			assetsToLoad.push_back(std::move(assetToLoad));
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
		
		for (AssetToLoad& asset : assetsToLoad)
		{
			assetsToLoadByName.emplace(asset.name, &asset);
		}
		
		AssetDirectory& mountDir = *FindDirectory(&assetRootDir, mountPath, true);
		
		std::vector<AssetToLoad*> assetsToposorted;
		
		for (AssetToLoad& asset : assetsToLoad)
		{
			ProcessAsset(asset, mountDir, assetsToLoadByName, createAssetPackage ? &assetsToposorted : nullptr);
		}
		
		if (createAssetPackage)
		{
			std::ofstream eapStream(path + ".eap", std::ios::binary);
			eapStream.write(EAPMagic, sizeof(EAPMagic));
			BinWrite(eapStream, (uint32_t)assetsToposorted.size());
			
			//Extracts loader names
			std::vector<std::string_view> loaderNames;
			for (const AssetToLoad* asset : assetsToposorted)
				loaderNames.push_back(asset->loader->name);
			std::sort(loaderNames.begin(), loaderNames.end());
			loaderNames.erase(std::unique(loaderNames.begin(), loaderNames.end()), loaderNames.end());
			
			//Writes loader names
			BinWrite(eapStream, (uint32_t)loaderNames.size());
			for (std::string_view loader : loaderNames)
				BinWriteString(eapStream, loader);
			
			for (const AssetToLoad* asset : assetsToposorted)
			{
				uint32_t loaderIndex = std::lower_bound(loaderNames.begin(), loaderNames.end(), asset->loader->name) - loaderNames.begin();
				
				BinWriteString(eapStream, asset->name);
				BinWrite(eapStream, loaderIndex);
				BinWrite(eapStream, asset->loader->format->nameHash);
				BinWrite(eapStream, asset->loader->format->version);
				
				bool compress = !disableAssetPackageCompression &&
					!eg::HasFlag(asset->generatedAsset.flags, AssetFlags::DisableEAPCompression);
				
				uint64_t dataBytes = asset->generatedAsset.data.size();
				EG_ASSERT(dataBytes < 0x8000000000000000ULL);
				if (compress)
					dataBytes |= 0x8000000000000000ULL;
				
				BinWrite(eapStream, dataBytes);
				
				if (compress)
				{
					WriteCompressedSection(eapStream, asset->generatedAsset.data.data(), dataBytes);
				}
				else
				{
					eapStream.write(asset->generatedAsset.data.data(), dataBytes);
				}
			}
		}
		
		return true;
#endif
	}
	
	bool LoadAssetsFromEAPStream(std::istream& stream, std::string_view mountPath)
	{
		AssetDirectory& mountDir = *FindDirectory(&assetRootDir, mountPath, true);
		
		char magic[sizeof(EAPMagic)];
		stream.read(magic, sizeof(magic));
		if (std::memcmp(magic, EAPMagic, sizeof(magic)) != 0)
			return false;
		
		const uint32_t numAssets = BinRead<uint32_t>(stream);
		const uint32_t numLoaderNames = BinRead<uint32_t>(stream);
		std::vector<const AssetLoader*> loaders(numLoaderNames);
		for (uint32_t i = 0; i < numLoaderNames; i++)
		{
			std::string loaderName = BinReadString(stream);
			loaders[i] = FindAssetLoader(loaderName);
			if (loaders[i] == nullptr)
			{
				eg::Log(LogLevel::Error, "as", "EAP file references unknown loader '{0}'", loaderName);
				return false;
			}
		}
		
		std::vector<char> dataTempBuffer;
		
		for (uint32_t i = 0; i < numAssets; i++)
		{
			std::string name = BinReadString(stream);
			
			uint32_t loaderIndex = BinRead<uint32_t>(stream);
			if (loaderIndex >= numLoaderNames)
			{
				eg::Log(LogLevel::Error, "as", "Corrupt EAP, references out of range loader {0}.", loaderIndex);
				return false;
			}
			
			//Checks that the format version is supported by the loader
			const uint32_t formatNameHash = BinRead<uint32_t>(stream);
			const uint32_t formatVersion = BinRead<uint32_t>(stream);
			if (formatNameHash != loaders[loaderIndex]->format->nameHash ||
				formatVersion != loaders[loaderIndex]->format->version)
			{
				eg::Log(LogLevel::Error, "as", "EAP asset '{0}' uses a format not supported by it's loader ({1})",
					name, loaders[loaderIndex]->name);
				return false;
			}
			
			uint64_t dataBytes = BinRead<uint64_t>(stream);
			bool compressed = (dataBytes & 0x8000000000000000ULL) != 0;
			dataBytes &= ~0x8000000000000000ULL;
			
			if (dataTempBuffer.size() < dataBytes)
				dataTempBuffer.resize(dataBytes);
			
			if (compressed)
			{
				ReadCompressedSection(stream, dataTempBuffer.data(), dataBytes);
			}
			else
			{
				stream.read(dataTempBuffer.data(), dataBytes);
			}
			
			//Loads the asset
			Asset* asset = LoadAsset(*loaders[loaderIndex], name, std::span<const char>(dataTempBuffer.data(), dataBytes), nullptr);
			if (asset == nullptr)
			{
				eg::Log(LogLevel::Error, "as", "EAP asset '{0}' failed to load.", name);
				return false;
			}
			
			asset->fullName = detail::assetAllocator.MakeStringCopy(name);
			asset->name = BaseName(asset->fullName);
			
			AssetDirectory* directory = FindDirectory(&mountDir, ParentPath(name), true);
			asset->next = directory->firstAsset;
			directory->firstAsset = asset;
		}
		
		return true;
	}
	
	bool LoadAssets(const std::string& path, std::string_view mountPath)
	{
		//First, tries to load assets from a YAML list. If that fails, attempts to load from an EAP.
		if (LoadAssetsYAML(path, mountPath))
		{
			Log(LogLevel::Info, "as", "Loaded asset list '{0}/Assets.yaml'.", path);
			return true;
		}
		
		std::ifstream stream(path + ".eap", std::ios::binary);
		if (LoadAssetsFromEAPStream(stream, mountPath))
		{
			Log(LogLevel::Info, "as", "Loaded asset package '{0}.eap'.", path);
			return true;
		}
		
		Log(LogLevel::Error, "as", "Failed to load assets from '{0}'. Both '{0}.eap' and '{0}/Assets.yaml' failed to load.", path);
		return false;
	}
	
#ifdef __EMSCRIPTEN__
	void LoadAssetGenLibrary() { }
#else
	static DynamicLibrary assetGenLibrary;
	
	void LoadAssetGenLibrary()
	{
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
		
		reinterpret_cast<void(*)()>(initSym)();
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
		IterateAssets([&] (const Asset& asset)
		{
			if (assetType == nullptr || asset.assetType == *assetType)
			{
				list.Add(asset.fullName);
			}
		});
	}
}
