#include "InfoOutput.hpp"
#include "ANSIColors.hpp"

#include "../EGame/Utils.hpp"
#include "../EGame/Assets/AssetLoad.hpp"
#include "../EGame/Assets/Texture2DLoader.hpp"

#include <iomanip>
#include <iostream>

void WriteListOutput(std::span<const eg::EAPAsset> assets)
{
	std::cout << "list output (in load-order):" << std::endl;
	
	for (const eg::EAPAsset& asset : assets)
	{
		std::cout << ANSI_COLOR_GREEN << asset.assetName << ANSI_COLOR_RESET << std::endl;
		
		std::cout << " " << asset.loaderName
			<< " " << asset.format.version << ":" << std::hex << asset.format.nameHash << std::dec
			<< " " << eg::ReadableBytesSize(asset.generatedAssetData.size());
		
		if (asset.compress)
		{
			int compressionRatio = std::round(100 * (1 - (double)asset.compressedSize / (double)asset.generatedAssetData.size()));
			std::cout << " (comp: " << eg::ReadableBytesSize(asset.compressedSize)
					  << " " << compressionRatio << "%)";
		}
		std::cout << std::endl;
		
		if (asset.loaderName == "Texture2D" && asset.format == eg::Texture2DAssetFormat)
			eg::Texture2DLoaderPrintInfo(asset.generatedAssetData, std::cout);
	}
}
