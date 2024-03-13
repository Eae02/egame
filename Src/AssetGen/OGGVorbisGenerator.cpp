#include "../EGame/Assets/AssetGenerator.hpp"
#include "../EGame/Assets/AudioClipAsset.hpp"
#include "../EGame/IOUtils.hpp"
#include "../EGame/Log.hpp"

#include <fstream>
#include <vorbis/vorbisfile.h>
#include <yaml-cpp/yaml.h>

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
			Log(LogLevel::Error, "as", "Invalid mode parameter for OGG asset: '{0}', expected 'stereo' or 'mono'.",
			    sourcePath);
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
			Log(LogLevel::Error, "as", "Invalid number of channels for OGG asset: '{0}', got {1}.", sourcePath,
			    info->channels);
			return false;
		}

		int outputChannels = info->channels;
		if (modeString == "mono")
			outputChannels = 1;
		if (modeString == "stereo")
			outputChannels = 2;

		std::vector<int16_t> samples;
		int bitStream;
		int16_t buffer[1024];
		long bytes;
		do
		{
			bytes = ov_read(&oggFile, reinterpret_cast<char*>(buffer), sizeof(buffer), 0, 2, 1, &bitStream);

			if (info->channels == 1 && outputChannels == 2)
			{
				for (size_t i = 0; i < static_cast<size_t>(bytes) / sizeof(int16_t); i++)
				{
					samples.push_back(buffer[i]);
					samples.push_back(buffer[i]);
				}
			}
			else if (info->channels == 2 && outputChannels == 1)
			{
				for (size_t i = 0; i < static_cast<size_t>(bytes) / (2 * sizeof(int16_t)); i++)
				{
					samples.push_back(static_cast<uint16_t>(
						(static_cast<int32_t>(buffer[i * 2 + 1]) + static_cast<int32_t>(buffer[i * 2])) / 2));
				}
			}
			else
			{
				samples.insert(samples.end(), buffer, buffer + bytes / 2);
			}
		} while (bytes != 0);

		generateContext.outputFlags |= eg::AssetFlags::DisableEAPCompression;
		generateContext.writer.Write<uint32_t>(outputChannels);
		generateContext.writer.Write<uint64_t>(info->rate);
		generateContext.writer.Write<uint64_t>(samples.size());

		generateContext.writer.WriteBytes(
			{ reinterpret_cast<const char*>(samples.data()), samples.size() * sizeof(int16_t) });

		ov_clear(&oggFile);
		return true;
	}
};

void RegisterOGGVorbisGenerator()
{
	RegisterAssetGenerator<OGGVorbisGenerator>("OGGVorbis", AudioClipAssetFormat);
}
} // namespace eg::asset_gen
