#include "AssetLoad.hpp"
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
		assetLoaders.insert(it, AssetLoader{ std::move(name), &format, std::move(loader) });
	}
}

const AssetLoader* FindAssetLoader(std::string_view loader)
{
	auto it = std::lower_bound(assetLoaders.begin(), assetLoaders.end(), loader, &AssetLoaderLess);
	if (it == assetLoaders.end() || it->name != loader)
		return nullptr;
	return &*it;
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
	  m_data(loadArgs.generatedData), m_sideStreamsData(loadArgs.sideStreamsData)
{
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

void detail::RegisterAssetLoaders()
{
	RegisterAssetLoader("Shader", &ShaderModuleAsset::AssetLoader, ShaderModuleAsset::AssetFormat);
	RegisterAssetLoader("Texture2D", &Texture2DLoader, Texture2DAssetFormat);
	RegisterAssetLoader("Model", &ModelAssetLoader, ModelAssetFormat);
	RegisterAssetLoader("ParticleEmitter", &ParticleEmitterType::AssetLoader, ParticleEmitterType::AssetFormat);
	RegisterAssetLoader("SpriteFont", &SpriteFontLoader, SpriteFontAssetFormat);
	RegisterAssetLoader("AudioClip", &AudioClipAssetLoader, AudioClipAssetFormat);

	RegisterAssetLoader(
		"String",
		[](const AssetLoadContext& loadContext)
		{
			loadContext.CreateResult<std::string>(loadContext.Data().data(), loadContext.Data().size());
			return true;
		});
}

static std::unordered_map<std::string, bool> shouldLoadAssetSideStream;

void SetShouldLoadAssetSideStream(std::string_view sideStreamName, bool shouldLoad) {}
} // namespace eg
