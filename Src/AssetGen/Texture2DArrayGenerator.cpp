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
			
			bool isCubeMap = generateContext.YAMLNode()["cubeMap"].as<bool>(false);
			textureWriter.SetIsCubeMap(isCubeMap);
			
			if (!isCubeMap)
			{
				textureWriter.SetIs3D(generateContext.YAMLNode()["3d"].as<bool>(false));
			}
			
			std::vector<std::string> layerNames;
			
			if (isCubeMap && generateContext.YAMLNode()["faces"].IsDefined())
			{
				const char* faceNames[] = { "+x", "-x", "+y", "-y", "+z", "-z" };
				for (const char* faceName : faceNames)
				{
					std::string path = generateContext.YAMLNode()["faces"][faceName].as<std::string>();
					if (path.empty())
					{
						eg::Log(eg::LogLevel::Error, "as", "Empty or not specified cube map face '{0}'.", faceName);
						return false;
					}
					layerNames.push_back(std::move(path));
				}
			}
			else
			{
				for (const YAML::Node& layerNode : generateContext.YAMLNode()["layers"])
				{
					layerNames.push_back(layerNode.as<std::string>());
				}
			}
			
			for (const std::string& layerName : layerNames)
			{
				std::string layerAbsPath = generateContext.FileDependency(layerName);
				
				std::ifstream layerStream(layerAbsPath, std::ios::binary);
				if (!layerStream)
				{
					Log(LogLevel::Error, "as", "Error opening texture layer file for reading: '{0}'.",
					    layerAbsPath);
					return false;
				}
				
				if (!textureWriter.AddLayer(layerStream))
				{
					return false;
				}
			}
			
			return textureWriter.Write(generateContext.outputStream);
		}
	};
	
	void RegisterTexture2DArrayGenerator()
	{
		RegisterAssetGenerator<Texture2DArrayGenerator>("Texture2DArray", Texture2DAssetFormat);
	}
}
