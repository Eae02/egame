#if defined(__linux__) || defined(__APPLE__)

#include "FileSystem.hpp"
#include "../String.hpp"

#include <iostream>
#include <libgen.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

namespace eg
{
	bool FileExists(const char* path)
	{
		return access(path, F_OK) != -1;
	}
	
	std::string RealPath(const char* path)
	{
		char* rp = realpath(path, nullptr);
		std::string result(rp);
		std::free(rp);
		return result;
	}
	
	void CreateDirectory(const char* path)
	{
		mkdir(path, S_IRWXU);
	}
	
	bool IsRegularFile(const char* path)
	{
		struct stat attrib;
		stat(path, &attrib);
		return S_ISREG(attrib.st_mode);
	}
}

#endif
