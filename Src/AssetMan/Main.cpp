#include <fstream>
#include <iostream>

#include "../ANSIColors.hpp"
#include "../EGame/Alloc/LinearAllocator.hpp"
#include "../EGame/Assets/Asset.hpp"
#include "../EGame/Assets/AssetLoad.hpp"
#include "../EGame/Assets/EAPFile.hpp"
#include "InfoOutput.hpp"
#include "ParseArguments.hpp"

int main(int argc, char** argv)
{
	if (argc <= 1)
	{
		std::cout << "expected more arguments" << std::endl;
		return 1;
	}

	ParsedArguments parsedArguments = ParseArguments(argc, argv);

	if (parsedArguments.updateCache)
	{
		eg::LoadAssetGenLibrary();
		eg::detail::RegisterAssetLoaders();
		auto assetsInfo = eg::DetectAndGenerateYAMLAssets(parsedArguments.inputFileName);
		if (assetsInfo.has_value())
		{
			for (const eg::YAMLAssetInfo& assetInfo : *assetsInfo)
			{
				switch (assetInfo.status)
				{
				case eg::YAMLAssetStatus::Cached: break;
				case eg::YAMLAssetStatus::Generated:
					std::cout << ANSI_COLOR_CYAN "regenerated asset: " << assetInfo.name << ANSI_COLOR_RESET
							  << std::endl;
					break;
				case eg::YAMLAssetStatus::ErrorGenerate:
					std::cout << ANSI_COLOR_RED "error generating asset: " << assetInfo.name << ANSI_COLOR_RESET
							  << std::endl;
					break;
				case eg::YAMLAssetStatus::ErrorUnknownExtension:
				case eg::YAMLAssetStatus::ErrorLoaderNotFound: break;
				}
			}
		}
		return 0;
	}

	eg::LinearAllocator allocator;
	allocator.disableMultiPoolWarning = true;
	auto readResult = eg::ReadEAPFileFromFileSystem(
		std::string(parsedArguments.inputFileName), [](std::string_view) { return true; }, allocator);
	if (!readResult)
	{
		std::cout << "error reading eap from '" << parsedArguments.inputFileName << "'" << std::endl;
		return 1;
	}

	std::span<eg::EAPAsset> assets = readResult->assets;

	if (assets.empty())
	{
		std::cout << "file ok, but contains no assets" << std::endl;
		return 0;
	}

	bool operationPerformed = false;
	if (parsedArguments.writeList)
	{
		WriteListOutput(assets);
		operationPerformed = true;
	}

	if (parsedArguments.removeByName.empty())
	{
		if (!operationPerformed)
			std::cout << "file ok, no operation performed" << std::endl;
		return 1;
	}
}
