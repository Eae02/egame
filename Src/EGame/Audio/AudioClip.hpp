#pragma once

#include "../API.hpp"

#include <span>
#include <memory>
#include <cstdint>

namespace eg
{
	class EG_API AudioClip
	{
	public:
		friend class AudioPlayer;
		
		AudioClip(std::span<const int16_t> data, bool isStereo, uint64_t frequency);
		~AudioClip() { Destroy(); }
		
		AudioClip(AudioClip&& other);
		AudioClip& operator=(AudioClip&& other);
		
		uint64_t NumSamples() const { return m_numSamples; }
		uint64_t Frequency() const { return m_frequency; }
		bool IsStereo() const { return m_isStereo; }
		
	private:
		void Destroy();
		
		bool m_isStereo;
		bool m_isNull;
		uint32_t m_id;
		uint64_t m_numSamples;
		uint64_t m_frequency;
	};
}
