#ifdef __linux__

#include "../String.hpp"
#include "FileSystem.hpp"

#include <linux/limits.h>
#include <pwd.h>
#include <sys/prctl.h>
#include <unistd.h>

namespace eg
{
static std::string appDataPath;

const std::string& AppDataPath()
{
	if (appDataPath.empty())
	{
		const char* LINUX_PATH = "/.local/share/";
		if (struct passwd* pwd = getpwuid(getuid()))
			appDataPath = Concat({ pwd->pw_dir, LINUX_PATH });
		else
			appDataPath = Concat({ getenv("HOME"), LINUX_PATH });
	}
	return appDataPath;
}
} // namespace eg

#endif
