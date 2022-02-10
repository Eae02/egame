#pragma once

#include "API.hpp"

#include <string>
#include <iostream>

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

#ifdef NDEBUG
#include <sstream>
#define EG_ASSERT(condition)
#define EG_DEBUG_ASSERT(condition) if (!(condition)) {\
	::eg::ReleasePanic("A runtime error occured\nAssertion at " __FILE__ ":" EG_MACRO_ITOS(__LINE__));\
}
#define EG_PANIC(msg) {\
	std::ostringstream ps;\
	ps << "A runtime error occured\nPanic at " __FILE__ ":" EG_MACRO_ITOS(__LINE__) "\n" << msg;\
	::eg::ReleasePanic(ps.str());\
}
#else
#define EG_PANIC(msg) { \
	std::cerr << "PANIC@" __FILE__ ":" EG_MACRO_ITOS(__LINE__) "\n" << msg << std::endl;\
	EG_DEBUG_BREAK;\
	std::abort();\
}
#define EG_DEBUG_ASSERT(condition) EG_ASSERT(condition)
#define EG_ASSERT(condition) if (!(condition)) {\
	std::cerr << "ASSERT@" __FILE__ ":" EG_MACRO_ITOS(__LINE__) " " #condition << std::endl;\
	EG_DEBUG_BREAK;\
	std::abort();\
}
#endif

#define EG_CONCAT_IMPL(x, y) x##y
#define EG_CONCAT(x, y) EG_CONCAT_IMPL(x, y)

namespace eg
{
	EG_API extern void (*releasePanicCallback)(const std::string& message);
	
	[[noreturn]] EG_API void ReleasePanic(const std::string& message);
}
