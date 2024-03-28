#pragma once

#include "API.hpp"

#include <iostream>
#include <string>

#if defined(NDEBUG)
#define EG_DEBUG_BREAK
#elif defined(_MSC_VER)
#define EG_DEBUG_BREAK __debugbreak();
#elif defined(__linux__)
#include <signal.h>
#define EG_DEBUG_BREAK raise(SIGTRAP);
#else
#define EG_DEBUG_BREAK
#endif

#ifndef NDEBUG
#define EG_UNREACHABLE std::abort();
#elif defined(__GNUC__)
#define EG_UNREACHABLE __builtin_unreachable();
#else
#define EG_UNREACHABLE
#endif

#define EG_MACRO_ITOS(X) _EG_MACRO_ITOS(X)
#define _EG_MACRO_ITOS(X) #X

#include <sstream>

#define EG_ASSERT(condition)                                                                                           \
	if (!(condition))                                                                                                  \
	{                                                                                                                  \
		::eg::detail::PanicImpl("EG_ASSERT " __FILE__ ":" EG_MACRO_ITOS(__LINE__) " " #condition);                     \
	}
#define EG_PANIC(msg)                                                                                                  \
	{                                                                                                                  \
		std::ostringstream ps;                                                                                         \
		ps << "EG_PANIC " __FILE__ ":" EG_MACRO_ITOS(__LINE__) "\n" << msg;                                            \
		::eg::detail::PanicImpl(ps.str());                                                                             \
	}

#ifdef NDEBUG
#define EG_DEBUG_ASSERT(condition) static_cast<void>(condition)
#else
#define EG_DEBUG_ASSERT(condition) EG_ASSERT(condition)
#endif

#define EG_CONCAT_IMPL(x, y) x##y
#define EG_CONCAT(x, y) EG_CONCAT_IMPL(x, y)

namespace eg
{
EG_API extern void (*releasePanicCallback)(const std::string& message);

namespace detail
{
[[noreturn]] EG_API void PanicImpl(const std::string& message);
}
} // namespace eg
