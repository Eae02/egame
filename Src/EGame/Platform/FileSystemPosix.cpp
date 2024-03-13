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

const intptr_t MemoryMappedFile::FILE_HANDLE_NULL = -1;

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
	off_t file_size = fileInfo.st_size;

	void* fileData = mmap(NULL, fileInfo.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (fileData == nullptr)
	{
		close(fd);
		return std::nullopt;
	}

	MemoryMappedFile file;
	file.data = std::span<const char>(static_cast<char*>(fileData), fileInfo.st_size);
	file.fileHandle = fd;
	return file;
}

void MemoryMappedFile::CloseImpl()
{
	close(fileHandle);
	munmap(const_cast<char*>(data.data()), data.size());
}
} // namespace eg

#endif
