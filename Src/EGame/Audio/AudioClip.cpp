#include "AudioClip.hpp"
#include "OpenALLoader.hpp"

namespace eg
{
	extern bool alInitialized;
	
	AudioClip::AudioClip(std::span<const int16_t> data, bool isStereo, uint64_t frequency)
		: m_isNull(false), m_numSamples(data.size() / ((int)isStereo + 1)), m_frequency(frequency)
	{
		if (alInitialized)
		{
			al::GenBuffers(1, &m_id);
			al::BufferData(m_id, isStereo ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16, data.data(), data.size_bytes(), frequency);
		}
	}
	
	AudioClip::AudioClip(AudioClip&& other)
		: m_isNull(other.m_isNull), m_id(other.m_id), m_numSamples(other.m_numSamples), m_frequency(other.m_frequency)
	{
		other.m_isNull = true;
	}
	
	AudioClip& AudioClip::operator=(AudioClip&& other)
	{
		Destroy();
		m_isNull       = other.m_isNull;
		m_id           = other.m_id;
		m_numSamples   = other.m_numSamples;
		m_frequency    = other.m_frequency;
		other.m_isNull = true;
		return *this;
	}
	
	void AudioClip::Destroy()
	{
		if (!m_isNull && alInitialized)
		{
			al::DeleteBuffers(1, &m_id);
			m_isNull = true;
		}
	}
}
