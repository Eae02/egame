#include "AbstractionHL.hpp"
#include "../Alloc/PoolAllocator.hpp"
#include "../Assert.hpp"
#include "../Core.hpp"
#include "../Hash.hpp"
#include "../IOUtils.hpp"
#include "ImageLoader.hpp"
#include "SpirvCrossUtils.hpp"

#include <fstream>
#include <mutex>
#include <numeric>
#include <sstream>
#include <unordered_map>

namespace eg
{
CommandContext DC;

const BlendState AlphaBlend{ BlendFunc::Add, BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha };

GraphicsDeviceInfo detail::graphicsDeviceInfo;

ShaderModule ShaderModule::CreateFromFile(const std::string& path)
{
	ShaderStage stage;
	if (path.ends_with(".fs.spv") || path.ends_with(".frag.spv"))
		stage = ShaderStage::Fragment;
	else if (path.ends_with(".vs.spv") || path.ends_with(".vert.spv"))
		stage = ShaderStage::Vertex;
	else
		EG_PANIC("Unrecognized shader stage file extension in '" << path << "'.");

	std::ifstream stream(path, std::ios::binary);
	if (!stream)
	{
		EG_PANIC("Error opening shader file for reading: " << path);
	}

	std::vector<char> code = ReadStreamContents(stream);
	return ShaderModule(stage, code);
}

static Texture whitePixelTexture;
static Texture blackPixelTexture;

const Texture& Texture::WhitePixel()
{
	if (whitePixelTexture.handle != nullptr)
		return whitePixelTexture;

	whitePixelTexture = Texture::Create2D({
		.flags = TextureFlags::ShaderSample | TextureFlags::CopyDst,
		.mipLevels = 1,
		.width = 1,
		.height = 1,
		.format = Format::R8G8B8A8_UNorm,
	});

	char whitePixelData[4];
	std::fill_n(whitePixelData, 4, static_cast<char>(0xFF));
	whitePixelTexture.SetData(whitePixelData, whitePixelTexture.WholeRange());

	whitePixelTexture.UsageHint(TextureUsage::ShaderSample, ShaderAccessFlags::Fragment | ShaderAccessFlags::Vertex);

	return whitePixelTexture;
}

const Texture& Texture::BlackPixel()
{
	if (blackPixelTexture.handle != nullptr)
		return blackPixelTexture;

	blackPixelTexture = Texture::Create2D({
		.flags = TextureFlags::ShaderSample | TextureFlags::CopyDst,
		.mipLevels = 1,
		.width = 1,
		.height = 1,
		.format = Format::R8G8B8A8_UNorm,
	});

	char blackPixelData[4];
	std::fill_n(blackPixelData, 4, static_cast<char>(0x0));
	blackPixelTexture.SetData(blackPixelData, blackPixelTexture.WholeRange());

	blackPixelTexture.UsageHint(TextureUsage::ShaderSample, ShaderAccessFlags::Fragment | ShaderAccessFlags::Vertex);

	return blackPixelTexture;
}

void detail::DestroyPixelTextures()
{
	whitePixelTexture.Destroy();
	blackPixelTexture.Destroy();
}

Texture Texture::Load(std::istream& stream, LoadFormat format, uint32_t mipLevels, CommandContext* commandContext)
{
	if (!stream)
		return Texture();

	if (commandContext == nullptr)
		commandContext = &DC;

	ImageLoader loader(stream);

	TextureCreateInfo createInfo;
	createInfo.width = ToUnsigned(loader.Width());
	createInfo.height = ToUnsigned(loader.Height());
	createInfo.mipLevels = mipLevels == 0 ? MaxMipLevels(std::max(createInfo.width, createInfo.height)) : mipLevels;

	switch (format)
	{
	case LoadFormat::R_UNorm: createInfo.format = Format::R8_UNorm; break;
	case LoadFormat::RGBA_UNorm: createInfo.format = Format::R8G8B8A8_UNorm; break;
	case LoadFormat::RGBA_sRGB: createInfo.format = Format::R8G8B8A8_sRGB; break;
	}

	auto data = loader.Load(format == LoadFormat::R_UNorm ? 1 : 4);
	if (data == nullptr)
		return Texture();

	const TextureRange range = { 0, 0, 0, createInfo.width, createInfo.height, 1, 0 };

	uint32_t imageBytes = GetImageByteSize(createInfo.width, createInfo.height, createInfo.format);

	Texture texture = Create2D(createInfo);
	texture.SetData({ reinterpret_cast<char*>(data.get()), imageBytes }, range, commandContext);

	return texture;
}

void Texture::SetData(std::span<const char> packedData, const TextureRange& range, CommandContext* commandContext)
{
	const uint32_t blockSize = GetFormatBlockWidth(m_format);
	EG_ASSERT((range.offsetX % blockSize) == 0);
	EG_ASSERT((range.offsetY % blockSize) == 0);

	const uint32_t sizeXBlocks = (range.sizeX + blockSize - 1) / blockSize;
	const uint32_t sizeYBlocks = (range.sizeY + blockSize - 1) / blockSize;

	const uint32_t bytesPerBlock = GetFormatBytesPerBlock(m_format);

	const uint32_t packedBytesPerRow = sizeXBlocks * bytesPerBlock;
	const uint32_t packedBytesPerLayer = sizeXBlocks * sizeYBlocks * bytesPerBlock;

	EG_ASSERT(packedData.size() >= range.sizeZ * packedBytesPerLayer);

	const uint32_t strideAlignment = GetGraphicsDeviceInfo().textureBufferCopyStrideAlignment;
	const uint32_t bytesPerRow = RoundToNextMultiple(packedBytesPerRow, std::lcm(strideAlignment, bytesPerBlock));
	const uint32_t bytesPerLayer = RoundToNextMultiple(bytesPerRow * range.sizeY, strideAlignment);

	Buffer dedicatedBuffer;

	const uint32_t bufferSize = bytesPerLayer * range.sizeZ;

	UploadBuffer uploadBuffer;
	if (commandContext == nullptr && bufferSize < 4 * 1024)
	{
		uploadBuffer = GetTemporaryUploadBuffer(bufferSize);
	}
	else
	{
		dedicatedBuffer =
			Buffer(BufferFlags::CopySrc | BufferFlags::MapWrite | eg::BufferFlags::ManualBarrier, bufferSize, nullptr);
		uploadBuffer.buffer = dedicatedBuffer;
		uploadBuffer.offset = 0;
		uploadBuffer.range = bufferSize;
	}

	char* uploadBufferMem = static_cast<char*>(uploadBuffer.Map());
	if (packedBytesPerRow == bytesPerRow && packedBytesPerLayer == bytesPerLayer)
	{
		std::memcpy(uploadBufferMem, packedData.data(), range.sizeZ * packedBytesPerLayer);
	}
	else
	{
		for (uint32_t z = 0; z < range.sizeZ; z++)
		{
			for (uint32_t y = 0; y < sizeYBlocks; y++)
			{
				const uint32_t inputOffset = packedBytesPerRow * y + packedBytesPerLayer * z;
				const uint32_t outputOffset = bytesPerRow * y + bytesPerLayer * z;
				std::memcpy(uploadBufferMem + outputOffset, packedData.data() + inputOffset, packedBytesPerRow);
			}
		}
	}

	uploadBuffer.Flush();

	(commandContext ? commandContext : &DC)
		->CopyBufferToTexture(
			*this, range, uploadBuffer.buffer,
			TextureBufferCopyLayout{
				.offset = static_cast<uint32_t>(uploadBuffer.offset),
				.rowByteStride = bytesPerRow,
				.layerByteStride = bytesPerLayer,
			});
}

void TextureRef::UsageHint(TextureUsage usage, ShaderAccessFlags shaderAccessFlags)
{
	if (usage == TextureUsage::ShaderSample && shaderAccessFlags == ShaderAccessFlags::None)
		EG_PANIC("shaderAccessFlags set to None, but not allowed by usage.");
	gal::TextureUsageHint(handle, usage, shaderAccessFlags);
}

static constexpr uint64_t MIN_BUFFER_SIZE = 4 * 1024 * 1024; // 4MiB

struct UploadBufferEntry
{
	uint64_t lastUsedFrame;
	uint64_t size;
	uint64_t offset;
	Buffer buffer;

