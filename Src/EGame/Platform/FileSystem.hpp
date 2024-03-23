#pragma once

#include "../API.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

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

EG_API const std::string& AppDataPath();

EG_API void CreateDirectory(const char* path);

EG_API void CreateDirectories(std::string_view path);

EG_API bool IsRegularFile(const char* path);

class EG_API MemoryMappedFile
{
public:
	MemoryMappedFile() = default;

	~MemoryMappedFile() { Close(); }

	MemoryMappedFile(const MemoryMappedFile& other) = delete;
	MemoryMappedFile& operator=(const MemoryMappedFile& other) = delete;

	MemoryMappedFile(MemoryMappedFile&& other)
	{
		std::swap(other.data, data);
		std::swap(other.handles, handles);
	}

	MemoryMappedFile& operator=(MemoryMappedFile&& other)
	{
		Close();
		std::swap(other.data, data);
		std::swap(other.handles, handles);
		return *this;
	}

	static std::optional<MemoryMappedFile> OpenRead(const char* path);

	std::span<const char> data;

	void Close()
	{
		if (handles.has_value())
		{
			CloseImpl();
			data = {};
			handles = std::nullopt;
		}
	}

private:
	void CloseImpl();

	struct Handles
	{
#if defined(__linux__) || defined(__APPLE__)
		int fd;
#elif defined(_WIN32)
		void* file;
		void* mapping;
#endif
	};

	std::optional<Handles> handles;
};
} // namespace eg
