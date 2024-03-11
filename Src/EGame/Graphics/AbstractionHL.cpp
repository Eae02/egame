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

	size_t dataBytes = loader.Width() * loader.Height() * (format == LoadFormat::R_UNorm ? 1 : 4);
	Buffer uploadBuffer(BufferFlags::CopySrc | BufferFlags::MapWrite, dataBytes, nullptr);

	void* uploadBufferMem = uploadBuffer.Map(0, dataBytes);
	std::memcpy(uploadBufferMem, data.get(), dataBytes);
	uploadBuffer.Flush(0, dataBytes);

	const TextureRange range = { 0, 0, 0, createInfo.width, createInfo.height, 1, 0 };

	Texture texture = Create2D(createInfo);
	commandContext->SetTextureData(texture, range, uploadBuffer, 0);

	return texture;
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

void TextureRef::DCUpdateData(const TextureRange& range, size_t dataLen, const void* data)
{
	UploadBuffer uploadBuffer = GetTemporaryUploadBuffer(dataLen);
	std::memcpy(uploadBuffer.Map(), data, dataLen);
	uploadBuffer.Flush();

	DC.SetTextureData(*this, range, uploadBuffer.buffer, uploadBuffer.offset);
}

ShaderModule::ShaderModule(ShaderStage stage, std::span<const uint32_t> code, const char* label)
	: m_parsedIR(ParseSpirV(code)), m_handle(gal::CreateShaderModule(stage, *m_parsedIR, label))
{
}

static std::mutex samplersTableMutex;
static std::unordered_map<SamplerDescription, SamplerHandle, MemberFunctionHash<SamplerDescription>> samplersTable;

Sampler::Sampler(const SamplerDescription& description)
{
	std::lock_guard<std::mutex> lock(samplersTableMutex);

	auto it = samplersTable.find(description);
	if (it == samplersTable.end())
	{
		m_handle = gal::CreateSampler(description);
		samplersTable.emplace(description, m_handle);
	}
	else
	{
		m_handle = it->second;
	}
}
} // namespace eg
