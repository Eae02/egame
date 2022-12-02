#pragma once

#include "AssetFormat.hpp"
#include "YAMLUtils.hpp"
#include "../API.hpp"
#include "../String.hpp"
#include "../Utils.hpp"

#include <memory>
#include <string>
#include <vector>
#include <string_view>
#include <sstream>
#include <optional>

namespace eg
{
	enum class AssetFlags : uint32_t
	{
		None = 0,
		NeverCache = 1,
		NeverPackage = 2,
		DisableEAPCompression = 4
	};
	
	EG_BIT_FIELD(AssetFlags)
	
	struct GeneratedAsset
	{
		std::string data;
		std::vector<std::string> fileDependencies; //List of files that are referenced by this resource
		std::vector<std::string> loadDependencies; //List of resources that must be loaded before this one
		AssetFlags flags;
		AssetFormat format;
	};
	
	class EG_API AssetGenerateContext
	{
	public:
		AssetGenerateContext(std::string_view currentDir, std::string_view assetName,
		                     const YAML::Node& node, const YAML::Node& rootNode)
			: m_currentDir(currentDir), m_assetName(assetName), m_node(&node), m_rootNode(&rootNode) { }
		
		const YAML::Node& YAMLNode() const
		{
			return *m_node;
		}
		
		const YAML::Node& RootYAMLNode() const
		{
			return *m_rootNode;
		}
		
		std::string ResolveRelPath(std::string_view relPath) const
		{
			return Concat({ m_currentDir, relPath });
		}
		
		std::string FileDependency(std::string relPath)
		{
			std::string ret = ResolveRelPath(relPath);
			m_fileDependencies.push_back(std::move(relPath));
			return ret;
		}
		
		void AddLoadDependency(std::string relPath)
		{
			m_loadDependencies.push_back(std::move(relPath));
		}
		
		std::string_view AssetName() const
		{
			return m_assetName;
		}
		
		std::string RelSourcePath() const;
		
		const std::vector<std::string>& FileDependencies() const
		{
			return m_fileDependencies;
		}
		
		const std::vector<std::string>& LoadDependencies() const
		{
			return m_loadDependencies;
		}
		
		void AddSecondaryStream();
		
		std::ostringstream outputStream;
		AssetFlags outputFlags = AssetFlags::None;
		
	private:
		std::vector<std::string> m_fileDependencies;
		std::vector<std::string> m_loadDependencies;
		std::string_view m_currentDir;
		std::string_view m_assetName;
		const YAML::Node* m_node;
		const YAML::Node* m_rootNode;
	};
	
	class EG_API AssetGenerator
	{
	public:
		virtual ~AssetGenerator() = default;
		
		virtual bool Generate(AssetGenerateContext& generateContext) = 0;
	};
	
	EG_API std::optional<GeneratedAsset> GenerateAsset(
		std::string_view currentDir, std::string_view generator, std::string_view assetName,
		const YAML::Node& node, const YAML::Node& rootNode);
	
	EG_API void RegisterAssetGeneratorInstance(std::string name, const AssetFormat& format,
		std::unique_ptr<AssetGenerator> generator);
	
	template <typename T, typename... A>
	void RegisterAssetGenerator(std::string name, const AssetFormat& format, A&&... args)
	{
		RegisterAssetGeneratorInstance(std::move(name), format, std::make_unique<T>(std::forward<A>(args)...));
	}
}
