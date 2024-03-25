#include "AudioClipAsset.hpp"
#include "AssetLoad.hpp"

namespace eg
{
const AssetFormat AudioClipAssetFormat{ "EG::AudioClip", 1 };

bool AudioClipAssetLoader(const AssetLoadContext& loadContext)
{
	uint32_t channelCount;
	uint32_t frequency;
	uint32_t samples;

	std::memcpy(&channelCount, loadContext.Data().data(), sizeof(uint32_t));
	std::memcpy(&frequency, loadContext.Data().data() + sizeof(uint32_t), sizeof(uint32_t));
	std::memcpy(&samples, loadContext.Data().data() + sizeof(uint32_t) * 2, sizeof(uint32_t));

	constexpr size_t DATA_OFFSET = sizeof(uint32_t) * 3;

	static_assert(DATA_OFFSET % alignof(int16_t) == 0);

	std::span<const int16_t> sampleData(
		reinterpret_cast<const int16_t*>(loadContext.Data().data() + DATA_OFFSET), samples);

	loadContext.CreateResult<AudioClip>(sampleData, channelCount == 2, frequency);
	return true;
}
} // namespace eg
