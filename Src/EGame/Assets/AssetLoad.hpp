#pragma once

#include "../API.hpp"
#include "../Assert.hpp"
#include "../Utils.hpp"
#include "AssetGenerator.hpp"
#include "AssetManager.hpp"
#include "DefaultAssetGenerator.hpp"
#include "EAPFile.hpp"

#include <span>

namespace eg
{
class GraphicsLoadContext;

struct AssetLoadArgs
{
	Asset* asset;
	std::string_view assetPath;
	std::span<const char> generatedData;
	std::span<const SideStreamData> sideStreamsData;
	eg::LinearAllocator* allocator;
	GraphicsLoadContext* graphicsLoadContext;
};

class EG_API AssetLoadContext
{
public:
	explicit AssetLoadContext(const AssetLoadArgs& loadArgs);

	/**
	 * Creates the result asset. The loader must always call this function with the same type T.
	 * @tparam T The type of the result asset.
	 * @tparam A Types of the arguments to be passed to the constructor of the asset class.
	 * @param args Argument values to be passed to the constructor of the asset class.
	 * @return A reference to the allocated asset.
	 */
	template <typename T, typename... A>
	T& CreateResult(A&&... args) const
	{
		if (m_asset == nullptr)
		{
			m_asset = m_allocator->New<Asset>(std::type_index(typeid(T)));
			m_asset->instanceDtor = [](void* instance) { reinterpret_cast<T*>(instance)->~T(); };
			m_asset->instance = m_allocator->Allocate(sizeof(T), alignof(T));
		}
		else if (std::type_index(typeid(T)) != m_asset->assetType)
		{
			EG_PANIC("eg::AssetLoader::CreateResult called with different asset type upon reload.")
		}

		return *(new (m_asset->instance) T(std::forward<A>(args)...));
	}

	template <typename T>
	T* GetResult() const
	{
		if (m_asset == nullptr)
			return nullptr;
		return reinterpret_cast<T*>(m_asset->instance);
	}

	Asset* GetAsset() const { return m_asset; }

	std::span<const char> Data() const { return m_data; }

	std::string_view AssetPath() const { return m_assetPath; }

	std::string_view DirPath() const { return m_dirPath; }

	std::optional<std::span<const char>> FindSideStreamData(std::string_view streamName) const;

	GraphicsLoadContext& GetGraphicsLoadContext() const { return *m_graphicsLoadContext; }

private:
	mutable Asset* m_asset = nullptr;
	std::string_view m_assetPath;
	std::string_view m_dirPath;
	std::span<const char> m_data;
	std::span<const SideStreamData> m_sideStreamsData;
	eg::LinearAllocator* m_allocator;
	GraphicsLoadContext* m_graphicsLoadContext;
};

using AssetLoaderCallback = std::function<bool(const AssetLoadContext&)>;

struct AssetLoader
{
	std::string name;
	const AssetFormat* format;
	AssetLoaderCallback callback;
};

struct LoaderGeneratorPair
{
	std::string_view loader;
	std::string_view generator;
};

class EG_API AssetLoaderRegistry
{
public:
	AssetLoaderRegistry();

	void AddLoader(std::string name, AssetLoaderCallback loader, const AssetFormat& format = DefaultGeneratorFormat);

	const AssetLoader* FindLoader(std::string_view loader) const;

	// Sets an automatic loader and generator for a file extension (without the dot). The AssetLoaderRegistry does not
	// take ownership of the string views so these need to be valid for as long as the loader registry is used.
	void SetLoaderAndGeneratorForFileExtension(std::string_view extension, LoaderGeneratorPair loaderAndGenerator);

	std::optional<LoaderGeneratorPair> GetLoaderAndGeneratorForFileExtension(std::string_view extension) const;

private:
	std::vector<AssetLoader> m_loaders;

	std::unordered_map<std::string_view, LoaderGeneratorPair> m_assetExtensions;
};

EG_API Asset* LoadAsset(const AssetLoader& loader, const AssetLoadArgs& loadArgs);
} // namespace eg
