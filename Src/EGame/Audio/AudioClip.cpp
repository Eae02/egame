#include "AudioClip.hpp"
#include <al.h>

namespace eg
{
	AudioClip::AudioClip(Span<const int16_t> data, bool isStereo, uint64_t frequency)
		: m_isNull(false), m_numSamples(data.size() / ((int)isStereo + 1)), m_frequency(frequency)
	{
		alGenBuffers(1, &m_id);
		alBufferData(m_id, isStereo ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16, data.data(), data.SizeBytes(), frequency);
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
		if (!m_isNull)
		{
			alDeleteBuffers(1, &m_id);
			m_isNull = true;
		}
	}
}
