#include "AudioPlayer.hpp"
#include <al.h>
#include <alc.h>
#include <atomic>

namespace eg
{
	static std::atomic_uint32_t nextParity { 1 };
	
	AudioPlayer::AudioSourceHandle::AudioSourceHandle()
	{
		alGenSources(1, &handle);
		null = false;
	}
	
	void AudioPlayer::AudioSourceHandle::Destroy()
	{
		if (!null)
		{
			alDeleteSources(1, &handle);
			null = true;
		}
	}
	
	AudioPlaybackHandle AudioPlayer::Play(const AudioClip& clip, float volume, float pitch,
	                                      const AudioLocationParameters* p, AudioPlaybackFlags flags)
	{
		uint32_t index;
		for (index = 0; index < m_sources.size(); index++)
		{
			if (m_sources[index].parity == 0) break;
			ALint state;
			alGetSourcei(m_sources[index].handle.handle, AL_SOURCE_STATE, &state);
			if (state == AL_STOPPED)
			{
				m_sources[index] = {};
				break;
			}
		}
		if (index == m_sources.size())
			m_sources.emplace_back();
		
		m_sources[index].volume   = volume;
		m_sources[index].pitch    = pitch;
		m_sources[index].relative = true;
		m_sources[index].parity   = nextParity++;
		
		alGenSources(1, &m_sources[index].handle.handle);
		alSourcei(m_sources[index].handle.handle, AL_BUFFER, clip.m_id);
		alSourcei(m_sources[index].handle.handle, AL_SOURCE_RELATIVE, 1);
		alSourcei(m_sources[index].handle.handle, AL_LOOPING, eg::HasFlag(flags, AudioPlaybackFlags::Loop));
		UpdateVolume(index);
		UpdatePitch(index);
		SetLocationParameters(index, p);
		
		if (!eg::HasFlag(flags, AudioPlaybackFlags::StartPaused))
		{
			alSourcePlay(m_sources[index].handle.handle);
		}
		
		AudioPlaybackHandle handle;
		handle.index = index;
		handle.parity = m_sources[index].parity;
		handle.handle = m_sources[index].handle.handle;
		return handle;
	}
	
	void AudioPlayer::UpdateVolume(uint32_t index) const
	{
		alSourcef(m_sources[index].handle.handle, AL_GAIN, m_globalVolume * m_sources[index].volume);
	}
	
	void AudioPlayer::UpdatePitch(uint32_t index) const
	{
		alSourcef(m_sources[index].handle.handle, AL_PITCH, m_globalPitch * m_sources[index].pitch);
	}
	
	void AudioPlayer::SetLocationParameters(uint32_t index, const AudioLocationParameters* p)
	{
		if (p == nullptr)
			return;
		if (m_sources[index].relative)
		{
			alSourcei(m_sources[index].handle.handle, AL_SOURCE_RELATIVE, 0);
			m_sources[index].relative = false;
		}
		alSource3f(m_sources[index].handle.handle, AL_POSITION, p->position.x, p->position.y, p->position.z);
		alSource3f(m_sources[index].handle.handle, AL_VELOCITY, p->velocity.x, p->velocity.y, p->velocity.z);
		alSource3f(m_sources[index].handle.handle, AL_DIRECTION, p->direction.x, p->direction.y, p->direction.z);
	}
	
	void AudioPlayer::Stop(const AudioPlaybackHandle& handle)
	{
		if (CheckHandle(handle))
		{
			m_sources[handle.index] = {};
		}
	}
	
	void AudioPlayer::Pause(const AudioPlaybackHandle& handle)
	{
		if (CheckHandle(handle)) alSourcePause(handle.handle);
	}
	
	void AudioPlayer::Resume(const AudioPlaybackHandle& handle)
	{
		if (CheckHandle(handle)) alSourcePlay(handle.handle);
	}
	
	bool AudioPlayer::IsStopped(const AudioPlaybackHandle& handle) const
	{
		if (!CheckHandle(handle))
			return true;
		ALint state;
		alGetSourcei(handle.handle, AL_SOURCE_STATE, &state);
		return state == AL_STOPPED;
	}
	
	bool AudioPlayer::IsPaused(const AudioPlaybackHandle& handle) const
	{
		if (!CheckHandle(handle))
			return false;
		ALint state;
		alGetSourcei(handle.handle, AL_SOURCE_STATE, &state);
		return state == AL_PAUSED;
	}
	
	void AudioPlayer::SetGlobalVolume(float globalVolume)
	{
		if (globalVolume == m_globalVolume)
			return;
		m_globalVolume = globalVolume;
		for (uint32_t i = 0; i < m_sources.size(); i++)
		{
			if (m_sources[i].parity != 0)
				UpdateVolume(i);
		}
	}
	
	void AudioPlayer::SetGlobalPitch(float globalPitch)
	{
		if (globalPitch == m_globalPitch)
			return;
		m_globalPitch = globalPitch;
		for (uint32_t i = 0; i < m_sources.size(); i++)
		{
			if (m_sources[i].parity != 0)
				UpdatePitch(i);
		}
	}
	
	void AudioPlayer::SetVolume(const AudioPlaybackHandle& handle, float volume)
	{
		if (CheckHandle(handle))
		{
			m_sources[handle.index].volume = volume;
			UpdateVolume(handle.index);
		}
	}
	
	void AudioPlayer::SetPitch(const AudioPlaybackHandle& handle, float pitch)
	{
		if (CheckHandle(handle))
		{
			m_sources[handle.index].pitch = pitch;
			UpdatePitch(handle.index);
		}
	}
	
	void AudioPlayer::StopAll()
	{
		m_sources.clear();
	}
	
	static ALCdevice* alDevice;
	static ALCcontext* alContext;
	
	void InitializeAudio()
	{
		alDevice = alcOpenDevice(nullptr);
		if (alDevice == nullptr)
			EG_PANIC("Error opening OpenAL device.");
		
		alContext = alcCreateContext(alDevice, nullptr);
		if (alContext == nullptr)
			EG_PANIC("Error creating OpenAL context.");
		alcMakeContextCurrent(alContext);
	}
	
	void SetListenerPosition(const AudioLocationParameters& locParameters)
	{
		alListener3f(AL_POSITION, locParameters.position.x, locParameters.position.y, locParameters.position.z);
		alListener3f(AL_VELOCITY, locParameters.velocity.x, locParameters.velocity.y, locParameters.velocity.z);
		alListener3f(AL_DIRECTION, locParameters.direction.x, locParameters.direction.y, locParameters.direction.z);
	}
	
	void SetMasterVolume(float volume)
	{
		alListenerf(AL_GAIN, volume);
	}
	
	void SetMasterPitch(float pitch)
	{
		alListenerf(AL_PITCH, pitch);
	}
}
