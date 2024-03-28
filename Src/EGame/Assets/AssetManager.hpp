#pragma once

#include "../API.hpp"
#include "../Alloc/LinearAllocator.hpp"
#include "../Assert.hpp"
#include "AssetGenerator.hpp"

#include <functional>
#include <span>
#include <string>
#include <typeindex>

namespace eg
{
struct Asset
{
	std::string_view fullName;
	std::string_view name;
	std::type_index assetType;
	void* instance;
	void (*instanceDtor)(void*);
	Asset* next;

	explicit Asset(std::type_index _assetType) : assetType(_assetType), next(nullptr) {}

	void DestroyInstance()
	{
		if (instance != nullptr)
		{
			instanceDtor(instance);
			instance = nullptr;
		}
	}

	template <typename T>
	bool IsOfType() const
	{
		return assetType == std::type_index(typeid(T));
	}
};

struct EG_API AssetDirectory
{
	std::string_view name;
	Asset* firstAsset = nullptr;
	AssetDirectory* firstChildDir = nullptr;
	AssetDirectory* nextSiblingDir = nullptr;

	AssetDirectory* FindDirectory(std::string_view path, bool create, eg::LinearAllocator* allocator);
};

struct AssetToLoad;

class EG_API AssetManager
{
public:
	struct LoadArgs
	{
		std::string_view path;
		std::string_view mountPath;
		const class AssetLoaderRegistry* loaderRegistry;
		class GraphicsLoadContext* graphicsLoadContext;
		std::span<const std::string_view> enabledSideStreams;
		bool createAssetPackage;
		bool disableAssetPackageCompression;
	};

	AssetManager() = default;

	~AssetManager();

	// Attempts to load assets from path, mounting these at mountPath.
	//  Returns true if assets loaded successfully, false otherwise.
	[[nodiscard]] bool LoadAssets(const LoadArgs& loadArgs);

	[[nodiscard]] bool LoadAssetsEAP(
		std::span<const struct EAPAsset> assets, std::string_view mountPath,
		const class AssetLoaderRegistry& loaderRegistry, GraphicsLoadContext& graphicsLoadContext);

	inline std::optional<std::type_index> GetAssetType(std::string_view name)
	{
		if (const Asset* node = FindAssetImpl(name))
			return node->assetType;
		return {};
	}

	template <typename T>
	T* FindAsset(std::string_view name) const
	{
		const Asset* asset = FindAssetImpl(name);
		if (asset == nullptr || asset->assetType != std::type_index(typeid(T)))
			return nullptr;
		return reinterpret_cast<T*>(asset->instance);
	}

	template <typename T>
	T& GetAsset(std::string_view name) const
	{
		if (T* asset = FindAsset<T>(name))
			return *asset;
		EG_PANIC("Asset not found '" << name << "'");
	}

	void IterateAssets(const std::function<void(const Asset&)>& callback) const;

private:
	bool LoadAssetsYAML(const LoadArgs& loadArgs);

	LinearAllocator m_allocator;

	AssetDirectory m_rootDirectory;

	std::vector<Asset*> m_allAssets;

	const Asset* FindAssetImpl(std::string_view name) const;
};

enum class YAMLAssetStatus
{
	Cached,
	Generated,
	ErrorGenerate,
	ErrorUnknownExtension,
	ErrorLoaderNotFound,
};

struct YAMLAssetInfo
{
	YAMLAssetStatus status;
	std::string name;
	GeneratedAsset generatedAsset;
	std::string loaderName;
	const struct AssetLoader* loader;
};

EG_API std::optional<std::vector<YAMLAssetInfo>> DetectAndGenerateYAMLAssets(
	std::string_view path, const AssetLoaderRegistry& loaderRegistry);

EG_API void LoadAssetGenLibrary();
} // namespace eg
