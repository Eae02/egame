#if defined(__linux__) || defined(__APPLE__)

#include "../String.hpp"
#include "DynamicLibrary.hpp"

#include <dlfcn.h>

namespace eg
{
bool DynamicLibrary::Open(const char* path)
{
	Close();
	m_handle = dlopen(path, RTLD_LAZY);
	return m_handle != nullptr;
}

void DynamicLibrary::Close()
{
	if (m_handle != nullptr)
	{
		dlclose(m_handle);
		m_handle = nullptr;
	}
}

void* DynamicLibrary::GetSymbol(const char* name) const
{
	return dlsym(m_handle, name);
}

#ifdef __linux__
const std::string_view DynamicLibrary::FileExtension = ".so";
#endif
#ifdef __APPLE__
const std::string_view DynamicLibrary::FileExtension = ".dylib";
#endif

std::string DynamicLibrary::PlatformFormat(std::string_view name)
{
	return Concat({ "lib", name, DynamicLibrary::FileExtension });
}

const char* DynamicLibrary::FailureReason()
{
	return dlerror();
}
} // namespace eg

#endif
