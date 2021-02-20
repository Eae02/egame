#include "AudioClipAsset.hpp"
#include "AssetLoad.hpp"

namespace eg
{
	const AssetFormat AudioClipAssetFormat { "EG::AudioClip", 0 };
	
	bool AudioClipAssetLoader(const AssetLoadContext& loadContext)
	{
		const AudioClipAssetHeader& header = loadContext.Data().AtAs<const AudioClipAssetHeader>(0);
		
		Span<const int16_t> sampleData(
			reinterpret_cast<const int16_t*>(loadContext.Data().data() + sizeof(AudioClipAssetHeader)),
			header.samples
		);
		
		loadContext.CreateResult<AudioClip>(sampleData, header.channelCount == 2, header.frequency);
		return true;
	}
}
