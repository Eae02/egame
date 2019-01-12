#ifdef _WIN32

#include "FileSystem.hpp"
#include "../Utils.hpp"

#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0600
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>

namespace eg
{
	bool Exists(const std::string& path)
	{
		return PathFileExistsA(path.c_str());
	}
	
	std::string RealPath(const std::string& path)
	{
		TCHAR pathOut[MAX_PATH];
		GetFullPathNameA(path.c_str(), MAX_PATH, pathOut, nullptr);
		return pathOut;
	}
	
	void CreateDirectory(const std::string& path)
	{
		CreateDirectoryA(path.c_str());
	}
	
	bool IsRegularFile(const std::string& path)
	{
		return GetFileAttributes(path.c_str()) == FILE_ATTRIBUTE_NORMAL;
	}
}

#endif
