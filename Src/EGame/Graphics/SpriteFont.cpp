#include "SpriteFont.hpp"
#include "../Utils.hpp"
#include "../Platform/FontConfig.hpp"

#include <fstream>
#include <utf8.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/fetch.h>
#endif

namespace eg
{
	SpriteFont::SpriteFont(FontAtlas atlas)
		: FontAtlas(std::move(atlas))
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
		
		TextureRange textureRange = { };
		textureRange.sizeX = AtlasWidth();
		textureRange.sizeY = AtlasHeight();
		textureRange.sizeZ = 1;
		DC.SetTextureData(m_texture, textureRange, uploadBuffer, 0);
		
		m_texture.UsageHint(TextureUsage::ShaderSample, ShaderAccessFlags::Fragment);
		
		FreeAtlasData();
	}
	
	static std::unique_ptr<SpriteFont> s_devFont;
	
	static GlyphRange devFontRanges[] = 
	{
		GlyphRange::ASCII,
		GlyphRange::LatinSupplement,
		GlyphRange::LatinExtended
	};
	
	void SpriteFont::LoadDevFont()
	{
		if (s_devFont != nullptr)
			return;
		
#ifdef __EMSCRIPTEN__
		emscripten_fetch_attr_t attr;
		emscripten_fetch_attr_init(&attr);
		std::strcpy(attr.requestMethod, "GET");
		attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
		attr.onsuccess = [] (emscripten_fetch_t* fetch)
		{
			Span<const char> dataSpan(fetch->data, fetch->numBytes);
			if (std::optional<FontAtlas> atlas = FontAtlas::Render(dataSpan, 14, devFontRanges))
			{
				s_devFont = std::make_unique<SpriteFont>(std::move(*atlas));
				Log(LogLevel::Info, "fnt", "Loaded dev font (atlas size: {0}x{1})",
					s_devFont->AtlasWidth(), s_devFont->AtlasHeight());
			}
			
			emscripten_fetch_close(fetch);
		};
		attr.onerror = [] (emscripten_fetch_t* fetch)
		{
			EG_PANIC("Dev font failed to load");
			emscripten_fetch_close(fetch);
		};
		emscripten_fetch(&attr, "https://fontlibrary.org/assets/fonts/source-code-pro/8733444bf1b52108e4cad8cfcbc40e15/cc35f676db8d665e341971d0c290c03c/SourceCodeProRegular.ttf");
#else
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
				if (std::optional<FontAtlas> atlas = FontAtlas::Render(fontPath, 14, devFontRanges))
				{
					s_devFont = std::make_unique<SpriteFont>(std::move(*atlas));
					Log(LogLevel::Info, "fnt", "Loaded dev font '{0}' (atlas size: {1}x{2})", fontPath,
						s_devFont->AtlasWidth(), s_devFont->AtlasHeight());
				}
				return;
			}
		}
		
		Log(LogLevel::Warning, "fnt", "Dev font failed to load: no suitable font was found");
#endif
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
}
