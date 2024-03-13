#include "Texture2DLoader.hpp"
#include "../Graphics/AbstractionHL.hpp"
#include "AssetLoad.hpp"

namespace eg
{
const AssetFormat Texture2DAssetFormat{ "EG::Texture2D", 4 };

TextureQuality TextureAssetQuality = TextureQuality::Medium;

static_assert(sizeof(Format) == sizeof(uint32_t));

enum
{
	TF_UseGlobalScale = 4,
	TF_ArrayTexture = 8,
	TF_CubeMap = 16,
	TF_3D = 32
};

bool Texture2DLoader(const AssetLoadContext& loadContext)
{
	Texture2DHeader header = ReadFromSpan<Texture2DHeader>(loadContext.Data(), 0);

	uint32_t mipShift = std::min(header.mipShifts[static_cast<int>(TextureAssetQuality)], header.numMipLevels - 1);

	if (IsCompressedFormat(header.format))
	{
		uint32_t requestedMipShift = mipShift;
		while (mipShift > 0 && ((header.width >> mipShift) % 4 || (header.height >> mipShift) % 4))
		{
			mipShift--;
		}
		if (requestedMipShift != mipShift)
		{
			eg::Log(
				eg::LogLevel::Warning, "as",
				"Mip shift {0} applied instead of the requested {1} because the"
				" compressed texture '{2}' would otherwise have a resolution that is not a multiple of 4.",
				mipShift, requestedMipShift, loadContext.AssetPath());
		}
	}

	Texture* texture;

	TextureCreateInfo createInfo;
	createInfo.flags = TextureFlags::CopyDst | TextureFlags::ShaderSample;
	createInfo.width = header.width >> mipShift;
	createInfo.height = header.height >> mipShift;
	createInfo.format = header.format;
	createInfo.arrayLayers = header.numLayers;
	createInfo.mipLevels = header.numMipLevels - mipShift;

	AssertFormatSupport(createInfo.format, eg::FormatCapabilities::SampledImage);

	if (header.flags & TF_CubeMap)
	{
		texture = &loadContext.CreateResult<Texture>(Texture::CreateCube(createInfo));
	}
	else if (header.flags & TF_3D)
	{
		createInfo.arrayLayers = 1;
		createInfo.depth = header.numLayers;
		texture = &loadContext.CreateResult<Texture>(Texture::Create3D(createInfo));
	}
	else if (header.flags & TF_ArrayTexture)
	{
		texture = &loadContext.CreateResult<Texture>(Texture::Create2DArray(createInfo));
	}
	else
	{
		texture = &loadContext.CreateResult<Texture>(Texture::Create2D(createInfo));
	}

	size_t bytesPerLayer = 0;
	for (uint32_t i = 0; i < header.numMipLevels; i++)
	{
		bytesPerLayer +=
			GetImageByteSize(std::max(header.width >> i, 1U), std::max(header.height >> i, 1U), createInfo.format);
	}

	const size_t uploadBufferSize = bytesPerLayer * header.numLayers;
	EG_ASSERT(uploadBufferSize + sizeof(Texture2DHeader) <= loadContext.Data().size());

	Buffer uploadBuffer(BufferFlags::CopySrc | BufferFlags::MapWrite, uploadBufferSize, nullptr);

	void* uploadBufferMemory = uploadBuffer.Map(0, uploadBufferSize);
	std::memcpy(uploadBufferMemory, loadContext.Data().data() + sizeof(Texture2DHeader), uploadBufferSize);
	uploadBuffer.Flush(0, uploadBufferSize);

	uint64_t bufferOffset = 0;
	for (uint32_t i = 0; i < header.numLayers; i++)
	{
		for (uint32_t mip = 0; mip < header.numMipLevels; mip++)
		{
			const uint32_t mipWidth = std::max(header.width >> mip, 1U);
			const uint32_t mipHeight = std::max(header.height >> mip, 1U);

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

void Texture2DLoaderPrintInfo(std::span<const char> data, std::ostream& outStream)
{
	Texture2DHeader header = ReadFromSpan<Texture2DHeader>(data, 0);

	uint32_t depth = 0;
	const char* type;
	if (header.flags & TF_CubeMap)
	{
		type = "cubemap";
	}
	else if (header.flags & TF_3D)
	{
		type = "3d";
		depth = header.numLayers;
	}
	else if (header.flags & TF_ArrayTexture)
	{
		type = "array";
		depth = header.numLayers;
	}
	else
	{
		type = "2d";
	}

	outStream << " " << type << " " << header.width << "x" << header.height;
	if (depth)
		outStream << "x" << depth;

	outStream << " " << FormatToString(header.format) << std::endl;
}
} // namespace eg
