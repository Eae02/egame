#include "Memory.hpp"

#include <fstream>
#include <string>

namespace eg
{
#if defined(__linux__)
	uint32_t GetMemoryUsageRSS()
	{
		std::ifstream stream("/proc/self/statm");
		if (!stream)
			return 0;
		
		std::string line;
		std::getline(stream, line);
		
		size_t space1 = line.find(' ');
		size_t space2 = line.find(' ', space1 + 1);
		size_t space3 = line.find(' ', space2 + 1);
		
		if (space1 == std::string::npos || space2 == std::string::npos || space3 == std::string::npos)
			return 0;
		
		line[space2] = '\0';
		line[space3] = '\0';
		
		int rssPages = atoi(&line[space1 + 1]) - atoi(&line[space2 + 1]);
		return rssPages * 4096;
	}
#else
	uint32_t GetMemoryUsageRSS()
	{
		return 0;
	}
#endif
}
