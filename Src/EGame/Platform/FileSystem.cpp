#include "FileSystem.hpp"
#include "../Utils.hpp"

#include <stdio.h>
#include <time.h>
#include <sys/stat.h>

namespace eg
{
	std::string ResolveRelativePath(std::string_view relativeTo, std::string_view path)
	{
		if (path.empty())
			return { };
		if (path[0] == '/' || relativeTo.empty())
			return std::string(path);
		if (relativeTo.back() == '/')
			return Concat({ relativeTo, path });
		return Concat({ relativeTo, "/", path });
	}
	
	std::string_view BaseName(std::string_view path)
	{
		const size_t lastSlash = path.rfind('/');
		if (lastSlash == std::string_view::npos)
			return path;
		return path.substr(lastSlash + 1);
	}
	
	std::string_view PathWithoutExtension(std::string_view fileName)
	{
		const size_t lastDot = fileName.rfind('.');
		if (lastDot == std::string_view::npos)
			return fileName;
		return fileName.substr(0, lastDot);
	}
	
	//Does not include the dot before the extension!
	std::string_view PathExtension(std::string_view fileName)
	{
		const size_t lastDot = fileName.rfind('.');
		if (lastDot == std::string_view::npos)
			return { };
		return fileName.substr(lastDot + 1);
	}
	
	std::string_view ParentPath(std::string_view path, bool includeSlash)
	{
		const size_t lastSlash = path.rfind('/');
		if (lastSlash == std::string_view::npos)
			return { };
		return path.substr(0, lastSlash + (includeSlash ? 1 : 0));
	}
	
	std::chrono::system_clock::time_point LastWriteTime(const char* path)
	{
		struct stat attrib;
		stat(path, &attrib);
		return std::chrono::system_clock::from_time_t(attrib.st_mtime);
	}
	
	void CreateDirectories(const char* path)
	{
		size_t pathLen = strlen(path);
		char* pathCopy = reinterpret_cast<char*>(alloca(pathLen + 1));
		std::memcpy(pathCopy, path, pathLen + 1);
		
		bool seenNonSep = false;
		for (size_t i = 0; true; i++)
		{
			if (path[i] == '\\' || path[i] == '/' || path[i] == '\0')
			{
				if (seenNonSep)
				{
					pathCopy[i] = '\0';
					if (!FileExists(pathCopy))
						CreateDirectory(pathCopy);
					pathCopy[i] = path[i];
					seenNonSep = false;
				}
				
				if (path[i] == '\0')
					break;
			}
			else
				seenNonSep = true;
		}
	}
}
