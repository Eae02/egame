#include "SpriteFont.hpp"
#include "../Utils.hpp"
#include "../Platform/FontConfig.hpp"

#include <fstream>
#include <charconv>
#include <utf8.h>

namespace eg
{
	SpriteFont::SpriteFont(FontAtlas atlas)
		: FontAtlas(std::move(atlas))
	{
		SamplerDescription samplerDescription;
		
		Texture2DCreateInfo texCreateInfo;
		texCreateInfo.flags = TextureFlags::CopyDst | TextureFlags::ShaderSample;
		texCreateInfo.width = AtlasWidth();
		texCreateInfo.height = AtlasHeight();
		texCreateInfo.mipLevels = 1;
		texCreateInfo.defaultSamplerDescription = &samplerDescription;
		texCreateInfo.format = Format::R8_UNorm;
		texCreateInfo.swizzleR = SwizzleMode::One;
		texCreateInfo.swizzleG = SwizzleMode::One;
		texCreateInfo.swizzleB = SwizzleMode::One;
		texCreateInfo.swizzleA = SwizzleMode::R;
		m_texture = Texture::Create2D(texCreateInfo);
		
		const size_t uploadBytes = AtlasWidth() * AtlasHeight();
		Buffer uploadBuffer(BufferFlags::CopySrc | BufferFlags::MapWrite, uploadBytes, nullptr);
		void* uploadMem = uploadBuffer.Map(0, uploadBytes);
		std::memcpy(uploadMem, AtlasData(), uploadBytes);
		uploadBuffer.Flush(0, uploadBytes);
		
		TextureRange textureRange = { };
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
		
		const char* devFontNames[] =
		{
			"Source Code Pro",
			"Ubuntu Mono",
			"Droid Sans Mono",
			"DejaVu Sans Mono",
			"Consolas"
		};
		
		for (const char* devFontName : devFontNames)
		{
			std::string fontPath = GetFontPathByName(devFontName);
			if (!fontPath.empty())
			{
				GlyphRange ranges[] = 
				{
					GlyphRange::ASCII,
					GlyphRange::LatinSupplement,
					GlyphRange::LatinExtended
				};
				
				if (std::optional<FontAtlas> atlas = FontAtlas::Render(fontPath, 14, ranges))
				{
					s_devFont = std::make_unique<SpriteFont>(std::move(*atlas));
					Log(LogLevel::Info, "fnt", "Loaded dev font '{0}' (atlas size: {1}x{2})", fontPath,
						s_devFont->AtlasWidth(), s_devFont->AtlasHeight());
				}
				return;
			}
		}
		
		Log(LogLevel::Warning, "fnt", "Dev font failed to load: no suitable font was found");
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
}
