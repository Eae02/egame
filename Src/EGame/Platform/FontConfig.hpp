#pragma once

#include "../API.hpp"

#include <string>

namespace eg
{
void InitPlatformFontConfig();
void DestroyPlatformFontConfig();

EG_API std::string GetFontPathByName(const char* name);

inline std::string GetFontPathByName(const std::string& name)
{
	return GetFontPathByName(name.c_str());
}
} // namespace eg
