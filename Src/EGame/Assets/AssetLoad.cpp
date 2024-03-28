#include "AssetLoad.hpp"
#include "../Graphics/GraphicsLoadContext.hpp"
#include "../Graphics/Particles/ParticleEmitterType.hpp"
#include "../Log.hpp"
#include "../Platform/FileSystem.hpp"
#include "AudioClipAsset.hpp"
#include "ModelAsset.hpp"
#include "ShaderModule.hpp"
#include "SpriteFontLoader.hpp"
#include "Texture2DLoader.hpp"

#include <algorithm>

namespace eg
{
AssetLoaderRegistry::AssetLoaderRegistry()
{
	AddLoader("Shader", &ShaderModuleAsset::AssetLoader, ShaderModuleAsset::AssetFormat);
	AddLoader("Texture2D", &Texture2DLoader, Texture2DAssetFormat);
	AddLoader("Model", &ModelAssetLoader, ModelAssetFormat);
	AddLoader("ParticleEmitter", &ParticleEmitterType::AssetLoader, ParticleEmitterType::AssetFormat);
	AddLoader("SpriteFont", &SpriteFontLoader, SpriteFontAssetFormat);
	AddLoader("AudioClip", &AudioClipAssetLoader, AudioClipAssetFormat);

	AddLoader(
		"String",
		[](const AssetLoadContext& loadContext)
		{
			loadContext.CreateResult<std::string>(loadContext.Data().data(), loadContext.Data().size());
			return true;
		});

	SetLoaderAndGeneratorForFileExtension("glsl", { .loader = "Shader", .generator = "Shader" });
	SetLoaderAndGeneratorForFileExtension("png", { .loader = "Texture2D", .generator = "Texture2D" });
	SetLoaderAndGeneratorForFileExtension("obj", { .loader = "Model", .generator = "OBJModel" });
	SetLoaderAndGeneratorForFileExtension("gltf", { .loader = "Model", .generator = "GLTFModel" });
	SetLoaderAndGeneratorForFileExtension("glb", { .loader = "Model", .generator = "GLTFModel" });
	SetLoaderAndGeneratorForFileExtension("ype", { .loader = "ParticleEmitter", .generator = "ParticleEmitter" });
	SetLoaderAndGeneratorForFileExtension("ttf", { .loader = "SpriteFont", .generator = "Font" });
	SetLoaderAndGeneratorForFileExtension("ogg", { .loader = "AudioClip", .generator = "OGGVorbis" });
}

struct AssetLoaderStringCompare
{
	bool operator()(const AssetLoader& a, std::string_view b) const { return a.name < b; }
};

void AssetLoaderRegistry::AddLoader(std::string name, AssetLoaderCallback loader, const AssetFormat& format)
{
	auto it = std::lower_bound(m_loaders.begin(), m_loaders.end(), name, AssetLoaderStringCompare());
	if (it != m_loaders.end() && it->name == name)
	{
		Log(LogLevel::Warning, "as", "Re-registering asset loader '{0}'.", name);
		it->format = &format;
		it->callback = std::move(loader);
	}
	else
	{
		m_loaders.insert(it, AssetLoader{ std::move(name), &format, std::move(loader) });
	}
}

const AssetLoader* AssetLoaderRegistry::FindLoader(std::string_view loader) const
{
	auto it = std::lower_bound(m_loaders.begin(), m_loaders.end(), loader, AssetLoaderStringCompare());
	if (it == m_loaders.end() || it->name != loader)
		return nullptr;
	return &*it;
}

void AssetLoaderRegistry::SetLoaderAndGeneratorForFileExtension(
	std::string_view extension, LoaderGeneratorPair loaderAndGenerator)
{
	auto it = m_assetExtensions.find(extension);
	if (it != m_assetExtensions.end())
	{
		it->second = loaderAndGenerator;
		Log(LogLevel::Warning, "as", "Re-binding asset extension '{0}'", extension);
	}
	else
	{
		m_assetExtensions.emplace(extension, loaderAndGenerator);
	}
}

std::optional<LoaderGeneratorPair> AssetLoaderRegistry::GetLoaderAndGeneratorForFileExtension(
	std::string_view extension) const
{
	auto it = m_assetExtensions.find(extension);
	if (it == m_assetExtensions.end())
		return std::nullopt;
	return it->second;
}

Asset* LoadAsset(const AssetLoader& loader, const AssetLoadArgs& loadArgs)
{
	AssetLoadContext context(loadArgs);
	if (!loader.callback(context))
		return nullptr;

	if (context.GetAsset() == nullptr)
	{
		Log(LogLevel::Error, "as", "Asset loader '{0}' returned true but did not call CreateResult.", loader.name);
	}

	return context.GetAsset();
}

AssetLoadContext::AssetLoadContext(const AssetLoadArgs& loadArgs)
	: m_asset(loadArgs.asset), m_assetPath(loadArgs.assetPath), m_dirPath(ParentPath(loadArgs.assetPath, true)),
	  m_data(loadArgs.generatedData), m_sideStreamsData(loadArgs.sideStreamsData), m_allocator(loadArgs.allocator),
	  m_graphicsLoadContext(loadArgs.graphicsLoadContext)
{
	if (m_graphicsLoadContext == nullptr)
		m_graphicsLoadContext = &eg::GraphicsLoadContext::Direct;

	EG_ASSERT(m_allocator != nullptr);
}

std::optional<std::span<const char>> AssetLoadContext::FindSideStreamData(std::string_view streamName) const
{
	auto it = std::find_if(
		m_sideStreamsData.begin(), m_sideStreamsData.end(),
		[&](const SideStreamData& data) { return data.streamName == streamName; });
	if (it == m_sideStreamsData.end())
		return std::nullopt;
	return it->data;
}

static std::unordered_map<std::string, bool> shouldLoadAssetSideStream;

void SetShouldLoadAssetSideStream(std::string_view sideStreamName, bool shouldLoad) {}
} // namespace eg
