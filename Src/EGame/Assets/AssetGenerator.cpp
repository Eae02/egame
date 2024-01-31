#include "AssetGenerator.hpp"
#include "../Log.hpp"
#include "../IOUtils.hpp"

#include <fstream>
#include <algorithm>

namespace eg
{
	struct AssetGeneratorEntry
	{
		std::string name;
		AssetFormat format;
		std::unique_ptr<AssetGenerator> generator;
	};
	
	static bool AssetGeneratorLess(const AssetGeneratorEntry& a, std::string_view b)
	{
		return a.name < b;
	}
	
	static std::vector<AssetGeneratorEntry> assetGenerators;
	
	void RegisterAssetGeneratorInstance(std::string name, const AssetFormat& format,
		std::unique_ptr<AssetGenerator> generator)
	{
		auto it = std::lower_bound(assetGenerators.begin(), assetGenerators.end(), name, &AssetGeneratorLess);
		if (it != assetGenerators.end() && it->name == name)
		{
			Log(LogLevel::Warning, "as", "Re-registering asset generator '{0}'.", name);
			it->format = format;
			it->generator = std::move(generator);
		}
		else
		{
			assetGenerators.insert(it, AssetGeneratorEntry{ std::move(name), format, std::move(generator) });
		}
	}
	
	std::optional<GeneratedAsset> GenerateAsset(
		std::string_view currentDir, std::string_view generator, std::string_view assetName,
		const YAML::Node& node, const YAML::Node& rootNode)
	{
		auto it = std::lower_bound(assetGenerators.begin(), assetGenerators.end(), generator, &AssetGeneratorLess);
		if (it == assetGenerators.end() || it->name != generator)
		{
			Log(LogLevel::Error, "as", "No generator named '{0}'", generator);
			return { };
		}
		
		AssetGenerateContext context(currentDir, assetName, node, rootNode);
		if (!it->generator->Generate(context))
			return { };
		
		GeneratedAsset generatedAsset;
		generatedAsset.data = context.outputStream.str();
		generatedAsset.fileDependencies = context.FileDependencies();
		generatedAsset.loadDependencies = context.LoadDependencies();
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
}
