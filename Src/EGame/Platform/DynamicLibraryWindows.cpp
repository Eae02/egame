#ifdef _WIN32

#include "../String.hpp"
#include "DynamicLibrary.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace eg
{
bool DynamicLibrary::Open(const char* path)
{
	Close();
	m_handle = reinterpret_cast<void*>(LoadLibrary(path));
	return m_handle != nullptr;
}

void DynamicLibrary::Close()
{
	if (m_handle != nullptr)
	{
		FreeLibrary(reinterpret_cast<HMODULE>(m_handle));
		m_handle = nullptr;
	}
}

void* DynamicLibrary::GetSymbol(const char* name) const
{
	return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(m_handle), name));
}

std::string DynamicLibrary::PlatformFormat(std::string_view name)
{
	return Concat({ name, ".dll" });
}

static char errorBuffer[1024];

const char* DynamicLibrary::FailureReason()
{
	auto errorId = GetLastError();
	if (errorId == 0)
		return nullptr;

	FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, errorId,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorBuffer, sizeof(errorBuffer), nullptr);

	return errorBuffer;
}
} // namespace eg

#endif
