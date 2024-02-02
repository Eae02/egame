#pragma once

#ifdef _WIN32
#define EG_C_EXPORT extern "C" __declspec(dllexport)

#ifdef EG_BUILDING_LIB
#define EG_API __declspec(dllexport)
#else
#define EG_API __declspec(dllimport)
#endif
#else
#define EG_API __attribute__((visibility("default")))
#define EG_C_EXPORT extern "C" EG_API
#endif
