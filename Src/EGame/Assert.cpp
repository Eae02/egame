#include "Assert.hpp"
#include "Platform/Debug.hpp"

#ifndef __EMSCRIPTEN__
#include <SDL2/SDL.h>
#endif

namespace eg
{
void (*releasePanicCallback)(const std::string& message);

void detail::PanicImpl(const std::string& message)
{
	std::cerr << message << std::endl;

#ifdef NDEBUG
#ifndef __EMSCRIPTEN__
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Runtime Error", message.c_str(), nullptr);
#endif
	if (releasePanicCallback)
		releasePanicCallback(message);
#else
	// PrintStackTraceToStdOut({});
	EG_DEBUG_BREAK;
#endif

	std::abort();
}
} // namespace eg
