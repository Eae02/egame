#include "TextureUpload.hpp"

#include <numeric>

namespace eg
{
TextureUploadBuffer::TextureUploadBuffer(
	std::span<const char> packedData, const TextureRange& range, Format format, GraphicsLoadContext& loadContext)
	: m_range(range)
{
	const uint32_t blockSize = GetFormatBlockWidth(format);
	EG_ASSERT((range.offsetX % blockSize) == 0);
	EG_ASSERT((range.offsetY % blockSize) == 0);

	const uint32_t sizeXBlocks = (range.sizeX + blockSize - 1) / blockSize;
	const uint32_t sizeYBlocks = (range.sizeY + blockSize - 1) / blockSize;

	const uint32_t bytesPerBlock = GetFormatBytesPerBlock(format);

	const uint32_t packedBytesPerRow = sizeXBlocks * bytesPerBlock;
	const uint32_t packedBytesPerLayer = sizeXBlocks * sizeYBlocks * bytesPerBlock;

	EG_ASSERT(packedData.size() >= range.sizeZ * packedBytesPerLayer);

	const uint32_t strideAlignment = GetGraphicsDeviceInfo().textureBufferCopyStrideAlignment;
	const uint32_t bytesPerRow = RoundToNextMultiple(packedBytesPerRow, std::lcm(strideAlignment, bytesPerBlock));
	const uint32_t bytesPerLayer = bytesPerRow * sizeYBlocks;

	const uint32_t bufferSize = bytesPerLayer * range.sizeZ;

	m_bytesPerRow = bytesPerRow;
	m_stagingBuffer = loadContext.AllocateStagingBuffer(bufferSize);

	if (packedBytesPerRow == bytesPerRow && packedBytesPerLayer == bytesPerLayer)
	{
		std::memcpy(m_stagingBuffer.memory.data(), packedData.data(), range.sizeZ * packedBytesPerLayer);
	}
	else
	{
		for (uint32_t z = 0; z < range.sizeZ; z++)
		{
			for (uint32_t y = 0; y < sizeYBlocks; y++)
			{
				const uint32_t inputOffset = packedBytesPerRow * y + packedBytesPerLayer * z;
				const uint32_t outputOffset = bytesPerRow * y + bytesPerLayer * z;
				std::memcpy(
					m_stagingBuffer.memory.data() + outputOffset, packedData.data() + inputOffset, packedBytesPerRow);
			}
		}
	}
}

void TextureUploadBuffer::CopyToTexture(CommandContext& cc, TextureRef texture) const
{
	m_stagingBuffer.Flush();

	cc.CopyBufferToTexture(
		texture, m_range, m_stagingBuffer.buffer,
		TextureBufferCopyLayout{
			.offset = UnsignedNarrow<uint32_t>(m_stagingBuffer.bufferOffset),
			.rowByteStride = m_bytesPerRow,
		});
}

void TextureUploadBuffer::CopyToTextureWithBarriers(
	CommandContext& cc, TextureRef texture, TextureUsage oldUsage, TextureUsage newUsage) const
{
	eg::TextureBarrier preCopyBarrier = {
		.oldUsage = oldUsage,
		.newUsage = eg::TextureUsage::CopyDst,
		.oldAccess = ShaderAccessFlags::All,
	};
	cc.Barrier(texture, preCopyBarrier);

	CopyToTexture(cc, texture);

	eg::TextureBarrier postCopyBarrier = {
		.oldUsage = eg::TextureUsage::CopyDst,
		.newUsage = newUsage,
		.newAccess = ShaderAccessFlags::All,
	};
	cc.Barrier(texture, postCopyBarrier);
}
} // namespace eg
