#pragma once

#include "API.hpp"

#include <string>
#include <type_traits>

namespace eg
{
enum class LogLevel
{
	Info,
	Warning,
	Error,
};

struct LogMessage
{
	LogLevel level;
	std::string message;
};

namespace detail
{
struct A
{
};
struct B : A
{
};

template <typename T, class = std::enable_if_t<std::is_pointer<typename std::remove_reference<T>::type>::value>>
inline std::string ToString(T val, const B& b)
{
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "%p", reinterpret_cast<const void*>(val));
	return buffer;
}

template <typename T>
inline std::string ToString(T val, const A& a)
{
	return std::to_string(val);
}

EG_API void Log(LogLevel level, const char* category, const char* format, size_t argc, const std::string* argv);
} // namespace detail

template <typename T>
inline std::string LogToString(T val)
{
	return detail::ToString<T>(val, detail::B());
}

template <>
inline std::string LogToString(const char* val)
{
	return val;
}

template <>
inline std::string LogToString(char* val)
{
	return val;
}

template <>
inline std::string LogToString(std::string val)
{
	return val;
}

template <>
inline std::string LogToString(std::string_view val)
{
	return { val.data(), val.size() };
}

template <typename... Args>
void Log(LogLevel level, const char* category, const char* format, Args... args)
{
	if constexpr (sizeof...(Args) == 0)
	{
		detail::Log(level, category, format, 0, nullptr);
	}
	else
	{
		std::string argStrings[] = { LogToString(args)... };
		detail::Log(level, category, format, sizeof...(Args), argStrings);
	}
}
} // namespace eg
