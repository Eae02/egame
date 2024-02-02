#ifdef __APPLE__

#include "../Assert.hpp"

#import <Foundation/Foundation.h>

#include <string>

namespace eg
{
static std::string appDataPath;

const std::string& AppDataPath()
{
	if (appDataPath.empty())
	{
		auto autoreleasePool = [[NSAutoreleasePool alloc] init];

		NSFileManager* fileManager = [NSFileManager defaultManager];
		NSArray* URLs = [fileManager URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask];
		if (URLs.count == 0)
		{
			EG_PANIC("Could not find path to application support");
		}

		NSURL* URL = [URLs objectAtIndex:0];
		NSString* path = URL.path;

		appDataPath = path.fileSystemRepresentation;

		[(NSAutoreleasePool*)autoreleasePool release];
	}
	return appDataPath;
}
} // namespace eg

#endif
