#include "../../Inc/Common.hpp"
#include "Texture2DWriter.hpp"
#include "../EGame/Assets/Texture2DLoader.hpp"
#include "../EGame/Assets/AssetGenerator.hpp"
#include "../EGame/Log.hpp"

#include <fstream>
#include <stb_image_resize.h>

namespace eg::asset_gen
{
	class Texture2DArrayGenerator : public AssetGenerator
	{
	public:
		bool Generate(AssetGenerateContext& generateContext) override
		{
			Texture2DWriter textureWriter;
			textureWriter.SetIsArrayTexture(true);
			
			textureWriter.ParseYAMLSettings(generateContext.YAMLNode());
			
			for (const YAML::Node& layerNode : generateContext.YAMLNode()["layers"])
			{
				std::string layerRelPath = layerNode.as<std::string>();
				std::string layerAbsPath = generateContext.FileDependency(layerRelPath);
				
				std::ifstream layerStream(layerAbsPath, std::ios::binary);
				if (!layerStream)
				{
					Log(LogLevel::Error, "as", "Error opening texture layer file for reading: '{0}'.", layerAbsPath);
					return false;
				}
				
				if (!textureWriter.AddLayer(layerStream))
				{
					return false;
				}
			}
			
			textureWriter.Write(generateContext.outputStream);
			
			return true;
		}
	};
	
	void RegisterTexture2DArrayGenerator()
	{
		RegisterAssetGenerator<Texture2DArrayGenerator>("Texture2DArray", Texture2DAssetFormat);
	}
}
