#include "Texture2DLoader.hpp"
#include "AssetLoad.hpp"
#include "../Graphics/AbstractionHL.hpp"

namespace eg
{
	const AssetFormat Texture2DAssetFormat { "EG::Texture2D", 0 };
	
#pragma pack(push, 1)
	struct Header
	{
		uint32_t numLayers;
		uint32_t format;
		uint8_t linearFiltering;
		uint8_t anistropy;
		uint32_t numMipLevels;
		uint32_t width;
		uint32_t height;
	};
#pragma pack(pop)
	
	bool Texture2DLoader(const AssetLoadContext& loadContext)
	{
		const Header* header = reinterpret_cast<const Header*>(loadContext.Data().data());
		
		SamplerDescription sampler;
		sampler.maxAnistropy = header->anistropy ? 16 : 0;
		auto filter = header->linearFiltering ? eg::TextureFilter::Linear : eg::TextureFilter::Nearest;
		sampler.minFilter = sampler.magFilter = filter;
		
		Texture2DArrayCreateInfo createInfo;
		createInfo.flags = TextureFlags::GenerateMipmaps | TextureFlags::CopyDst | TextureFlags::ShaderSample;
		createInfo.defaultSamplerDescription = &sampler;
		createInfo.width = header->width;
		createInfo.height = header->height;
		createInfo.format = (Format)header->format;
		createInfo.arrayLayers = header->numLayers;
		createInfo.mipLevels = header->numMipLevels;
		
		Texture* texture;
		if (header->numLayers == 0)
			texture = &loadContext.CreateResult<Texture>(Texture::Create2D(createInfo));
		else
			texture = &loadContext.CreateResult<Texture>(Texture::Create2DArray(createInfo));
		
		const size_t layerBytes = header->width * header->height * GetFormatSize(createInfo.format);
		const size_t uploadBufferSize = layerBytes * std::max<uint32_t>(header->numLayers, 1);
		Buffer uploadBuffer(BufferFlags::HostAllocate | BufferFlags::CopySrc | BufferFlags::MapWrite,
			uploadBufferSize, nullptr);
		
		void* uploadBufferMemory = uploadBuffer.Map(0, uploadBufferSize);
		std::memcpy(uploadBufferMemory, loadContext.Data().data() + sizeof(Header), uploadBufferSize);
		uploadBuffer.Unmap(0, uploadBufferSize);
		
		TextureRange range = { };
		range.sizeX = header->width;
		range.sizeY = header->height;
		range.sizeZ = std::max<uint32_t>(header->numLayers, 1);
		
		eg::DC.SetTextureData(*texture, range, uploadBuffer, 0);
		
		if (createInfo.mipLevels > 1)
		{
			eg::DC.GenerateMipmaps(*texture);
		}
		
		texture->UsageHint(TextureUsage::ShaderSample, ShaderAccessFlags::Vertex | ShaderAccessFlags::Fragment);
		
		return true;
	}
}
