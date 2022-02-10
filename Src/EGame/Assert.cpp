#include "Assert.hpp"

#ifndef __EMSCRIPTEN__
#include <SDL2/SDL.h>
#endif

namespace eg
{
	void (*releasePanicCallback)(const std::string& message);
	
	void ReleasePanic(const std::string& message)
	{
		std::cerr << message << std::endl;
		
#ifndef __EMSCRIPTEN__
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Runtime Error", message.c_str(), nullptr);
#endif
		
		if (releasePanicCallback)
			releasePanicCallback(message);
		
		std::abort();
	}
}
