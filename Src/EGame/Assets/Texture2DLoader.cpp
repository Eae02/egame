#include "Texture2DLoader.hpp"
#include "AssetLoad.hpp"
#include "../Graphics/AbstractionHL.hpp"

namespace eg
{
	const AssetFormat Texture2DAssetFormat { "EG::Texture2D", 1 };
	
	TextureQuality TextureAssetQuality = TextureQuality::Medium;
	
#pragma pack(push, 1)
	struct Header
	{
		uint32_t numLayers;
		uint32_t format;
		uint8_t flags;
		uint8_t mipShifts[3];
		uint32_t numMipLevels;
		uint32_t width;
		uint32_t height;
	};
#pragma pack(pop)
	
	enum
	{
		TF_LinearFiltering = 0x1,
		TF_Anistropy = 0x2,
		TF_UseGlobalScale = 0x4,
		TF_ArrayTexture = 0x8
	};
	
	bool Texture2DLoader(const AssetLoadContext& loadContext)
	{
		const Header* header = reinterpret_cast<const Header*>(loadContext.Data().data());
		
		SamplerDescription sampler;
		sampler.maxAnistropy = (header->flags & TF_Anistropy) ? 16 : 0;
		auto filter = (header->flags & TF_Anistropy) ? eg::TextureFilter::Linear : eg::TextureFilter::Nearest;
		sampler.minFilter = sampler.magFilter = filter;
		
		uint32_t mipShift = std::min((uint32_t)header->mipShifts[(int)TextureAssetQuality], header->numMipLevels - 1);
		
		Texture2DArrayCreateInfo createInfo;
		createInfo.flags = TextureFlags::CopyDst | TextureFlags::ShaderSample;
		createInfo.defaultSamplerDescription = &sampler;
		createInfo.width = header->width >> mipShift;
		createInfo.height = header->height >> mipShift;
		createInfo.format = (Format)header->format;
		createInfo.arrayLayers = header->numLayers;
		createInfo.mipLevels = header->numMipLevels - mipShift;
		
		Texture* texture;
		if (header->flags & TF_ArrayTexture)
			texture = &loadContext.CreateResult<Texture>(Texture::Create2DArray(createInfo));
		else
			texture = &loadContext.CreateResult<Texture>(Texture::Create2D(createInfo));
		
		size_t bytesPerLayer = 0;
		for (uint32_t i = 0; i < header->numMipLevels; i++)
		{
			bytesPerLayer += GetImageByteSize(std::max(header->width >> i, 1U), std::max(header->height >> i, 1U),
				createInfo.format);
		}
		
		const size_t uploadBufferSize = bytesPerLayer * header->numLayers;
		EG_ASSERT(uploadBufferSize + sizeof(Header) <= loadContext.Data().size());
		
		Buffer uploadBuffer(BufferFlags::HostAllocate | BufferFlags::CopySrc | BufferFlags::MapWrite,
			uploadBufferSize, nullptr);
		
		void* uploadBufferMemory = uploadBuffer.Map(0, uploadBufferSize);
		std::memcpy(uploadBufferMemory, loadContext.Data().data() + sizeof(Header), uploadBufferSize);
		uploadBuffer.Flush(0, uploadBufferSize);
		
		uint64_t bufferOffset = 0;
		for (uint32_t i = 0; i < header->numLayers; i++)
		{
			for (uint32_t mip = 0; mip < header->numMipLevels; mip++)
			{
				const uint32_t mipWidth = std::max(header->width >> mip, 1U);
				const uint32_t mipHeight = std::max(header->height >> mip, 1U);
				
				if (mip >= mipShift)
				{
					TextureRange range = {};
					range.sizeX = mipWidth;
					range.sizeY = mipHeight;
					range.sizeZ = 1;
					range.offsetZ = i;
					range.mipLevel = mip - mipShift;
					
					eg::DC.SetTextureData(*texture, range, uploadBuffer, bufferOffset);
				}
				
				bufferOffset += GetImageByteSize(mipWidth, mipHeight, createInfo.format);
			}
		}
		
		texture->UsageHint(TextureUsage::ShaderSample, ShaderAccessFlags::Vertex | ShaderAccessFlags::Fragment);
		
		return true;
	}
}
