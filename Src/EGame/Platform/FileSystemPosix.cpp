#if defined(__linux__) || defined(__APPLE__)

#include "../String.hpp"
#include "FileSystem.hpp"

#include <fcntl.h>
#include <iostream>
#include <libgen.h>
#include <pwd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace eg
{
bool FileExists(const char* path)
{
	return access(path, F_OK) != -1;
}

std::string RealPath(const char* path)
{
	char* rp = realpath(path, nullptr);
	std::string result(rp);
	std::free(rp);
	return result;
}

void CreateDirectory(const char* path)
{
	mkdir(path, S_IRWXU);
}

bool IsRegularFile(const char* path)
{
	struct stat attrib;
	stat(path, &attrib);
	return S_ISREG(attrib.st_mode);
}

std::optional<MemoryMappedFile> MemoryMappedFile::OpenRead(const char* path)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		return std::nullopt;

	struct stat fileInfo;
	if (fstat(fd, &fileInfo) == -1)
	{
		close(fd);
		return std::nullopt;
	}
	size_t fileSize = static_cast<size_t>(fileInfo.st_size);

	void* fileData = mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
	if (fileData == nullptr)
	{
		close(fd);
		return std::nullopt;
	}

	MemoryMappedFile file;
	file.data = std::span<const char>(static_cast<char*>(fileData), fileSize);
	file.handles = Handles { .fd = fd };
	return file;
}

void MemoryMappedFile::CloseImpl()
{
	close(handles->fd);
	munmap(const_cast<char*>(data.data()), data.size());
}
} // namespace eg

#endif
