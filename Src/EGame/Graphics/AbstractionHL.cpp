#include "AbstractionHL.hpp"
#include "ImageLoader.hpp"
#include "../Alloc/PoolAllocator.hpp"
#include "../Core.hpp"
#include "../Assert.hpp"
#include "../IOUtils.hpp"

#include <sstream>
#include <fstream>

namespace eg
{
	CommandContext DC;
	
	const BlendState AlphaBlend { BlendFunc::Add, BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha };
	
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
		Buffer uploadBuffer(BufferFlags::HostAllocate | BufferFlags::CopySrc | BufferFlags::MapWrite, dataBytes, nullptr);
		
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
	
	static constexpr uint64_t MIN_BUFFER_SIZE = 4 * 1024 * 1024; //4MiB
	
	struct UploadBufferEntry
	{
		uint64_t lastUsedFrame;
		uint64_t size;
		uint64_t offset;
		Buffer buffer;
		
		explicit UploadBufferEntry(uint64_t _size)
			: size(_size), offset(0)
		{
			BufferCreateInfo createInfo;
			createInfo.flags = BufferFlags::MapWrite | BufferFlags::CopySrc | BufferFlags::HostAllocate;
			createInfo.size = _size;
			createInfo.label = "UploadBuffer";
			buffer = Buffer(createInfo);
		}
	};
	
	static std::vector<UploadBufferEntry> uploadBuffers;
	
	UploadBuffer GetTemporaryUploadBuffer(uint64_t size, uint64_t alignment)
	{
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
					messageStream << " " << FormatCapabilityNames[i];
			}
			detail::PanicImpl(messageStream.str());
		}
	}
}
