#include "AudioPlayer.hpp"
#include <al.h>
#include <alc.h>
#include <atomic>

namespace eg
{
	bool alInitialized = false;
	
	static std::atomic_uint32_t nextParity { 1 };
	
	AudioPlayer::AudioSourceHandle::AudioSourceHandle()
	{
		if (alInitialized)
			alGenSources(1, &handle);
		null = false;
	}
	
	void AudioPlayer::AudioSourceHandle::Destroy()
	{
		if (!null && alInitialized)
		{
			alDeleteSources(1, &handle);
			null = true;
		}
	}
	
	AudioPlaybackHandle AudioPlayer::Play(const AudioClip& clip, float volume, float pitch,
	                                      const AudioLocationParameters* p, AudioPlaybackFlags flags)
	{
		if (!alInitialized) { };
		
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
		m_sources[index].parity   = nextParity++;
		
		alSourcei(m_sources[index].handle.handle, AL_BUFFER, clip.m_id);
		alSourcei(m_sources[index].handle.handle, AL_LOOPING, eg::HasFlag(flags, AudioPlaybackFlags::Loop));
		UpdateVolume(index);
		UpdatePitch(index);
		
		alSourcei(m_sources[index].handle.handle, AL_SOURCE_RELATIVE, p == nullptr);
		if (p != nullptr)
			SetLocationParameters(index, *p);
		
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
		if (alInitialized)
			alSourcef(m_sources[index].handle.handle, AL_GAIN, m_globalVolume * m_sources[index].volume);
	}
	
	void AudioPlayer::UpdatePitch(uint32_t index) const
	{
		if (alInitialized)
			alSourcef(m_sources[index].handle.handle, AL_PITCH, m_globalPitch * m_sources[index].pitch);
	}
	
	void AudioPlayer::SetLocationParameters(uint32_t index, const AudioLocationParameters& p)
	{
		if (!alInitialized) return;
		alSource3f(m_sources[index].handle.handle, AL_POSITION, p.position.x, p.position.y, p.position.z);
		alSource3f(m_sources[index].handle.handle, AL_VELOCITY, p.velocity.x, p.velocity.y, p.velocity.z);
		alSource3f(m_sources[index].handle.handle, AL_DIRECTION, p.direction.x, p.direction.y, p.direction.z);
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
		if (CheckHandle(handle) && alInitialized) alSourcePause(handle.handle);
	}
	
	void AudioPlayer::Resume(const AudioPlaybackHandle& handle)
	{
		if (CheckHandle(handle) && alInitialized) alSourcePlay(handle.handle);
	}
	
	bool AudioPlayer::IsStopped(const AudioPlaybackHandle& handle) const
	{
		if (!CheckHandle(handle) || !alInitialized)
			return true;
		ALint state;
		alGetSourcei(handle.handle, AL_SOURCE_STATE, &state);
		return state == AL_STOPPED;
	}
	
	bool AudioPlayer::IsPaused(const AudioPlaybackHandle& handle) const
	{
		if (!CheckHandle(handle) || !alInitialized)
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
	
	bool InitializeAudio()
	{
		if (alInitialized)
			return true;
		
		alDevice = alcOpenDevice(nullptr);
		if (alDevice == nullptr)
			return false;
		
		alContext = alcCreateContext(alDevice, nullptr);
		if (alContext == nullptr)
			return false;
		alcMakeContextCurrent(alContext);
		
		alInitialized = true;
		return true;
	}
	
	void UpdateAudioListener(const AudioLocationParameters& locParameters, const glm::vec3& up)
	{
		if (!alInitialized) return;
		
		alListener3f(AL_POSITION, locParameters.position.x, locParameters.position.y, locParameters.position.z);
		alListener3f(AL_VELOCITY, locParameters.velocity.x, locParameters.velocity.y, locParameters.velocity.z);
		
		const float ori[6] = 
		{
			locParameters.direction.x,
			locParameters.direction.y,
			locParameters.direction.z,
			up.x,
			up.y,
			up.z,
		};
		alListenerfv(AL_ORIENTATION, ori);
	}
	
	void SetMasterVolume(float volume)
	{
		if (!alInitialized) return;
		alListenerf(AL_GAIN, volume);
	}
	
	void SetMasterPitch(float pitch)
	{
		if (!alInitialized) return;
		alListenerf(AL_PITCH, pitch);
	}
}
