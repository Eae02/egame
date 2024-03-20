#pragma once

#include "API.hpp"

#include <string>
#include <string_view>
#include <vector>

// For strcasecmp
#if defined(__linux__)
#include <strings.h>
#elif defined(_WIN32)
#include <string.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

namespace eg
{
inline bool StringEqualCaseInsensitive(std::string_view a, std::string_view b)
{
	return a.size() == b.size() && strncasecmp(a.data(), b.data(), a.size()) == 0;
}

/**
 * Concatenates a list of string views into one string object.
 * @param list The list of parts to concatenate
 * @return The combined string
 */
EG_API std::string Concat(std::initializer_list<std::string_view> list);

/**
 * Removes whitespace from the start and end of the input string.
 * @param input The string to remove whitespace from.
 * @return Input without leading and trailing whitespace.
 */
EG_API std::string_view TrimString(std::string_view input);

/**
 * Invokes a callback for each parts of a string that is separated by a given delimiter. Empty parts are skipped.
 * @tparam CallbackTp The callback type, signature should be void(std::string_view)
 * @param string The string to loop through the parts of.
 * @param delimiter The delimiter to use.
 * @param callback The callback function.
 */
template <typename CallbackTp>
void IterateStringParts(std::string_view string, char delimiter, CallbackTp callback)
{
	for (size_t pos = 0; pos < string.size(); pos++)
	{
		const size_t end = string.find(delimiter, pos);
		if (end == pos)
			continue;

		const size_t partLen = end == std::string_view::npos ? std::string_view::npos : (end - pos);
		callback(string.substr(pos, partLen));

		if (end == std::string_view::npos)
			break;
		pos = end;
	}
}

EG_API void SplitString(std::string_view string, char delimiter, std::vector<std::string_view>& partsOut);

EG_API std::pair<std::string_view, std::string_view> SplitStringOnce(std::string_view string, char delimiter);
} // namespace eg
