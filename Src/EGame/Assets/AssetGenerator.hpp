#pragma once

#include "AssetFormat.hpp"
#include "../API.hpp"
#include "../Utils.hpp"

#include <yaml-cpp/yaml.h>
#include <sstream>

namespace eg
{
	enum class AssetFlags : uint32_t
	{
		None = 0,
		NoCache = 1,
		NoPackage = 2
	};
	
	EG_BIT_FIELD(AssetFlags)
	
	struct GeneratedAsset
	{
		std::string data;
		std::vector<std::string> dependencies;
		AssetFlags flags;
		AssetFormat format;
	};
	
	class EG_API AssetGenerateContext
	{
	public:
		AssetGenerateContext(std::string_view currentDir, const YAML::Node* node)
			: m_currentDir(currentDir), m_node(node) { }
		
		const YAML::Node& YAMLNode() const
		{
			return *m_node;
		}
		
		std::string ResolveRelPath(std::string_view relPath) const
		{
			return Concat({ m_currentDir, relPath });
		}
		
		std::string FileDependency(std::string relPath)
		{
			std::string ret = ResolveRelPath(relPath);
			m_dependencies.push_back(std::move(relPath));
			return ret;
		}
		
		std::string RelSourcePath() const;
		
		const std::vector<std::string>& Dependencies() const
		{
			return m_dependencies;
		}
		
		std::ostringstream outputStream;
		AssetFlags outputFlags = AssetFlags::None;
		
	private:
		std::vector<std::string> m_dependencies;
		std::string_view m_currentDir;
		const YAML::Node* m_node;
	};
	
	class EG_API AssetGenerator
	{
	public:
		virtual ~AssetGenerator() = default;
		
		virtual bool Generate(AssetGenerateContext& generateContext) = 0;
	};
	
	EG_API std::optional<GeneratedAsset> GenerateAsset(std::string_view currentDir, std::string_view generator,
		const YAML::Node& node);
	
	EG_API void RegisterAssetGeneratorInstance(std::string name, const AssetFormat& format,
		std::unique_ptr<AssetGenerator> generator);
	
	template <typename T, typename... A>
	void RegisterAssetGenerator(std::string name, const AssetFormat& format, A&&... args)
	{
		RegisterAssetGeneratorInstance(std::move(name), format, std::make_unique<T>(std::forward<A>(args)...));
	}
}
