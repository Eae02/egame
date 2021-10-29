#include "Debug.hpp"
#include <iomanip>
#include <atomic>
#include <iostream>

#ifdef __GNUC__
#include <cxxabi.h>
#endif

namespace eg
{
	static std::atomic_int stackTraceID = 0;
	
	void PrintStackTraceToStdOut(std::string_view message)
	{
		std::vector<std::string> trace = GetStackTrace();
		
		time_t time = std::time(nullptr);
		std::cout << "Stack trace @" << std::put_time(std::localtime(&time), "%H:%M:%S") << " [" << (stackTraceID++) << "]";
		if (!message.empty())
			std::cout << " " << message;
		std::cout << ":\n";
		if (!trace.empty())
		{
			for (const std::string& entry : trace)
				std::cout << "  " << entry << "\n";
		}
		else
		{
			std::cout << "  No trace\n";
		}
		std::cout << std::flush;
	}
	
#ifndef __linux__
	std::vector<std::string> GetStackTrace() { return { }; }
#endif
	
#ifdef __GNUC__
	std::string DemangeTypeName(const char* name)
	{
		int status = 0;
		char* result = abi::__cxa_demangle(name, nullptr, nullptr, &status);
		std::string resultString = status ? name : result;
		std::free(result);
		return resultString;
	}
#else
	std::string DemangeTypeName(const char* name)
	{
		return name;
	}
#endif
}
