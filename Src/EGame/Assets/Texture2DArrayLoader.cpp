#include "Texture2DArrayLoader.hpp"
#include "AssetLoad.hpp"
#include "../Graphics/AbstractionHL.hpp"

namespace eg
{
	const AssetFormat Texture2DArrayAssetFormat { "EG::Texture2DArray", 0 };
	
	struct Header
	{
		uint32_t numLayers;
		uint32_t format;
		uint32_t numMipLevels;
		uint32_t width;
		uint32_t height;
	};
	
	bool Texture2DArrayLoader(const AssetLoadContext& loadContext)
	{
		const Header* header = reinterpret_cast<const Header*>(loadContext.Data().data());
		
		Texture2DArrayCreateInfo createInfo;
		createInfo.width = header->width;
		createInfo.height = header->height;
		createInfo.format = (Format)header->format;
		createInfo.arrayLayers = header->numLayers;
		createInfo.mipLevels = header->numMipLevels;
		
		Texture& texture = loadContext.CreateResult<Texture>(Texture::Create2DArray(createInfo));
		
		const char* dataIn = loadContext.Data().data() + sizeof(Header);
		size_t layerBytes = header->width * header->height * GetFormatSize(createInfo.format);
		for (uint32_t layer = 0; layer < header->numLayers; layer++)
		{
			TextureRange range;
			range.offsetX = 0;
			range.offsetY = 0;
			range.offsetZ = layer;
			range.sizeX = header->width;
			range.sizeY = header->height;
			range.sizeZ = 1;
			range.mipLevel = 0;
			
			eg::DC.SetTextureData(texture, range, dataIn);
			dataIn += layerBytes;
		}
		
		return true;
	}
}
