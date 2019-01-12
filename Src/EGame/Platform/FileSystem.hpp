#pragma once

#include "../API.hpp"

namespace eg
{
	EG_API bool FileExists(const char* path);
	
	EG_API std::chrono::system_clock::time_point LastWriteTime(const char* path);
	
	EG_API std::string ResolveRelativePath(std::string_view relativeTo, std::string_view path);
	
	EG_API std::string_view ParentPath(std::string_view path, bool includeSlash = true);
	
	EG_API std::string_view BaseName(std::string_view path);
	
	EG_API std::string_view PathWithoutExtension(std::string_view fileName);
	
	EG_API std::string_view PathExtension(std::string_view fileName);
	
	EG_API std::string RealPath(const char* path);
	
	EG_API void CreateDirectory(const char* path);
	
	EG_API void CreateDirectories(const char* path);
	
	EG_API bool IsRegularFile(const char* path);
}
