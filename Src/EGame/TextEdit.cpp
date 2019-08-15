#include "TextEdit.hpp"

#include <utf8.h>

namespace eg
{
	void TextEdit::InsertText(std::string_view text)
	{
		m_data.insert(m_data.begin() + m_cursorPos, text.begin(), text.end());
		m_cursorPos += text.size();
		m_cursorBlinkProgress = 0;
	}
	
	void TextEdit::Update(float dt, bool enabled)
	{
		constexpr float BLINK_TIME = 0.3f;
		m_cursorBlinkProgress = std::fmod(m_cursorBlinkProgress + dt / BLINK_TIME, 2.0f);
		
		if (enabled && !InputtedText().empty())
		{
			InsertText(InputtedText());
		}
		
		m_buttonEventListener.ProcessAll([&] (const ButtonEvent& event)
		{
			if (!enabled || !event.newState)
				return;
			
			auto StepBack = [&]
			{
				auto it = m_data.begin() + m_cursorPos;
				utf8::prior(it, m_data.begin());
				m_cursorPos = (int)(it - m_data.begin());
			};
			
			auto StepForward = [&]
			{
				auto it = m_data.begin() + m_cursorPos;
				utf8::next(it, m_data.end());
				m_cursorPos = (int)(it - m_data.begin());
			};
			
			switch (event.button)
			{
			case eg::Button::LeftArrow:
				if (m_cursorPos > 0)
				{
					StepBack();
					if (InputState::Current().IsCtrlDown())
					{
						while (m_cursorPos > 0 && m_data[m_cursorPos - 1] != ' ')
							StepBack();
					}
					m_cursorBlinkProgress = 0;
				}
				break;
			case eg::Button::RightArrow:
				if (m_cursorPos < (int)m_data.size())
				{
					StepForward();
					if (InputState::Current().IsCtrlDown())
					{
						while (m_cursorPos < (int)m_data.size() && m_data[m_cursorPos] != ' ')
							StepForward();
					}
					m_cursorBlinkProgress = 0;
				}
				break;
			case Button::Backspace:
				if (m_cursorPos > 0)
				{
					int oldCursorPos = m_cursorPos;
					StepBack();
					m_data.erase(m_data.begin() + m_cursorPos, m_data.begin() + oldCursorPos);
					m_cursorBlinkProgress = 0;
				}
				break;
			case Button::Delete:
				if (m_cursorPos < (int)m_data.size())
				{
					auto first = m_data.begin() + m_cursorPos;
					auto last = first;
					utf8::next(last, m_data.end());
					m_data.erase(first, last);
					m_cursorBlinkProgress = 0;
				}
				break;
			case Button::Home:
				m_cursorPos = 0;
				m_cursorBlinkProgress = 0;
				break;
			case Button::End:
				m_cursorPos = (int)m_data.size();
				m_cursorBlinkProgress = 0;
				break;
			default:
				break;
			}
		});
	}
	
	void TextEdit::Draw(const glm::vec2& position, SpriteBatch& spriteBatch, const ColorLin& color) const
	{
		spriteBatch.DrawText(*m_font, Text(), position, color, m_fontScale, nullptr);
		
		if (m_cursorBlinkProgress < 1)
		{
			const float CURSOR_EXTRA_H = 2;
			const int cursorX = (int)(position.x + m_font->GetTextExtents(Text().substr(0, (size_t)m_cursorPos)).x * m_fontScale);
			spriteBatch.DrawLine(glm::vec2(cursorX, position.y - CURSOR_EXTRA_H),
				glm::vec2(cursorX, position.y + m_font->Size() * m_fontScale + CURSOR_EXTRA_H), color);
		}
	}
}
