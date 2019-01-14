#include "AbstractionHL.hpp"
#include "ImageLoader.hpp"
#include "../Core.hpp"
#include "../IOUtils.hpp"

#include <fstream>

namespace eg
{
	CommandContext DC;
	
	const BlendState AlphaBlend { BlendFunc::Add, BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha };
	
	GraphicsCapabilities detail::graphicsCapabilities;
	
	void ShaderProgram::AddStageFromFile(const std::string& path)
	{
		ShaderStage stage;
		if (StringEndsWith(path, ".fs.spv") || StringEndsWith(path, ".frag.spv"))
			stage = ShaderStage::Fragment;
		else if (StringEndsWith(path, ".vs.spv") || StringEndsWith(path, ".vert.spv"))
			stage = ShaderStage::Vertex;
		else
			EG_PANIC("Unrecognized shader stage file extension in '" << path << "'.");
		
		std::ifstream stream(path, std::ios::binary);
		if (!stream)
		{
			EG_PANIC("Error opening shader file for reading: " << path);
		}
		
		std::vector<char> code = ReadStreamContents(stream);
		AddStage(stage, std::move(code));
	}
	
	Texture Texture::Load(std::istream& stream, LoadFormat format, uint32_t mipLevels, CommandContext* commandContext)
	{
		if (!stream)
			return Texture();
		
		if (commandContext == nullptr)
			commandContext = &DC;
		
		ImageLoader loader(stream);
		
		Texture2DCreateInfo createInfo;
		createInfo.width = (uint32_t)loader.Width();
		createInfo.height = (uint32_t)loader.Height();
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
		
		Texture texture = Create2D(createInfo);
		commandContext->SetTextureData(texture, range, data.get());
		
		return texture;
	}
	
	struct UploadBuffer
	{
		uint64_t lastUsedFrame;
		uint64_t size;
		Buffer buffer;
		
		explicit UploadBuffer(uint64_t size)
			: size(size), buffer(BufferUsage::MapWrite | BufferUsage::CopySrc, size, nullptr) { }
	};
	
	static std::vector<UploadBuffer> uploadBuffers;
	
	BufferRef GetTemporaryUploadBuffer(uint64_t size)
	{
		UploadBuffer* selected = nullptr;
		for (UploadBuffer& buffer : uploadBuffers)
		{
			if (buffer.size >= size &&
				(buffer.lastUsedFrame == UINT64_MAX || buffer.lastUsedFrame + MAX_CONCURRENT_FRAMES <= FrameIdx()) &&
				(selected == nullptr || buffer.size < selected->size))
			{
				selected = &buffer;
			}
		}
		
		if (selected == nullptr)
		{
			selected = &uploadBuffers.emplace_back(RoundToNextMultiple<uint64_t>(size, 1024 * 1024));
		}
		
		selected->lastUsedFrame = FrameIdx();
		return selected->buffer;
	}
	
	void MarkUploadBuffersAvailable()
	{
		for (UploadBuffer& buffer : uploadBuffers)
			buffer.lastUsedFrame = UINT64_MAX;
	}
	
	void DestroyUploadBuffers()
	{
		uploadBuffers.clear();
	}
}
