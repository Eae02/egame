#include "Debug.hpp"
#include <iomanip>
#include <atomic>

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
}
