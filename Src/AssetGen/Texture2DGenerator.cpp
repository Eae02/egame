#include "../EGame/Assets/AssetGenerator.hpp"
#include "../EGame/Assets/Texture2DLoader.hpp"
#include "../EGame/Graphics/AbstractionHL.hpp"
#include "../EGame/Graphics/Format.hpp"
#include "../EGame/Graphics/ImageLoader.hpp"
#include "../EGame/IOUtils.hpp"
#include "../EGame/Log.hpp"
#include "Texture2DWriter.hpp"

#include <fstream>

namespace eg::asset_gen
{
class Texture2DGenerator : public AssetGenerator
{
public:
	bool Generate(AssetGenerateContext& generateContext) override
	{
		Texture2DWriter textureWriter;

		textureWriter.ParseYAMLSettings(generateContext.YAMLNode());

		std::string sourcePath = generateContext.FileDependency(generateContext.RelSourcePath());
		std::ifstream stream(sourcePath, std::ios::binary);
		if (!stream)
		{
			Log(LogLevel::Error, "as", "Error opening texture for reading: '{0}'.", sourcePath);
			return false;
		}

		if (!textureWriter.AddLayer(stream, sourcePath))
			return false;

		return textureWriter.Write(generateContext.outputStream);
	}
};

void RegisterTexture2DGenerator()
{
	RegisterAssetGenerator<Texture2DGenerator>("Texture2D", Texture2DAssetFormat);
}
} // namespace eg::asset_gen
