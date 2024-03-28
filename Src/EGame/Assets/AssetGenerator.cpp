#include "AssetGenerator.hpp"
#include "../IOUtils.hpp"
#include "../Log.hpp"

#include <algorithm>
#include <yaml-cpp/yaml.h>

namespace eg
{
struct AssetGeneratorEntry
{
	std::string name;
	AssetFormat format;
	AssetGenerator* generator;
};

static bool AssetGeneratorLess(const AssetGeneratorEntry& a, std::string_view b)
{
	return a.name < b;
}

static std::vector<AssetGeneratorEntry> assetGenerators;

void RegisterAssetGeneratorInstance(std::string name, const AssetFormat& format, AssetGenerator* generator)
{
	auto it = std::lower_bound(assetGenerators.begin(), assetGenerators.end(), name, &AssetGeneratorLess);
	if (it != assetGenerators.end() && it->name == name)
	{
		Log(LogLevel::Warning, "as", "Re-registering asset generator '{0}'.", name);
		it->format = format;
		delete it->generator;
		it->generator = generator;
	}
	else
	{
		assetGenerators.insert(it, AssetGeneratorEntry{ std::move(name), format, generator });
	}
}

std::optional<GeneratedAsset> GenerateAsset(
	std::string_view currentDir, std::string_view generator, std::string_view assetName, const YAML::Node& node,
	const YAML::Node& rootNode)
{
	auto it = std::lower_bound(assetGenerators.begin(), assetGenerators.end(), generator, &AssetGeneratorLess);
	if (it == assetGenerators.end() || it->name != generator)
	{
		Log(LogLevel::Error, "as", "No generator named '{0}'", generator);
		return {};
	}

	AssetGenerateContext context(currentDir, assetName, node, rootNode);
	if (!it->generator->Generate(context))
		return {};

	GeneratedAsset generatedAsset;
	generatedAsset.data = context.writer.ToVector();
	generatedAsset.fileDependencies = context.FileDependencies();
	generatedAsset.loadDependencies = context.LoadDependencies();
	generatedAsset.sideStreamsData = context.SideStreamsData();
	generatedAsset.flags = context.outputFlags;
	generatedAsset.format = it->format;
	return generatedAsset;
}

std::string AssetGenerateContext::RelSourcePath() const
{
	if (const YAML::Node& sourceNode = YAMLNode()["source"])
	{
		return sourceNode.as<std::string>();
	}

	return std::string(m_assetName);
}

void AssetGenerateContext::SetSideStreamData(std::string_view sideStreamName, std::vector<char> data)
{
	for (GeneratedAssetSideStreamData& sideStreamData : m_sideStreamsData)
	{
		if (sideStreamData.streamName == sideStreamName)
		{
			sideStreamData.data = std::move(data);
			return;
		}
	}

	m_sideStreamsData.push_back(GeneratedAssetSideStreamData{
		.streamName = std::string(sideStreamName),
		.data = std::move(data),
	});
}
} // namespace eg
