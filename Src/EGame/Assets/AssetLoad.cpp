#include "AssetLoad.hpp"
#include "ShaderModule.hpp"
#include "Texture2DLoader.hpp"
#include "ModelAsset.hpp"
#include "../Graphics/StdVertex.hpp"
#include "../Log.hpp"

namespace eg
{
	static std::vector<AssetLoader> assetLoaders;
	
	static bool AssetLoaderLess(const AssetLoader& a, std::string_view b)
	{
		return a.name < b;
	}
	
	void RegisterAssetLoader(std::string name, AssetLoaderCallback loader, const AssetFormat& format)
	{
		auto it = std::lower_bound(assetLoaders.begin(), assetLoaders.end(), name, &AssetLoaderLess);
		if (it != assetLoaders.end() && it->name == name)
		{
			Log(LogLevel::Warning, "as", "Re-registering asset loader '{0}'.", name);
			it->format = &format;
			it->callback = std::move(loader);
		}
		else
		{
			assetLoaders.insert(it, AssetLoader{std::move(name), &format, std::move(loader)});
		}
	}
	
	Asset* LoadAsset(std::string_view loader, Span<const char> data, Asset* asset)
	{
		auto it = std::lower_bound(assetLoaders.begin(), assetLoaders.end(), loader, &AssetLoaderLess);
		if (it == assetLoaders.end() || it->name != loader)
		{
			Log(LogLevel::Error, "as", "Asset loader not found: '{0}'.", loader);
			return nullptr;
		}
		
		AssetLoadContext context(asset, data);
		if (!it->callback(context))
			return nullptr;
		
		if (context.GetAsset() == nullptr)
		{
			Log(LogLevel::Error, "as", "Asset loader '{0}' returned true but did not call CreateResult.", loader);
		}
		
		return context.GetAsset();
	}
	
	void RegisterAssetLoaders()
	{
		RegisterAssetLoader("Shader", &ShaderModule::AssetLoader, ShaderModule::AssetFormat);
		RegisterAssetLoader("Texture2D", &Texture2DLoader, Texture2DAssetFormat);
		RegisterAssetLoader("Model", &ModelAssetLoader, ModelAssetFormat);
		
		DefineModelVertexType<StdVertex>();
		
		BindAssetExtension("glsl", "Shader", "Shader");
		BindAssetExtension("png", "Texture2D", "Texture2D");
		BindAssetExtension("obj", "Model", "OBJModel");
	}
}
