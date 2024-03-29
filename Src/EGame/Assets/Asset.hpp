#pragma once

#include <atomic>
#include <functional>
#include <istream>
#include <optional>
#include <thread>
#include <typeindex>

#include "../API.hpp"
#include "../Alloc/LinearAllocator.hpp"
#include "../Assert.hpp"

namespace eg
{
namespace detail
{
extern EG_API LinearAllocator assetAllocator;

extern bool createAssetPackage;
extern bool disableAssetPackageCompression;

void LoadAssetGenLibrary();
} // namespace detail

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
	static Asset* Create()
	{
		Asset* asset = detail::assetAllocator.New<Asset>(std::type_index(typeid(T)));
		asset->instanceDtor = [](void* instance) { reinterpret_cast<T*>(instance)->~T(); };
		asset->instance = detail::assetAllocator.Allocate(sizeof(T), alignof(T));
		return asset;
	}
};

EG_API void BindAssetExtension(std::string_view extension, std::string_view loader, std::string_view generator);

inline void BindAssetExtension(std::string_view extension, std::string_view loader)
{
	BindAssetExtension(extension, loader, "Default");
}

// Attempts to load assets from path, mounting these at mountPath.
//  Returns true if assets loaded successfully, false otherwise.
[[nodiscard]] EG_API bool LoadAssets(const std::string& path, std::string_view mountPath);

// Similar to LoadAssets, but loads from an EAP stream
[[nodiscard]] EG_API bool LoadAssetsFromEAPStream(std::istream& stream, std::string_view mountPath);

EG_API void UnloadAssets();

namespace console
{
struct CompletionsList;
}

namespace detail
{
EG_API const Asset* FindAsset(std::string_view name);
}

inline std::optional<std::type_index> GetAssetType(std::string_view name)
{
	if (const Asset* node = detail::FindAsset(name))
		return node->assetType;
	return {};
}

template <typename T>
T* FindAsset(std::string_view name)
{
	const Asset* asset = detail::FindAsset(name);
	if (asset == nullptr || asset->assetType != std::type_index(typeid(T)))
		return nullptr;
	return reinterpret_cast<T*>(asset->instance);
}

template <typename T>
T& GetAsset(std::string_view name)
{
	if (T* asset = FindAsset<T>(name))
		return *asset;
	EG_PANIC("Asset not found '" << name << "'");
}

EG_API void IterateAssets(const std::function<void(const Asset&)>& callback);

EG_API void AssetCommandCompletionProvider(console::CompletionsList& list, const std::type_index* assetType = nullptr);
} // namespace eg
