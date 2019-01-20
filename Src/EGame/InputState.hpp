#pragma once

#include <atomic>
#include <bitset>

#include "API.hpp"

namespace eg
{
	enum class Button
	{
		Unknown,
		MouseLeft,
		MouseRight,
		MouseMiddle,
		MouseSide1,
		MouseSide2,
		LeftShift,
		RightShift,
		LeftControl,
		RightControl,
		LeftAlt,
		RightAlt,
		Escape,
		Enter,
		Space,
		Tab,
		Backspace,
		LeftArrow,
		UpArrow,
		RightArrow,
		DownArrow,
		PageUp,
		PageDown,
		Home,
		End,
		Delete,
		D0,
		D1,
		D2,
		D3,
		D4,
		D5,
		D6,
		D7,
		D8,
		D9,
		A,
		B,
		C,
		D,
		E,
		F,
		G,
		H,
		I,
		J,
		K,
		L,
		M,
		N,
		O,
		P,
		Q,
		R,
		S,
		T,
		U,
		V,
		W,
		X,
		Y,
		Z,
		F1,
		F2,
		F3,
		F4,
		F5,
		F6,
		F7,
		F8,
		F9,
		F10,
		F11,
		F12,
		F13,
		F14,
		F15,
		F16,
		F17,
		F18,
		F19,
		F20,
		F21,
		F22,
		F23,
		F24,
		NUM_BUTTONS
	};
	
	EG_API std::string_view ButtonToString(Button button);
	EG_API Button ButtonFromString(std::string_view str);
	EG_API std::string_view ButtonDisplayName(Button button);
	
	constexpr size_t NUM_BUTTONS = (size_t)Button::NUM_BUTTONS;
	
	class InputState;
	
	namespace detail
	{
		extern EG_API InputState* currentIS;
		extern EG_API InputState* previousIS;
	}
	
	class EG_API InputState
	{
	public:
		InputState() = default;
		
		Button PressedButton() const
		{
			return m_pressed;
		}
		
		bool IsButtonDown(Button button) const
		{
			return (m_isButtonDown[(int)button / 8] & (1 << ((int)button % 8))) != 0;
		}
		
		void OnButtonDown(Button button)
		{
			m_isButtonDown[(int)button / 8] |= 1 << ((int)button % 8);
			m_pressed = button;
		}
		
		void OnButtonUp(Button button)
		{
			if (button == m_pressed)
				m_pressed = Button::Unknown;
			m_isButtonDown[(int)button / 8] &= ~(1 << ((int)button % 8));
		}
		
		glm::ivec2 CursorPos() const
		{
			return { cursorX, cursorY };
		}
		
		glm::ivec2 CursorPosDelta() const
		{
			return { cursorDeltaX, cursorDeltaY };
		}
		
		glm::ivec2 ScrollPos() const
		{
			return { scrollX, scrollY };
		}
		
		static const InputState& Current()
		{
			return *detail::currentIS;
		}
		
		static const InputState& Previous()
		{
			return *detail::previousIS;
		}
		
		int32_t scrollX = 0;
		int32_t scrollY = 0;
		int32_t cursorX = 0;
		int32_t cursorY = 0;
		int32_t cursorDeltaX = 0;
		int32_t cursorDeltaY = 0;
		
	private:
		Button m_pressed = Button::Unknown;
		char m_isButtonDown[12];
		
		static_assert(sizeof(m_isButtonDown) * 8 >= NUM_BUTTONS);
	};
	
	EG_API void SetRelativeMouseMode(bool relMouseMode);
	EG_API bool RelativeMouseModeActive();
	
	inline Button PressedButton()
	{
		return detail::currentIS->PressedButton();
	}
	
	inline bool IsButtonDown(Button button)
	{
		return detail::currentIS->IsButtonDown(button);
	}
	
	inline bool WasButtonDown(Button button)
	{
		return detail::previousIS->IsButtonDown(button);
	}
	
	inline glm::ivec2 CursorPos()
	{
		return detail::currentIS->CursorPos();
	}
	
	inline int32_t CursorX()
	{
		return detail::currentIS->cursorX;
	}
	
	inline int32_t CursorY()
	{
		return detail::currentIS->cursorY;
	}
	
	inline int32_t CursorDeltaX()
	{
		return detail::currentIS->cursorDeltaX;
	}
	
	inline int32_t CursorDeltaY()
	{
		return detail::currentIS->cursorDeltaY;
	}
	
	inline glm::ivec2 CursorPosDelta()
	{
		return detail::currentIS->CursorPosDelta();
	}
	
	inline glm::ivec2 PrevCursorPos()
	{
		return detail::previousIS->CursorPos();
	}
	
	inline int32_t PrevCursorX()
	{
		return detail::previousIS->cursorX;
	}
	
	inline int32_t PrevCursorY()
	{
		return detail::previousIS->cursorY;
	}
}
