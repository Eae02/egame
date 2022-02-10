#ifdef __linux__

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
		return Concat({ "lib", name, ".so" });
	}
	
	const char* DynamicLibrary::FailureReason()
	{
		return dlerror();
	}
}

#endif
