#if defined(__linux__) || defined(__APPLE__)

#include "Debug.hpp"
#include "../Utils.hpp"

#include <array>
#include <execinfo.h>
#include <cxxabi.h>

namespace eg
{
	std::vector<std::string> GetStackTrace()
	{
		std::array<void*, 128> trace = {};
		size_t traceSize = backtrace(trace.data(), trace.size());
		if (traceSize == 0)
			return { };
		
		char** traceSybmols = backtrace_symbols(trace.data(), ToInt(traceSize));
		if (traceSybmols == nullptr)
			return { };
		
		std::vector<std::string> result(traceSize);
		for (size_t i = 0; i < traceSize; i++)
		{
			result[i] = traceSybmols[i];
			
			size_t symbolEnd = result[i].rfind('+');
			if (symbolEnd == std::string::npos || symbolEnd == 0)
				continue;
			symbolEnd--;
			while (symbolEnd > 0 && isspace(result[i][symbolEnd]))
				symbolEnd--;
			
			size_t symbolBegin = result[i].rfind(' ', symbolEnd);
			if (symbolBegin == std::string::npos)
				continue;
			
			std::string mangledName = result[i].substr(symbolBegin, symbolEnd - symbolBegin);
			
			int status = 0;
			char* demangled = abi::__cxa_demangle(mangledName.c_str(), nullptr, nullptr, &status);
			if (status == 0)
			{
				result[i] = result[i].substr(0, symbolBegin) + demangled + result[i].substr(symbolEnd);
			}
			std::free(demangled);
		}
		
		free(traceSybmols);
		return result;
	}
}

#endif
