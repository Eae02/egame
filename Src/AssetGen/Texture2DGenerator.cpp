#include "../../Inc/Common.hpp"
#include "EGame/Assets/Texture2DLoader.hpp"
#include "../EGame/Assets/AssetGenerator.hpp"
#include "../EGame/Graphics/ImageLoader.hpp"
#include "../EGame/Graphics/Format.hpp"
#include "../EGame/Log.hpp"
#include "../EGame/IOUtils.hpp"
#include "../EGame/Graphics/AbstractionHL.hpp"

#include <fstream>

namespace eg::asset_gen
{
	class Texture2DGenerator : public AssetGenerator
	{
	public:
		bool Generate(AssetGenerateContext& generateContext) override
		{
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
			
			BinWrite(generateContext.outputStream, (uint32_t)0);
			
			BinWrite(generateContext.outputStream, (uint32_t)format);
			
			BinWrite(generateContext.outputStream, (uint8_t)linearFiltering);
			BinWrite(generateContext.outputStream, (uint8_t)anistropy);
			
			std::string sourcePath = generateContext.FileDependency(generateContext.RelSourcePath());
			std::ifstream stream(sourcePath, std::ios::binary);
			if (!stream)
			{
				Log(LogLevel::Error, "as", "Error opening texture for reading: '{0}'.", sourcePath);
				return false;
			}
			
			ImageLoader loader(stream);
			
			if (mipLevels == 0)
			{
				mipLevels = Texture::MaxMipLevels((uint32_t)std::max(loader.Width(), loader.Height()));
			}
			
			BinWrite(generateContext.outputStream, (uint32_t)mipLevels);
			BinWrite(generateContext.outputStream, (uint32_t)loader.Width());
			BinWrite(generateContext.outputStream, (uint32_t)loader.Height());
			
			std::unique_ptr<uint8_t, FreeDel> data = loader.Load(numChannels);
			
			if (data == nullptr)
				return false;
			
			generateContext.outputStream.write(reinterpret_cast<char*>(data.get()),
				loader.Width() * loader.Height() * numChannels);
			
			return true;
		}
	};
	
	void RegisterTexture2DGenerator()
	{
		RegisterAssetGenerator<Texture2DGenerator>("Texture2D", Texture2DAssetFormat);
	}
}
