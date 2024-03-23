#ifdef _WIN32

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif

#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0600
#define _WIN32_WINNT 0x0600

#include "../Utils.hpp"
#include "FileSystem.hpp"

#include <shlobj.h>
#include <shlwapi.h>
#include <windows.h>

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

std::optional<MemoryMappedFile> MemoryMappedFile::OpenRead(const char* path)
{
	HANDLE fileHandle =
		CreateFile(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (fileHandle == INVALID_HANDLE_VALUE)
		return std::nullopt;

	DWORD fileSize = GetFileSize(fileHandle, nullptr);

	HANDLE mapping = CreateFileMapping(fileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);
	if (mapping == nullptr)
	{
		CloseHandle(fileHandle);
		return std::nullopt;
	}

	const void* fileData = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);

	MemoryMappedFile file;
	file.data = std::span<const char>(static_cast<const char*>(fileData), fileSize);
	file.handles = Handles{ .file = fileHandle, .mapping = mapping };
	return file;
}

void MemoryMappedFile::CloseImpl()
{
	UnmapViewOfFile(data.data());
	CloseHandle(handles->mapping);
	CloseHandle(handles->file);
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
} // namespace eg

#endif
