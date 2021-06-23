#pragma once

#include <al.h>
#include <alc.h>

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
