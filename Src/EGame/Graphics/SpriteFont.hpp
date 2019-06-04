#pragma once

#include "AbstractionHL.hpp"
#include "FontAtlas.hpp"

namespace eg
{
	class EG_API SpriteFont : public FontAtlas
	{
	public:
		explicit SpriteFont(FontAtlas atlas);
		
		const Texture& Tex() const
		{
			return m_texture;
		}
		
		static void LoadDevFont();
		static void UnloadDevFont();
		
		static bool IsDevFontLoaded();
		
		static const SpriteFont& DevFont();
		
	private:
		Texture m_texture;
	};
}
