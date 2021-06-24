#include "AssetLoad.hpp"
#include "ShaderModule.hpp"
#include "Texture2DLoader.hpp"
#include "ModelAsset.hpp"
#include "SpriteFontLoader.hpp"
#include "AudioClipAsset.hpp"
#include "../Graphics/StdVertex.hpp"
#include "../Graphics/Particles/ParticleEmitterType.hpp"
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
	
	const AssetLoader* FindAssetLoader(std::string_view loader)
	{
		auto it = std::lower_bound(assetLoaders.begin(), assetLoaders.end(), loader, &AssetLoaderLess);
		if (it == assetLoaders.end() || it->name != loader)
			return nullptr;
		return &*it;
	}
	
	Asset* LoadAsset(const AssetLoader& loader, std::string_view dirPath, std::span<const char> data, Asset* asset)
	{
		AssetLoadContext context(asset, dirPath, data);
		if (!loader.callback(context))
			return nullptr;
		
		if (context.GetAsset() == nullptr)
		{
			Log(LogLevel::Error, "as", "Asset loader '{0}' returned true but did not call CreateResult.", loader.name);
		}
		
		return context.GetAsset();
	}
	
	void RegisterAssetLoaders()
	{
		RegisterAssetLoader("Shader", &ShaderModuleAsset::AssetLoader, ShaderModuleAsset::AssetFormat);
		RegisterAssetLoader("Texture2D", &Texture2DLoader, Texture2DAssetFormat);
		RegisterAssetLoader("Model", &ModelAssetLoader, ModelAssetFormat);
		RegisterAssetLoader("ParticleEmitter", &ParticleEmitterType::AssetLoader, ParticleEmitterType::AssetFormat);
		RegisterAssetLoader("SpriteFont", &SpriteFontLoader, SpriteFontAssetFormat);
		RegisterAssetLoader("AudioClip", &AudioClipAssetLoader, AudioClipAssetFormat);
		
		DefineModelVertexType<StdVertex>();
		
		BindAssetExtension("glsl", "Shader", "Shader");
		BindAssetExtension("png", "Texture2D", "Texture2D");
		BindAssetExtension("obj", "Model", "OBJModel");
		BindAssetExtension("gltf", "Model", "GLTFModel");
		BindAssetExtension("glb", "Model", "GLTFModel");
		BindAssetExtension("ype", "ParticleEmitter", "ParticleEmitter");
		BindAssetExtension("ttf", "SpriteFont", "Font");
		BindAssetExtension("ogg", "AudioClip", "OGGVorbis");
	}
}
