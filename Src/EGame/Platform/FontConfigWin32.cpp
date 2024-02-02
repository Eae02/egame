#ifdef _WIN32

#include "../String.hpp"
#include "FontConfig.hpp"

#include <limits>
#include <sstream>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#undef min

namespace eg
{
void InitPlatformFontConfig() {}
void DestroyPlatformFontConfig() {}

std::string GetFontPathByName(const char* name)
{
	static const LPCSTR fontRegistryPath = R"(Software\Microsoft\Windows NT\CurrentVersion\Fonts)";

	size_t nameLen = strlen(name);

	HKEY hKey;
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, fontRegistryPath, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
		return {};

	DWORD maxValueNameSize, maxValueDataSize;
	if (RegQueryInfoKey(hKey, 0, 0, 0, 0, 0, 0, 0, &maxValueNameSize, &maxValueDataSize, 0, 0) != ERROR_SUCCESS)
		return {};

	DWORD valueIndex = 0;
	std::string fontFileName;

	LPSTR valueName = static_cast<LPSTR>(alloca(maxValueNameSize));
	LPBYTE valueData = static_cast<LPBYTE>(alloca(maxValueDataSize));
	DWORD valueNameSize, valueDataSize, valueType, shortestNameLen = 0; //=0 to prevent compiler warning

	LONG result;
	do
	{
		valueDataSize = maxValueDataSize;
		valueNameSize = maxValueNameSize;

		result = RegEnumValue(hKey, valueIndex, valueName, &valueNameSize, 0, &valueType, valueData, &valueDataSize);

		valueIndex++;

		if (result != ERROR_SUCCESS || valueType != REG_SZ)
			continue;

		// Checks if this registry entry's name is shorter than the currently shortest name length.
		if (fontFileName.empty() || valueNameSize < shortestNameLen)
		{
			// Checks if this registry entry's name starts with the font name.
			if (StringEqualCaseInsensitive({ name, nameLen }, { valueName, std::min<size_t>(nameLen, valueNameSize) }))
			{
				fontFileName.assign(reinterpret_cast<LPSTR>(valueData), valueDataSize);
				shortestNameLen = valueNameSize;
			}
		}
	} while (result != ERROR_NO_MORE_ITEMS);

	RegCloseKey(hKey);

	if (fontFileName.empty())
		return {};

	if (fontFileName.size() >= 2 && fontFileName[1] == ':')
		return fontFileName;

	char windowsDirPath[MAX_PATH];
	GetWindowsDirectory(windowsDirPath, MAX_PATH);

	std::stringstream pathStream;
	pathStream << windowsDirPath << R"(\Fonts\)" << fontFileName;
	return pathStream.str();
}
} // namespace eg

#endif
