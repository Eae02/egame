#ifdef _WIN32

#include "FileSystem.hpp"
#include "../Utils.hpp"

#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0600
#define _WIN32_WINNT 0x0600
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
			LPWSTR wszPath = NULL;
			SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &wszPath);
			appDataPath = std::string(wszPath);
		}
		return appDataPath;
	}
}

#endif
