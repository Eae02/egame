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
#include <fstream>
#include <condition_variable>

namespace eg
{
	LinearAllocator detail::assetAllocator;
	
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
		std::ofstream stream(cachePath, std::ios::binary);
		
		stream.write(cachedAssetMagic, sizeof(cachedAssetMagic));
		BinWrite(stream, asset.format.nameHash);
		BinWrite(stream, asset.format.version);
		BinWrite(stream, (uint32_t)asset.flags);
		
		BinWrite(stream, (uint32_t)asset.dependencies.size());
		for (const std::string& dep : asset.dependencies)
			BinWriteString(stream, dep);
		
		BinWrite(stream, (uint32_t)asset.data.size());
		stream.write(asset.data.data(), asset.data.size());
	}
	
	static std::optional<GeneratedAsset> TryReadAssetFromCache(const std::string& cachePath)
	{
		//TODO: Implement caching
		return { };
	}
	
	bool LoadAssetsYAML(AssetDirectory* mountDir, const std::string& path)
	{
		std::string yamlPath = path + "/Assets.yaml";
		std::ifstream yamlStream(yamlPath, std::ios::binary);
		if (!yamlStream)
			return false;
		
		std::string cachePath = path + "/.gen/";
		std::string_view dirPath = std::string_view(yamlPath).substr(0, path.size() + 1);
		
		YAML::Node node = YAML::Load(yamlStream);
		
		for (const YAML::Node& assetNode : node["assets"])
		{
			const YAML::Node& nameNode = assetNode["name"];
			if (!nameNode)
				continue;
			std::string name = nameNode.as<std::string>();
			
			//Tries to find the generator and loader names
			std::string generatorS, loaderS;
			std::string_view generator, loader;
			if (const YAML::Node& loaderNode = assetNode["loader"])
			{
				loader = (loaderS = loaderNode.as<std::string>());
				generator = (generatorS = assetNode["generator"].as<std::string>("Default"));
			}
			else
			{
				std::string_view extension = PathExtension(name);
				auto it = std::lower_bound(assetExtensions.begin(), assetExtensions.end(), extension);
				if (it != assetExtensions.end() && it->extension == extension)
				{
					loader = it->loader;
					generator = it->generator;
				}
				else
				{
					Log(LogLevel::Error, "as", "Unrecognized asset extension for '{0}'", name);
					continue;
				}
			}
			
			std::optional<GeneratedAsset> generatedAsset = TryReadAssetFromCache(cachePath + name);
			
			//Generates the asset if loading from the cache failed
			if (!generatedAsset.has_value())
			{
				generatedAsset = GenerateAsset(dirPath, generator, assetNode);
				if (!generatedAsset.has_value())
					continue;
			}
			
			//Loads the asset
			Asset* asset = LoadAsset(loader, generatedAsset->data, nullptr);
			if (asset == nullptr)
				continue;
			
			asset->name = detail::assetAllocator.MakeStringCopy(BaseName(name));
			
			AssetDirectory* directory = FindDirectory(mountDir, ParentPath(name, false), true);
			asset->next = directory->firstAsset;
			directory->firstAsset = asset;
			
			Log(LogLevel::Info, "as", "Loaded asset '{0}' with loader {1}", name, loader);
		}
		
		return true;
	}
	
	void LoadAssets(const std::string& path, std::string_view mountPath)
	{
		AssetDirectory* mountDir = FindDirectory(&assetRootDir, mountPath, true);
		
		if (LoadAssetsYAML(mountDir, path))
			return;
		
		EG_PANIC("Assets failed to load");
	}
	
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
	
	const Asset* detail::FindAsset(std::string_view name)
	{
		AssetDirectory* dir = FindDirectory(&assetRootDir, ParentPath(name), false);
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
}
