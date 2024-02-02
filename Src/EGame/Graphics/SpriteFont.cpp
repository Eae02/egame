#include "SpriteFont.hpp"
#include "../../Assets/DevFont.fnt.h"
#include "../../Assets/DevFont.png.h"
#include "../Assert.hpp"
#include "../Log.hpp"
#include "../Platform/FontConfig.hpp"

#include <fstream>
#include <utf8.h>

namespace eg
{
SpriteFont::SpriteFont(FontAtlas atlas) : FontAtlas(std::move(atlas))
{
	SamplerDescription samplerDescription;

	TextureCreateInfo texCreateInfo;
	texCreateInfo.flags = TextureFlags::CopyDst | TextureFlags::ShaderSample;
	texCreateInfo.width = AtlasWidth();
	texCreateInfo.height = AtlasHeight();
	texCreateInfo.mipLevels = 1;
	texCreateInfo.defaultSamplerDescription = &samplerDescription;
	texCreateInfo.format = Format::R8_UNorm;
	m_texture = Texture::Create2D(texCreateInfo);

	const size_t uploadBytes = AtlasWidth() * AtlasHeight();
	Buffer uploadBuffer(BufferFlags::CopySrc | BufferFlags::MapWrite | BufferFlags::HostAllocate, uploadBytes, nullptr);
	void* uploadMem = uploadBuffer.Map(0, uploadBytes);
	std::memcpy(uploadMem, AtlasData(), uploadBytes);
	uploadBuffer.Flush(0, uploadBytes);

	TextureRange textureRange = {};
	textureRange.sizeX = AtlasWidth();
	textureRange.sizeY = AtlasHeight();
	textureRange.sizeZ = 1;
	DC.SetTextureData(m_texture, textureRange, uploadBuffer, 0);

	m_texture.UsageHint(TextureUsage::ShaderSample, ShaderAccessFlags::Fragment);

	FreeAtlasData();
}

static std::unique_ptr<SpriteFont> s_devFont;

void SpriteFont::LoadDevFont()
{
	if (s_devFont != nullptr)
		return;

	if (std::optional<FontAtlas> atlas = FontAtlas::FromFNTMemory(
			std::span<const char>(reinterpret_cast<const char*>(DevFont_fnt), DevFont_fnt_len),
			std::span<const char>(reinterpret_cast<const char*>(DevFont_png), DevFont_png_len)))
	{
		s_devFont = std::make_unique<SpriteFont>(std::move(*atlas));
	}
	else
	{
		Log(LogLevel::Warning, "fnt", "Dev font failed to load");
	}
}

bool SpriteFont::IsDevFontLoaded()
{
	return s_devFont != nullptr;
}

void SpriteFont::UnloadDevFont()
{
	s_devFont.reset();
}

const SpriteFont& SpriteFont::DevFont()
{
	if (s_devFont == nullptr)
		EG_PANIC("Dev font is not loaded");
	return *s_devFont;
}
} // namespace eg
