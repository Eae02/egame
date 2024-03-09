#pragma once

#include "../API.hpp"
#include "../Assert.hpp"
#include "../Utils.hpp"
#include "Asset.hpp"
#include "AssetGenerator.hpp"
#include "DefaultAssetGenerator.hpp"

#include <span>

namespace eg
{
class EG_API AssetLoadContext
{
public:
	AssetLoadContext(Asset* asset, std::string_view assetPath, std::span<const char> data);

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
			m_asset = Asset::Create<T>();
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

private:
	mutable Asset* m_asset = nullptr;
	std::string_view m_assetPath;
	std::string_view m_dirPath;
	std::span<const char> m_data;
};

using AssetLoaderCallback = std::function<bool(const AssetLoadContext&)>;

struct AssetLoader
{
	std::string name;
	const AssetFormat* format;
	AssetLoaderCallback callback;
};

EG_API const AssetLoader* FindAssetLoader(std::string_view loader);

EG_API Asset* LoadAsset(
	const AssetLoader& loader, std::string_view assetPath, std::span<const char> data, Asset* asset);

EG_API void RegisterAssetLoader(
	std::string name, AssetLoaderCallback loader, const AssetFormat& format = DefaultGeneratorFormat);

namespace detail
{
EG_API void RegisterAssetLoaders();
}
} // namespace eg
