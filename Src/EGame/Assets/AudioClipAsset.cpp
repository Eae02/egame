#include "AudioClipAsset.hpp"
#include "AssetLoad.hpp"

namespace eg
{
	const AssetFormat AudioClipAssetFormat { "EG::AudioClip", 1 };
	
	bool AudioClipAssetLoader(const AssetLoadContext& loadContext)
	{
		uint32_t channelCount;
		uint64_t frequency;
		uint64_t samples;
		
		std::memcpy(&channelCount, loadContext.Data().data(), sizeof(uint32_t));
		std::memcpy(&frequency, loadContext.Data().data() + sizeof(uint32_t), sizeof(uint64_t));
		std::memcpy(&samples, loadContext.Data().data() + sizeof(uint32_t) + sizeof(uint64_t), sizeof(uint64_t));
		
		std::span<const int16_t> sampleData(
			reinterpret_cast<const int16_t*>(loadContext.Data().data() + sizeof(uint32_t) + sizeof(uint64_t) * 2),
			samples
		);
		
		loadContext.CreateResult<AudioClip>(sampleData, channelCount == 2, frequency);
		return true;
	}
}
