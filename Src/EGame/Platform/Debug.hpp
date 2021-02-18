#pragma once

#include "../API.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace eg
{
	EG_API std::vector<std::string> GetStackTrace();
	
	EG_API void PrintStackTraceToStdOut(std::string_view message);
}
