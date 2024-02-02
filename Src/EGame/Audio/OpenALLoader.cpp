#ifndef EG_NO_OPENAL
#include "OpenALLoader.hpp"
#include "../Platform/DynamicLibrary.hpp"
#include "../Log.hpp"

namespace eg::al
{
	decltype(&alcMakeContextCurrent) MakeContextCurrent;
	decltype(&alcOpenDevice)         OpenDevice;
	decltype(&alcCreateContext)      CreateContext;
	
	decltype(&alGenBuffers)          GenBuffers;
	decltype(&alDeleteBuffers)       DeleteBuffers;
	decltype(&alGenSources)          GenSources;
	decltype(&alListener3f)          Listener3f;
	decltype(&alSourcei)             Sourcei;
	decltype(&alGetSourcei)          GetSourcei;
	decltype(&alDeleteSources)       DeleteSources;
	decltype(&alSource3f)            Source3f;
	decltype(&alSourcePlay)          SourcePlay;
	decltype(&alListenerfv)          Listenerfv;
	decltype(&alBufferData)          BufferData;
	decltype(&alSourcef)             Sourcef;
	decltype(&alSourcePause)         SourcePause;
	decltype(&alListenerf)           Listenerf;
	
#if defined(__EMSCRIPTEN__) || defined(__APPLE__)
	static bool StartLoadOpenAL() { return true; }
	
	#define LOAD_AL_FUNC(prefix, name) eg::al::name = &::prefix ## name;
#else
	
	static DynamicLibrary openalLibrary;
	
#if defined(__linux__)
	static const char* OPENAL_LIB_NAME = "libopenal.so";
#elif defined(_WIN32)
	static const char* OPENAL_LIB_NAME = "OpenAL32.dll";
#else
	static const char* OPENAL_LIB_NAME = nullptr;
#endif
	
	#define LOAD_AL_FUNC(prefix, name) \
		eg::al::name = reinterpret_cast<decltype(eg::al::name)>(openalLibrary.GetSymbol(#prefix #name)); \
		if (eg::al::name == nullptr) return false;
	
	static bool StartLoadOpenAL()
	{
		if (OPENAL_LIB_NAME == nullptr)
			return false;
		
		if (!openalLibrary.Open(OPENAL_LIB_NAME))
		{
			Log(LogLevel::Error, "al", "Failed to load OpenAL library, {0} not found", OPENAL_LIB_NAME);
			return false;
		}
		
		return true;
	}
#endif
	
	bool LoadOpenAL()
	{
		if (!StartLoadOpenAL()) return false;
		
		LOAD_AL_FUNC(alc, MakeContextCurrent)
		LOAD_AL_FUNC(alc, OpenDevice)
		LOAD_AL_FUNC(alc, CreateContext)
		LOAD_AL_FUNC(al, GenBuffers)
		LOAD_AL_FUNC(al, DeleteBuffers)
		LOAD_AL_FUNC(al, GenSources)
		LOAD_AL_FUNC(al, Listener3f)
		LOAD_AL_FUNC(al, Sourcei)
		LOAD_AL_FUNC(al, GetSourcei)
		LOAD_AL_FUNC(al, DeleteSources)
		LOAD_AL_FUNC(al, Source3f)
		LOAD_AL_FUNC(al, SourcePlay)
		LOAD_AL_FUNC(al, Listenerfv)
		LOAD_AL_FUNC(al, BufferData)
		LOAD_AL_FUNC(al, Sourcef)
		LOAD_AL_FUNC(al, SourcePause)
		LOAD_AL_FUNC(al, Listenerf)
		
		return true;
	}
}
#endif
