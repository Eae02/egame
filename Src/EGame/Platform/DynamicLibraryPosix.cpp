#if defined(__linux__) || defined(__APPLE__)

#include "DynamicLibrary.hpp"
#include "../String.hpp"

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
	
	std::string DynamicLibrary::PlatformFormat(std::string_view name)
	{
#ifdef __linux__
		constexpr const char* SUFFIX = ".so";
#endif
#ifdef __APPLE__
		constexpr const char* SUFFIX = ".dylib";
#endif

		return Concat({ "lib", name, SUFFIX });
	}
	
	const char* DynamicLibrary::FailureReason()
	{
		return dlerror();
	}
}

#endif
