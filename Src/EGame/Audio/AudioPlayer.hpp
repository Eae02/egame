#pragma once

#include "AudioClip.hpp"

#include <glm/vec3.hpp>

namespace eg
{
	struct AudioLocationParameters
	{
		glm::vec3 position;
		glm::vec3 direction;
		glm::vec3 velocity;
	};
	
	class AudioPlaybackHandle
	{
		friend class AudioPlayer;
		uint32_t index;
		uint32_t parity;
		uint32_t handle;
	};
	
	enum class AudioPlaybackFlags
	{
		StartPaused = 0x1,
		Loop = 0x2,
	};
	EG_BIT_FIELD(AudioPlaybackFlags)
	
	class EG_API AudioPlayer
	{
	public:
		AudioPlayer() = default;
		
		AudioPlaybackHandle Play(const AudioClip& clip, float volume, float pitch,
			const AudioLocationParameters* locParameters, AudioPlaybackFlags flags = {});
		
		void Stop(const AudioPlaybackHandle& handle);
		void Pause(const AudioPlaybackHandle& handle);
		void Resume(const AudioPlaybackHandle& handle);
		
		bool IsStopped(const AudioPlaybackHandle& handle) const;
		bool IsPaused(const AudioPlaybackHandle& handle) const;
		
		void SetVolume(const AudioPlaybackHandle& handle, float volume);
		void SetPitch(const AudioPlaybackHandle& handle, float pitch);
		
		void SetPlaybackLocation(const AudioPlaybackHandle& handle, const AudioLocationParameters& locParameters)
		{
			if (CheckHandle(handle))
				SetLocationParameters(handle.index, locParameters);
		}
		
		void StopAll();
		
		void SetGlobalVolume(float globalVolume);
		float GlobalVolume() const { return m_globalVolume; }
		
		void SetGlobalPitch(float globalPitch);
		float GlobalPitch() const { return m_globalPitch; }
		
	private:
		bool CheckHandle(const AudioPlaybackHandle& handle) const
		{
			return handle.index < m_sources.size() && m_sources[handle.index].parity == handle.parity;
		}
		
		void UpdateVolume(uint32_t index) const;
		void UpdatePitch(uint32_t index) const;
		
		void SetLocationParameters(uint32_t index, const AudioLocationParameters& locParameters);
		
		struct EG_API AudioSourceHandle
		{
			bool null;
			uint32_t handle;
			
			AudioSourceHandle();
			~AudioSourceHandle() { Destroy(); }
			AudioSourceHandle(AudioSourceHandle&& other)
				: null(other.null), handle(other.handle) { other.null = true; }
			AudioSourceHandle& operator=(AudioSourceHandle&& other)
			{
				Destroy();
				handle = other.handle;
				null = other.null;
				other.null = true;
				return *this;
			}
			
			AudioSourceHandle(const AudioSourceHandle& other) = delete;
			AudioSourceHandle& operator=(const AudioSourceHandle& other) = delete;
			
			void Destroy();
		};
		
		struct SourceEntry
		{
			AudioSourceHandle handle;
			float volume    = 1;
			float pitch     = 1;
			uint32_t parity = 0;
		};
		
		std::vector<SourceEntry> m_sources;
		
		float m_globalPitch  = 1;
		float m_globalVolume = 1;
	};
	
	EG_API bool InitializeAudio();
	
	EG_API void SetMasterVolume(float volume);
	EG_API void SetMasterPitch(float pitch);
	
	EG_API void UpdateAudioListener(const AudioLocationParameters& locParameters, const glm::vec3& up);
}
