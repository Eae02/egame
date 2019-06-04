#include "Asset.hpp"
#include "AssetGenerator.hpp"
#include "AssetLoad.hpp"
#include "../Platform/DynamicLibrary.hpp"
#include "../Platform/FileSystem.hpp"
#include "../Alloc/LinearAllocator.hpp"
#include "../Log.hpp"
#include "../IOUtils.hpp"

#include <yaml-cpp/yaml.h>
#include <queue>
#include <ctime>
#include <fstream>
#include <condition_variable>
#include <iomanip>

namespace eg
{
	LinearAllocator detail::assetAllocator;
	
	bool createAssetPackage;
	
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
	
	static const char cachedAssetMagic[] = { (char)0xFF, 'E', 'G', 'A' };
	
	static void SaveAssetToCache(const GeneratedAsset& asset, const std::string& cachePath)
	{
		eg::CreateDirectories(ParentPath(cachePath, false));
		
		std::ofstream stream(cachePath, std::ios::binary);
		
		stream.write(cachedAssetMagic, sizeof(cachedAssetMagic));
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
		const AssetFormat& expectedFormat, const std::string& cachePath)
	{
		std::ifstream stream(cachePath, std::ios::binary);
		if (!stream)
			return { };
		
		char magic[sizeof(cachedAssetMagic)];
		stream.read(magic, sizeof(magic));
		if (std::memcmp(cachedAssetMagic, magic, sizeof(magic)))
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
	};
	
	//Generates and loads an asset recursively so that all it's load-time dependencies are satisfied.
	static bool ProcessAsset(AssetToLoad& assetToLoad, AssetDirectory& destinationDir,
		std::vector<AssetToLoad>& assetsToLoad, EAPWriteContext* eapWriteContext)
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
			auto it = std::find_if(assetsToLoad.begin(), assetsToLoad.end(), [&] (const AssetToLoad& asset)
			{
				return asset.name == fullDepPathCanonical;
			});
			
			if (it == assetsToLoad.end())
			{
				eg::Log(LogLevel::Warning, "as", "Load-time dependency '{0}' of asset '{1}' not found, "
				        "this dependency will be ignored", dep, assetToLoad.name);
				continue;
			}
			
			if (!ProcessAsset(*it, destinationDir, assetsToLoad, eapWriteContext))
			{
				eg::Log(LogLevel::Warning, "as", "Cannot load asset '{0}' because load-time dependency '{1}' "
				        "failed to load.", assetToLoad.name, dep);
				assetToLoad.state = AssetToLoad::STATE_FAILED;
				return false;
			}
		}
		
		//Loads the asset
		std::string_view dirPath = ParentPath(assetToLoad.name, true);
		Asset* asset = LoadAsset(*assetToLoad.loader, dirPath, assetToLoad.generatedAsset.data, nullptr);
		if (asset == nullptr)
		{
			//The asset failed to load
			assetToLoad.state = AssetToLoad::STATE_FAILED;
			return false;
		}
		
		//Writes the asset to the EAP stream
		if (eapWriteContext != nullptr && !HasFlag(assetToLoad.generatedAsset.flags, AssetFlags::NeverPackage))
		{
			BinWriteString(eapWriteContext->outStream, assetToLoad.name);
			BinWriteString(eapWriteContext->outStream, assetToLoad.loader->name);
			BinWrite(eapWriteContext->outStream, assetToLoad.loader->format->nameHash);
			BinWrite(eapWriteContext->outStream, assetToLoad.loader->format->version);
			
			const size_t dataBytes = assetToLoad.generatedAsset.data.size();
			BinWrite(eapWriteContext->outStream, (uint32_t)dataBytes);
			eapWriteContext->outStream.write(assetToLoad.generatedAsset.data.data(), dataBytes);
			
			eapWriteContext->numAssets++;
		}
		
		asset->name = detail::assetAllocator.MakeStringCopy(BaseName(assetToLoad.name));
		
		AssetDirectory* directory = FindDirectory(&destinationDir, dirPath, true);
		asset->next = directory->firstAsset;
		directory->firstAsset = asset;
		
		assetToLoad.state = AssetToLoad::STATE_LOADED;
		return true;
	}
	
	static const char EAPMagic[] = { (char)0xFF, 'E', 'A', 'P' };
	
	static bool LoadAssetsYAML(const std::string& path, AssetDirectory& mountDir)
	{
#ifdef __EMSCRIPTEN__
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
		for (const YAML::Node& assetNode : node["assets"])
		{
			const YAML::Node& nameNode = assetNode["name"];
			if (!nameNode)
				continue;
			
			AssetToLoad assetToLoad;
			assetToLoad.name = nameNode.as<std::string>();
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
					continue;
				}
			}
			
			assetToLoad.loader = FindAssetLoader(loaderName);
			if (assetToLoad.loader == nullptr)
			{
				Log(LogLevel::Error, "as", "Asset loader not found: '{0}'.", loaderName);
				continue;
			}
			
			//Tries to load the asset from the cache
			std::string assetCachePath = Concat({ cachePath, assetToLoad.name, ".eab" });
			std::optional<GeneratedAsset> generated =
				TryReadAssetFromCache(dirPath, *assetToLoad.loader->format, assetCachePath);
			
			//Generates the asset if loading from the cache failed
			if (!generated.has_value())
			{
				int64_t timeBegin = NanoTime();
				generated = GenerateAsset(dirPath, generatorName, assetNode);
				int64_t genDuration = NanoTime() - timeBegin;
				
				if (!generated.has_value())
					continue;
				
				std::ostringstream msg;
				msg << "Generated asset '" << assetToLoad.name << "' in " <<
					std::setprecision(2) << std::fixed << (genDuration * 1E-6) << "ms";
				eg::Log(LogLevel::Info, "as", "{0}", msg.str());
				
				//Don't cache if the resource generated in less than 0.5ms
				constexpr int64_t CACHE_TIME_THRESHOLD = 500000;
				if (genDuration > CACHE_TIME_THRESHOLD && !HasFlag(generated->flags, AssetFlags::NeverCache))
				{
					SaveAssetToCache(*generated, assetCachePath);
				}
			}
			
			assetToLoad.generatedAsset = std::move(*generated);
			assetsToLoad.push_back(std::move(assetToLoad));
		}
		
		EAPWriteContext eapWriteContext;
		eapWriteContext.numAssets = 0;
		
		for (AssetToLoad& asset : assetsToLoad)
		{
			ProcessAsset(asset, mountDir, assetsToLoad, createAssetPackage ? &eapWriteContext : nullptr);
		}
		
		if (createAssetPackage)
		{
			std::ofstream eapStream(path + ".eap", std::ios::binary);
			eapStream.write(EAPMagic, sizeof(EAPMagic));
			BinWrite(eapStream, eapWriteContext.numAssets);
			
			std::string uncompressedData = eapWriteContext.outStream.str();
			BinWrite(eapStream, (uint64_t)uncompressedData.size());
			WriteCompressedSection(eapStream, uncompressedData.data(), uncompressedData.size());
		}
		
		return true;
