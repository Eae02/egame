#include "AudioClipAsset.hpp"
#include "AssetLoad.hpp"

namespace eg
{
	const AssetFormat AudioClipAssetFormat { "EG::AudioClip", 1 };
	
	bool AudioClipAssetLoader(const AssetLoadContext& loadContext)
	{
		const AudioClipAssetHeader& header = *reinterpret_cast<const AudioClipAssetHeader*>(loadContext.Data().data());
		
		std::span<const int16_t> sampleData(
			reinterpret_cast<const int16_t*>(loadContext.Data().data() + sizeof(AudioClipAssetHeader)),
			header.samples
		);
		
		loadContext.CreateResult<AudioClip>(sampleData, header.channelCount == 2, header.frequency);
		return true;
	}
}
