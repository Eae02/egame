#include "AbstractionHL.hpp"
#include "GraphicsLoadContext.hpp"

namespace eg
{
class TextureUploadBuffer
{
public:
	TextureUploadBuffer(
		std::span<const char> packedData, const TextureRange& range, Format format, GraphicsLoadContext& loadContext);

	void CopyToTexture(CommandContext& cc, TextureRef texture) const;

	void CopyToTextureWithBarriers(
		CommandContext& cc, TextureRef texture, TextureUsage oldUsage, TextureUsage newUsage) const;

private:
	GraphicsLoadContext::StagingBuffer m_stagingBuffer;
	TextureRange m_range;
	uint32_t m_bytesPerRow;
};
} // namespace eg
