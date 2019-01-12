#pragma once

#include "Asset.hpp"
#include "AssetGenerator.hpp"
#include "../API.hpp"
#include "../Utils.hpp"

namespace eg
{
	class EG_API AssetLoadContext
	{
	public:
		AssetLoadContext(Asset* asset, Span<const char> data)
			: m_asset(asset), m_data(data) { }
		
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
			
			return *(new (m_asset->instance) T (std::forward<A>(args)...));
		}
		
		template <typename T>
		T* GetResult() const
		{
			if (m_asset == nullptr)
				return nullptr;
			return reinterpret_cast<T*>(m_asset->instance);
		}
		
		Asset* GetAsset() const
		{
			return m_asset;
		}
		
		Span<const char> Data() const
		{
			return m_data;
		}
		
	private:
		mutable Asset* m_asset = nullptr;
		Span<const char> m_data;
	};
	
	using AssetLoaderCallback = std::function<bool(const AssetLoadContext&)>;
	
	struct AssetLoader
	{
		std::string name;
		const AssetFormat* format;
		AssetLoaderCallback callback;
	};
	
	EG_API Asset* LoadAsset(std::string_view loader, Span<const char> data, Asset* asset);
	
	EG_API void RegisterAssetLoader(std::string name, AssetLoaderCallback loader,
		const AssetFormat& format = DefaultGeneratorFormat);
}
