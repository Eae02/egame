#pragma once

#include "Graphics/SpriteFont.hpp"
#include "Graphics/SpriteBatch.hpp"
#include "InputState.hpp"
#include "API.hpp"
#include "Event.hpp"

namespace eg
{
	class EG_API TextEdit
	{
	public:
		TextEdit()
			: m_font(nullptr) { }
		explicit TextEdit(const SpriteFont& font)
			: m_font(&font) { }
		
		void Update(float dt, bool enabled = true);
		
		void Draw(const glm::vec2& position, SpriteBatch& spriteBatch, const ColorLin& color) const;
		
		void Clear()
		{
			m_data.clear();
			m_cursorPos = 0;
		}
		
		void InsertText(std::string_view text);
		
		std::string_view Text() const
		{
			return { m_data.data(), m_data.size() };
		}
		
		const SpriteFont* Font() const
		{
			return m_font;
		}
		
		void SetFont(const SpriteFont* font)
		{
			m_font = font;
		}
		
		void SetFontScale(float fontScale)
		{
			m_fontScale = fontScale;
		}
		
		float FontScale() const
		{
			return m_fontScale;
		}
		
		int CursorPos() const
		{
			return m_cursorPos;
		}
		
		float TextWidth() const
		{
			return m_font->GetTextExtents(Text()).x * m_fontScale;
		}
		
	private:
		const SpriteFont* m_font;
		float m_fontScale = 1;
		
		EventListener<ButtonEvent> m_buttonEventListener;
		
		float m_cursorBlinkProgress = 0;
		
		std::vector<char> m_data;
		int m_cursorPos = 0;
	};
}
