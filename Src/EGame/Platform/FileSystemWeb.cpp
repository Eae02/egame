#ifdef __EMSCRIPTEN__

#include "../Assert.hpp"
#include "FileSystem.hpp"

namespace eg
{
bool FileExists(const char* path)
{
	return false;
}

std::string RealPath(const char* path)
{
	return path;
}

void CreateDirectory(const char* path) {}

bool IsRegularFile(const char* path)
{
	return false;
}

const std::string& AppDataPath()
{
	EG_PANIC("No appdata path on the web");
}

std::optional<MemoryMappedFile> MemoryMappedFile::OpenRead(const char* path)
{
	return std::nullopt;
}

void MemoryMappedFile::CloseImpl() {}
} // namespace eg

#endif
