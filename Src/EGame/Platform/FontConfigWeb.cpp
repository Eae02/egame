#ifdef __EMSCRIPTEN__

#include "FontConfig.hpp"

namespace eg
{
	void InitPlatformFontConfig() { }
	void DestroyPlatformFontConfig() { }
	std::string GetFontPathByName(const char* name) { return { }; }
}

#endif