#endif
	}
	
	static bool LoadAssetsEAP(const std::string& path, AssetDirectory& mountDir)
	{
		std::ifstream stream(path + ".eap", std::ios::binary);
		if (!stream)
			return false;
		
		char magic[sizeof(EAPMagic)];
		stream.read(magic, sizeof(magic));
		if (std::memcmp(magic, EAPMagic, sizeof(magic)))
			return false;
		
		const uint32_t numAssets = BinRead<uint32_t>(stream);
		const uint64_t uncompressedDataSize = BinRead<uint64_t>(stream);
		std::unique_ptr<char[]> uncompressedData = std::make_unique<char[]>(uncompressedDataSize);
		ReadCompressedSection(stream, uncompressedData.get(), uncompressedDataSize);
		
		stream.close();
		
		size_t dataPos = 0;
		Span<char> dataSpan(uncompressedData.get(), uncompressedDataSize);
		
		auto NextString = [&]
		{
			uint16_t nameLen = dataSpan.AtAs<uint16_t>(dataPos);
			dataPos += sizeof(uint16_t);
			if (dataPos + nameLen > dataSpan.size())
				EG_PANIC("Corrupt EAP");
			std::string_view str(dataSpan.data() + dataPos, nameLen);
			dataPos += nameLen;
			return str;
		};
		
		for (uint32_t i = 0; i < numAssets; i++)
		{
			std::string_view name = NextString();
			std::string_view loaderName = NextString();
			
			//Finds the loader
			const AssetLoader* loader = FindAssetLoader(loaderName);
			if (loader == nullptr)
			{
				eg::Log(LogLevel::Error, "as", "EAP asset '{0}' references unknown loader '{1}'", name, loaderName);
				return false;
			}
			
			//Checks that the format version is supported by the loader
			const uint32_t formatNameHash = dataSpan.AtAs<uint32_t>(dataPos);
			const uint32_t formatVersion = dataSpan.AtAs<uint32_t>(dataPos + sizeof(uint32_t));
			if (formatNameHash != loader->format->nameHash || formatVersion != loader->format->version)
			{
				eg::Log(LogLevel::Error, "as", "EAP asset '{0}' uses a format not supported by it's loader ({1})",
					name, loaderName);
				return false;
			}
			dataPos += sizeof(uint32_t) * 2;
			
			//Reads the asset's data section
			uint32_t dataBytes = dataSpan.AtAs<uint32_t>(dataPos);
			dataPos += sizeof(uint32_t);
			if (dataPos + dataBytes > dataSpan.size())
				EG_PANIC("Corrupt EAP");
			Span<char> data(dataSpan.data() + dataPos, dataBytes);
			dataPos += dataBytes;
			
			//Loads the asset
			std::string_view dirPath = ParentPath(name, true);
			Asset* asset = LoadAsset(*loader, dirPath, data, nullptr);
			if (asset != nullptr)
			{
				asset->name = detail::assetAllocator.MakeStringCopy(BaseName(name));
				
				AssetDirectory* directory = FindDirectory(&mountDir, dirPath, true);
				asset->next = directory->firstAsset;
				directory->firstAsset = asset;
			}
		}
		
		return true;
	}
	
	void LoadAssets(const std::string& path, std::string_view mountPath)
	{
		AssetDirectory* mountDir = FindDirectory(&assetRootDir, mountPath, true);
		
		//First, tries to load assets from a YAML list. If that fails, attempts to load from an EAP.
		if (!LoadAssetsYAML(path, *mountDir) && !LoadAssetsEAP(path, *mountDir))
		{
			EG_PANIC("Failed to load assets from '{0}'. Both '{0}.eap' and '{0}/Assets.yaml' failed to load.");
		}
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
}