	explicit UploadBufferEntry(uint64_t _size) : size(_size), offset(0)
	{
		BufferCreateInfo createInfo;
		createInfo.flags = BufferFlags::MapWrite | BufferFlags::CopySrc;
		createInfo.size = _size;
		createInfo.label = "UploadBuffer";
		buffer = Buffer(createInfo);
	}
};

static std::vector<UploadBufferEntry> uploadBuffers;

UploadBuffer GetTemporaryUploadBuffer(uint64_t size, uint64_t alignment)
{
	alignment = std::max<uint64_t>(alignment, 16);

	UploadBufferEntry* selected = nullptr;
	for (UploadBufferEntry& buffer : uploadBuffers)
	{
		if (buffer.lastUsedFrame == UINT64_MAX || buffer.lastUsedFrame == FrameIdx() ||
		    buffer.lastUsedFrame + MAX_CONCURRENT_FRAMES <= FrameIdx())
		{
			uint64_t newOffset = RoundToNextMultiple(buffer.offset, alignment) + size;
			if (buffer.lastUsedFrame != FrameIdx() || newOffset <= buffer.size)
			{
				selected = &buffer;
				break;
			}
		}
	}

	if (selected == nullptr)
	{
		uint64_t allocSize = std::max(RoundToNextMultiple<uint64_t>(size, 1024 * 1024), MIN_BUFFER_SIZE);
		selected = &uploadBuffers.emplace_back(allocSize);

		Log(LogLevel::Info, "gfx", "Created upload buffer with size {0}.", ReadableBytesSize(allocSize));
	}
	else if (selected->lastUsedFrame != FrameIdx())
	{
		selected->offset = 0;
	}

	UploadBuffer ret;
	ret.buffer = selected->buffer;
	ret.offset = RoundToNextMultiple(selected->offset, alignment);
	ret.range = size;

	selected->offset = ret.offset + size;
	selected->lastUsedFrame = FrameIdx();

	return ret;
}

void MarkUploadBuffersAvailable()
{
	for (UploadBufferEntry& buffer : uploadBuffers)
		buffer.lastUsedFrame = UINT64_MAX;
}

void DestroyUploadBuffers()
{
	uploadBuffers.clear();
}

void AssertFormatSupport(Format format, FormatCapabilities capabilities)
{
	FormatCapabilities supportedCapabilities = gal::GetFormatCapabilities(format);
	if ((capabilities & supportedCapabilities) != capabilities)
	{
		std::ostringstream messageStream;
		messageStream << "Your graphics card (" << detail::graphicsDeviceInfo.deviceName << ") is not supported\n";
		messageStream << "Required capabilities are not available for " << FormatToString(format) << ":";
		for (uint32_t i = 0; i < std::size(FormatCapabilityNames); i++)
		{
			FormatCapabilities mask = static_cast<FormatCapabilities>(1U << i);
			if (HasFlag(capabilities, mask) && !HasFlag(supportedCapabilities, mask))
				messageStream << " " << FormatCapabilityNames.at(i);
		}
		detail::PanicImpl(messageStream.str());
	}
}

void BufferRef::DCUpdateData(uint64_t offset, uint64_t size, const void* data)
{
	UploadBuffer uploadBuffer = GetTemporaryUploadBuffer(size);
	std::memcpy(uploadBuffer.Map(), data, size);
	uploadBuffer.Flush();

	DC.CopyBuffer(uploadBuffer.buffer, *this, uploadBuffer.offset, offset, size);
}

DescriptorSetRef Texture::GetFragmentShaderSampleDescriptorSet(eg::BindingTypeTexture bindingTexture) const
{
	if (!m_fragmentShaderSampleDescriptorSet.has_value())
	{
		const DescriptorSetBinding binding = {
			.binding = 0,
			.type = bindingTexture,
			.shaderAccess = ShaderAccessFlags::Fragment,
		};

		m_fragmentShaderSampleDescriptorSet = DescriptorSet({ &binding, 1 });
		m_fragmentShaderSampleDescriptorSet->BindTexture(*this, 0);
	}
	return *m_fragmentShaderSampleDescriptorSet;
}

ShaderModule::ShaderModule(ShaderStage stage, std::span<const uint32_t> code, const char* label)
	: m_parsedIR(ParseSpirV(code)), m_handle(gal::CreateShaderModule(stage, *m_parsedIR, label))
{
}

static std::mutex samplersTableMutex;
static std::unordered_map<SamplerDescription, SamplerHandle, MemberFunctionHash<SamplerDescription>> samplersTable;

SamplerHandle GetSampler(const SamplerDescription& description)
{
	std::lock_guard<std::mutex> lock(samplersTableMutex);

	auto it = samplersTable.find(description);
	if (it != samplersTable.end())
		return it->second;

	SamplerHandle sampler = gal::CreateSampler(description);
	samplersTable.emplace(description, sampler);
	return sampler;
}

void CommandContext::CopyBuffer(BufferRef src, BufferRef dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size)
{
	EG_ASSERT((srcOffset % BUFFER_BUFFER_COPY_OFFSET_ALIGNMENT) == 0);
	EG_ASSERT((dstOffset % BUFFER_BUFFER_COPY_OFFSET_ALIGNMENT) == 0);
	EG_ASSERT((size % BUFFER_BUFFER_COPY_SIZE_ALIGNMENT) == 0);
	gal::CopyBuffer(Handle(), src.handle, dst.handle, srcOffset, dstOffset, size);
}

void CommandContext::CopyTextureToBuffer(
	TextureRef texture, const TextureRange& range, BufferRef buffer, const TextureBufferCopyLayout& copyLayout)
{
	EG_ASSERT((copyLayout.offset % BUFFER_TEXTURE_COPY_OFFSET_ALIGNMENT) == 0);
	gal::CopyTextureToBuffer(Handle(), texture.handle, range, buffer.handle, copyLayout);
}

void CommandContext::CopyBufferToTexture(
	TextureRef texture, const TextureRange& range, BufferRef buffer, const TextureBufferCopyLayout& copyLayout)
{
	EG_ASSERT((copyLayout.offset % BUFFER_TEXTURE_COPY_OFFSET_ALIGNMENT) == 0);
	gal::CopyBufferToTexture(Handle(), texture.handle, range, buffer.handle, copyLayout);
}
} // namespace eg
