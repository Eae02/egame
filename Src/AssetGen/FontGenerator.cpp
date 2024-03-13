#include "../EGame/Assets/AssetGenerator.hpp"
#include "../EGame/Assets/SpriteFontLoader.hpp"
#include "../EGame/Graphics/FontAtlas.hpp"
#include "../EGame/Log.hpp"
#include "../EGame/Platform/FontConfig.hpp"

#include <fstream>
#include <yaml-cpp/yaml.h>

namespace eg::asset_gen
{
class FontGenerator : public AssetGenerator
{
public:
	bool Generate(AssetGenerateContext& generateContext) override
	{
		uint32_t size = generateContext.YAMLNode()["size"].as<uint32_t>(32);

		std::vector<GlyphRange> glyphRanges;
		glyphRanges.push_back(GlyphRange::ASCII);
		glyphRanges.push_back(GlyphRange::LatinSupplement);

		std::string sourcePath;
		if (generateContext.YAMLNode()["fontNames"].IsDefined())
		{
			for (const auto& nameNode : generateContext.YAMLNode()["fontNames"])
			{
				std::string name = nameNode.as<std::string>("");
				sourcePath = GetFontPathByName(name);
				if (sourcePath.empty())
					eg::Log(eg::LogLevel::Warning, "as", "Named font not found '{0}'.", name);
				else
					break;
			}
			if (sourcePath.empty())
				return false;
		}
		else
		{
			std::string name = generateContext.YAMLNode()["fontName"].as<std::string>("");
			if (!name.empty())
			{
				sourcePath = GetFontPathByName(name);
				if (sourcePath.empty())
				{
					eg::Log(eg::LogLevel::Error, "as", "Named font not found '{0}'.", name);
					return false;
				}
			}
			else
			{
				sourcePath = generateContext.FileDependency(generateContext.RelSourcePath());
			}
		}

		std::optional<FontAtlas> atlas = FontAtlas::Render(sourcePath, size, glyphRanges);
		if (!atlas.has_value())
			return false;

		atlas->Serialize(generateContext.writer);

		return true;
	}
};

void RegisterFontGenerator()
{
	RegisterAssetGenerator<FontGenerator>("Font", SpriteFontAssetFormat);
}
} // namespace eg::asset_gen
