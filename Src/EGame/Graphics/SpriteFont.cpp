#include "SpriteFont.hpp"
#include "../../Assets/DevFont.fnt.h"
#include "../../Assets/DevFont.png.h"
#include "../Assert.hpp"
#include "../Core.hpp"
#include "../Log.hpp"
#include "../Platform/FontConfig.hpp"

#include <utf8.h>

namespace eg
{
SpriteFont::SpriteFont(FontAtlas atlas) : FontAtlas(std::move(atlas))
{
	TextureCreateInfo texCreateInfo;
	texCreateInfo.flags = TextureFlags::CopyDst | TextureFlags::ShaderSample;
	texCreateInfo.width = AtlasWidth();
	texCreateInfo.height = AtlasHeight();
	texCreateInfo.mipLevels = 1;
	texCreateInfo.format = Format::R8_UNorm;
	m_texture = Texture::Create2D(texCreateInfo);

	const size_t uploadBytes = AtlasWidth() * AtlasHeight();
	Buffer uploadBuffer(BufferFlags::CopySrc | BufferFlags::MapWrite, uploadBytes, nullptr);
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

static std::string devFontNames[] = {
	"Source Code Pro",
	"Consolas",
	"DejaVu Mono",
	"SF Mono",
};

void SpriteFont::LoadDevFont()
{
	if (s_devFont != nullptr)
		return;

	uint32_t devFontSize = static_cast<uint32_t>(std::round(14.0f * DisplayScaleFactor()));

	for (const std::string& devFontName : devFontNames)
	{
		std::string path = GetFontPathByName(devFontName);
		if (!path.empty())
		{
			if (std::optional<FontAtlas> atlas = FontAtlas::Render(path, devFontSize, { &GlyphRange::ASCII, 1 }))
			{
				eg::Log(
					eg::LogLevel::Info, "fnt", "Rendered dev font from '{0}' ({1}) at size {2}", devFontName, path,
					devFontSize);
				s_devFont = std::make_unique<SpriteFont>(std::move(*atlas));
				return;
			}
		}
	}

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
