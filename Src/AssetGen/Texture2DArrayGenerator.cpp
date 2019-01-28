#include "../../Inc/Common.hpp"
#include "EGame/Assets/Texture2DLoader.hpp"
#include "../EGame/Assets/AssetGenerator.hpp"
#include "../EGame/Graphics/ImageLoader.hpp"
#include "../EGame/Graphics/Format.hpp"
#include "../EGame/Log.hpp"
#include "../EGame/IOUtils.hpp"
#include "../EGame/Graphics/AbstractionHL.hpp"

#include <fstream>
#include <stb_image_resize.h>

namespace eg::asset_gen
{
	class Texture2DArrayGenerator : public AssetGenerator
	{
	public:
		bool Generate(AssetGenerateContext& generateContext) override
		{
			int width = generateContext.YAMLNode()["width"].as<int>(-1);
			int height = generateContext.YAMLNode()["height"].as<int>(-1);
			
			//Reads the image format
			Format format = Format::R8G8B8A8_UNorm;
			int numChannels = 4;
			if (const YAML::Node& formatNode = generateContext.YAMLNode()["format"])
			{
				std::string formatName = formatNode.as<std::string>();
				if (StringEqualCaseInsensitive(formatName, "r8"))
				{
					format = Format::R8_UNorm;
					numChannels = 1;
				}
				else if (StringEqualCaseInsensitive(formatName, "rgba8"))
				{
					format = Format::R8G8B8A8_UNorm;
					numChannels = 4;
				}
				else
				{
					Log(LogLevel::Error, "as", "Unknown texture format: {0}.", formatName);
				}
			}
			
			bool sRGB = false;
			if (generateContext.YAMLNode()["srgb"].as<bool>(false))
			{
				switch (format)
				{
				case Format::R8G8B8A8_UNorm:
					format = Format::R8G8B8A8_sRGB;
					sRGB = true;
					break;
				default:
					Log(LogLevel::Error, "as", "sRGB is not supported for the selected format.");
					break;
				}
			}
			
			int mipLevels = generateContext.YAMLNode()["mipLevels"].as<int>(0);
			
			bool linearFiltering = generateContext.YAMLNode()["filtering"].as<std::string>("linear") == "linear";
			bool anistropy = generateContext.YAMLNode()["enableAnistropy"].as<bool>(true);
			
			const YAML::Node& layersNode = generateContext.YAMLNode()["layers"];
			if (!layersNode.IsDefined())
			{
				Log(LogLevel::Error, "as", "Missing node 'layers'");
				return false;
			}
			
			BinWrite(generateContext.outputStream, (uint32_t)layersNode.size());
			size_t layerBytes;
			
			BinWrite(generateContext.outputStream, (uint32_t)format);
			
			BinWrite(generateContext.outputStream, (uint8_t)linearFiltering);
			BinWrite(generateContext.outputStream, (uint8_t)anistropy);
			
			bool hasWrittenSize = false;
			for (const YAML::Node& layerNode : layersNode)
			{
				std::string layerRelPath = layerNode.as<std::string>();
				std::string layerAbsPath = generateContext.FileDependency(layerRelPath);
				
				std::ifstream layerStream(layerAbsPath, std::ios::binary);
				if (!layerStream)
				{
					Log(LogLevel::Error, "as", "Error opening texture layer file for reading: '{0}'.", layerAbsPath);
					return false;
				}
				
				ImageLoader loader(layerStream);
				
				if (width == -1)
					width = loader.Width();
				if (height == -1)
					height = loader.Height();
				
				if (!hasWrittenSize)
				{
					if (mipLevels == 0)
					{
						mipLevels = Texture::MaxMipLevels((uint32_t)std::max(width, height));
					}
					
					layerBytes = width * height * numChannels;
					
					BinWrite(generateContext.outputStream, (uint32_t)mipLevels);
					BinWrite(generateContext.outputStream, (uint32_t)width);
					BinWrite(generateContext.outputStream, (uint32_t)height);
					hasWrittenSize = true;
				}
				
				std::unique_ptr<uint8_t, FreeDel> data = loader.Load(numChannels);
				
				if (data == nullptr)
					return false;
				
				if (width != loader.Width() || height != loader.Height())
				{
					Log(LogLevel::Warning, "as", "Inconsistent texture array resolution, "
						"layer '{0}' will be resized to {1}x{2}.", layerRelPath, width, height);
					
					uint8_t* newData = reinterpret_cast<uint8_t*>(std::malloc(layerBytes));
					stbir_resize_uint8(data.get(), loader.Width(), loader.Height(), 0, newData, width, height, 0, numChannels);
					data.reset(newData);
				}
				
				generateContext.outputStream.write(reinterpret_cast<char*>(data.get()), layerBytes);
			}
			
			return true;
		}
	};
	
	void RegisterTexture2DArrayGenerator()
	{
		RegisterAssetGenerator<Texture2DArrayGenerator>("Texture2DArray", Texture2DAssetFormat);
	}
}
