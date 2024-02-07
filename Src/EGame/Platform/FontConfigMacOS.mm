#ifdef __APPLE__

#include "../Assert.hpp"

#include <ApplicationServices/ApplicationServices.h>
#include <CoreText/CoreText.h>

#include <string>

namespace eg
{
void InitPlatformFontConfig() {}
void DestroyPlatformFontConfig() {}

std::string GetFontPathByName(const char* name)
{
	CFStringRef nameCFString = CFStringCreateWithCString(nullptr, name, kCFStringEncodingUTF8);
	CTFontRef fontRef = CTFontCreateWithName(nameCFString, 0.0, nullptr);
	CFURLRef fontUrl = static_cast<CFURLRef>(CTFontCopyAttribute(fontRef, kCTFontURLAttribute));
	NSString* fontUrlNS = (NSString*)CFURLGetString(fontUrl);
	std::string result([fontUrlNS UTF8String]);

	static const std::string_view fileUrlPrefix = "file:";
	if (result.starts_with(fileUrlPrefix))
	{
		result.erase(result.begin(), result.begin() + fileUrlPrefix.size());
	}

	CFRelease(fontRef);
	CFRelease(fontUrl);

	return result;
}
} // namespace eg

#endif

// ATSFontGetFileReference
