#include "../EGame/Assets/AssetGenerator.hpp"
#include "../EGame/Log.hpp"
#include "../EGame/IOUtils.hpp"
#include "../EGame/Assets/AudioClipAsset.hpp"

#include <fstream>
#include <vorbis/vorbisfile.h>

namespace eg::asset_gen
{
	class OGGVorbisGenerator : public AssetGenerator
	{
	public:
		bool Generate(AssetGenerateContext& generateContext) override
		{
			std::string relSourcePath = generateContext.RelSourcePath();
			std::string sourcePath = generateContext.FileDependency(relSourcePath);
			
			std::string modeString = generateContext.YAMLNode()["mode"].as<std::string>("");
			if (modeString != "" && modeString != "mono" && modeString != "stereo")
			{
				Log(LogLevel::Error, "as", "Invalid mode parameter for OGG asset: '{0}', expected 'stereo' or 'mono'.", sourcePath);
				return false;
			}
			
			FILE* file = fopen(sourcePath.c_str(), "rb");
			if (file == nullptr)
			{
				Log(LogLevel::Error, "as", "Error opening asset file for reading: '{0}'", sourcePath);
				return false;
			}
			
			OggVorbis_File oggFile;
			ov_open(file, &oggFile, nullptr, 0);
			
			vorbis_info* info = ov_info(&oggFile, -1);
			
			if (info->channels != 1 && info->channels != 2)
			{
				Log(LogLevel::Error, "as", "Invalid number of channels for OGG asset: '{0}', got {1}.", sourcePath, info->channels);
				return false;
			}
			
			int outputChannels = info->channels;
			if (modeString == "mono") outputChannels = 1;
			if (modeString == "stereo") outputChannels = 2;
			
			std::vector<int16_t> samples;
			int bitStream;
			int16_t buffer[1024];
			long bytes;
			do
			{
				bytes = ov_read(&oggFile, reinterpret_cast<char*>(buffer), sizeof(buffer), 0, 2, 1, &bitStream);
				
				if (info->channels == 1 && outputChannels == 2)
				{
					for (size_t i = 0; i < (size_t)bytes / sizeof(int16_t); i++)
					{
						samples.push_back(buffer[i]);
						samples.push_back(buffer[i]);
					}
				}
				else if (info->channels == 2 && outputChannels == 1)
				{
					for (size_t i = 0; i < (size_t)bytes / (2 * sizeof(int16_t)); i++)
					{
						samples.push_back(((int32_t)buffer[i * 2 + 1] + (int32_t)buffer[i * 2]) / 2);
					}
				}
				else
				{
					samples.insert(samples.end(), buffer, buffer + bytes / 2);
				}
			} while (bytes != 0);
			
			AudioClipAssetHeader header;
			header.channelCount = outputChannels;
			header.frequency = info->rate;
			header.samples = samples.size();
			
			generateContext.outputFlags |= eg::AssetFlags::DisableEAPCompression;
			generateContext.outputStream.write(reinterpret_cast<const char*>(&header), sizeof(header));
			generateContext.outputStream.write(reinterpret_cast<const char*>(samples.data()), samples.size() * sizeof(int16_t));
			
			ov_clear(&oggFile);
			return true;
		}
	};
	
	void RegisterOGGVorbisGenerator()
	{
		RegisterAssetGenerator<OGGVorbisGenerator>("OGGVorbis", AudioClipAssetFormat);
	}
}
