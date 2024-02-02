#include "DefaultAssetGenerator.hpp"
#include "../Log.hpp"
#include "AssetGenerator.hpp"

#include <fstream>

namespace eg
{
const AssetFormat DefaultGeneratorFormat{ "_Default", 0 };

class DefaultAssetGenerator : public AssetGenerator
{
public:
	bool Generate(AssetGenerateContext& context) override
	{
		std::string path = context.FileDependency(context.RelSourcePath());
		std::ifstream stream(path, std::ios::binary);
		if (!stream)
		{
			Log(LogLevel::Error, "as", "Error opening asset file for reading: '{0}'", path);
			return false;
		}

		char readBuffer[4096];
		while (!stream.eof())
		{
			stream.read(readBuffer, sizeof(readBuffer));
			context.outputStream.write(readBuffer, stream.gcount());
		}

		context.outputFlags = AssetFlags::NeverCache;
		return true;
	}
};

void detail::RegisterDefaultAssetGenerator()
{
	RegisterAssetGenerator<DefaultAssetGenerator>("Default", DefaultGeneratorFormat);
}
} // namespace eg
