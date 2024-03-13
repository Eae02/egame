#include "DefaultAssetGenerator.hpp"
#include "../Log.hpp"
#include "../Platform/FileSystem.hpp"
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

		std::optional<MemoryMappedFile> mappedFile = MemoryMappedFile::OpenRead(path.c_str());
		if (!mappedFile.has_value())
		{
			Log(LogLevel::Error, "as", "Error opening asset file for reading: '{0}'", path);
			return false;
		}

		context.writer.WriteBytes(mappedFile->data);
		context.outputFlags = AssetFlags::NeverCache;

		mappedFile->Close();

		return true;
	}
};

void detail::RegisterDefaultAssetGenerator()
{
	RegisterAssetGenerator<DefaultAssetGenerator>("Default", DefaultGeneratorFormat);
}
} // namespace eg
