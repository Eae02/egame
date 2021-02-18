#include "../EGame/Assets/SpriteFontLoader.hpp"
#include "../EGame/Assets/AssetGenerator.hpp"
#include "../EGame/Graphics/FontAtlas.hpp"
#include "../EGame/Graphics/Format.hpp"
#include "../EGame/Log.hpp"
#include "../EGame/IOUtils.hpp"
#include "../EGame/Graphics/AbstractionHL.hpp"

#include <fstream>

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
			
			std::string sourcePath = generateContext.FileDependency(generateContext.RelSourcePath());
			
			std::optional<FontAtlas> atlas = FontAtlas::Render(sourcePath, size, glyphRanges);
			if (!atlas.has_value())
				return false;
			
			atlas->Serialize(generateContext.outputStream);
			
			return true;
		}
	};
	
	void RegisterFontGenerator()
	{
		RegisterAssetGenerator<FontGenerator>("Font", SpriteFontAssetFormat);
	}
}
