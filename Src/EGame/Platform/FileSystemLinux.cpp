#ifdef __linux__

#include "FileSystem.hpp"
#include "../String.hpp"

#include <sys/prctl.h>
#include <linux/limits.h>

namespace eg
{
	static std::string appDataPath;
	
	const std::string& AppDataPath()
	{
		if (appDataPath.empty())
		{
			const char* LINUX_PATH = "/.local/share/";
			if (struct passwd* pwd = getpwuid(getuid()))
				appDataPath = Concat({pwd->pw_dir, LINUX_PATH});
			else
				appDataPath = Concat({getenv("HOME"), LINUX_PATH});
		}
		return appDataPath;
	}
}

#endif
