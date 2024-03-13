#pragma once

#ifdef _WIN32
#ifdef EG_BUILDING_LIB
#define EG_API [[gnu::dllexport]]
#else
#define EG_API [[gnu::dllimport]]
#endif
#else
#define EG_API [[gnu::visibility("default")]]
#endif
