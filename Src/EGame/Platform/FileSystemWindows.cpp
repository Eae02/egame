#ifdef _WIN32

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif

#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0600
#define _WIN32_WINNT 0x0600

#include "FileSystem.hpp"
#include "../Utils.hpp"

#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>

#undef CreateDirectory

namespace eg
{
	bool FileExists(const char* path)
	{
		return PathFileExistsA(path);
	}
	
	std::string RealPath(const char* path)
	{
		TCHAR pathOut[MAX_PATH];
		GetFullPathNameA(path, MAX_PATH, pathOut, nullptr);
		return pathOut;
	}
	
	void CreateDirectory(const char* path)
	{
		CreateDirectoryA(path, nullptr);
	}
	
	bool IsRegularFile(const char* path)
	{
		return GetFileAttributes(path) == FILE_ATTRIBUTE_NORMAL;
	}
	
	static std::string appDataPath;
	
	const std::string& AppDataPath()
	{
		if (appDataPath.empty())
		{
			char szPath[MAX_PATH];
			if (SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_APPDATA, nullptr, 0, szPath)))
			{
				appDataPath = std::string(szPath) + "/";
			}
			else
			{
				EG_PANIC("Could not get path to appdata")
			}
		}
		return appDataPath;
	}
}

#endif
