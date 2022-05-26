#pragma once

#ifndef EG_NO_OPENAL
#ifdef __EMSCRIPTEN__
#include <AL/al.h>
#include <AL/alc.h>
#else
#include <al.h>
#include <alc.h>
#endif

namespace eg::al
{
	extern decltype(&alcMakeContextCurrent) MakeContextCurrent;
	extern decltype(&alcOpenDevice)         OpenDevice;
	extern decltype(&alcCreateContext)      CreateContext;
	
	extern decltype(&alGenBuffers)          GenBuffers;
	extern decltype(&alDeleteBuffers)       DeleteBuffers;
	extern decltype(&alGenSources)          GenSources;
	extern decltype(&alListener3f)          Listener3f;
	extern decltype(&alSourcei)             Sourcei;
	extern decltype(&alGetSourcei)          GetSourcei;
	extern decltype(&alDeleteSources)       DeleteSources;
	extern decltype(&alSource3f)            Source3f;
	extern decltype(&alSourcePlay)          SourcePlay;
	extern decltype(&alListenerfv)          Listenerfv;
	extern decltype(&alBufferData)          BufferData;
	extern decltype(&alGenBuffers)          GenBuffers;
	extern decltype(&alSourcef)             Sourcef;
	extern decltype(&alSourcePause)         SourcePause;
	extern decltype(&alListenerf)           Listenerf;
	
	bool LoadOpenAL();
}
#else
namespace eg::al
{
	template <typename... Args> void GenBuffers(Args...) { }
	template <typename... Args> void DeleteBuffers(Args...) { }
	template <typename... Args> void GenSources(Args...) { }
	template <typename... Args> void DeleteSources(Args...) { }
	template <typename... Args> void SourcePlay(Args...) { }
	template <typename... Args> void SourcePause(Args...) { }
	
	inline bool LoadOpenAL() { return false; }
}
#endif
