#pragma once

#ifndef __EMSCRIPTEN__

#include "../API.hpp"

#include <string>

namespace eg
{
class EG_API DynamicLibrary
{
public:
	static std::string PlatformFormat(std::string_view name);

	DynamicLibrary() = default;

	~DynamicLibrary() { Close(); }

	DynamicLibrary(DynamicLibrary&& other) noexcept : m_handle(other.m_handle) { other.m_handle = nullptr; }

	DynamicLibrary& operator=(DynamicLibrary&& other) noexcept
	{
		Close();
		m_handle = other.m_handle;
		other.m_handle = nullptr;
		return *this;
	}

	static const char* FailureReason();

	bool Open(const char* path);

	void Close();

	void* GetSymbol(const char* name) const;

private:
	void* m_handle = nullptr;
};
} // namespace eg

#endif
