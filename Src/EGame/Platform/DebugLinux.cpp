#ifdef __linux__

#include "Debug.hpp"

#include <array>
#include <execinfo.h>

namespace eg
{
	std::vector<std::string> GetStackTrace()
	{
		std::array<void*, 128> trace = {};
		size_t traceSize = backtrace(trace.data(), trace.size());
		if (traceSize == 0)
			return { };
		
		char** traceSybmols = backtrace_symbols(trace.data(), (int)traceSize);
		if (traceSybmols == nullptr)
			return { };
		
		std::vector<std::string> result(traceSize);
		for (size_t i = 0; i < traceSize; i++)
		{
			result[i] = traceSybmols[i];
		}
		
		free(traceSybmols);
		return result;
	}
}

#endif
