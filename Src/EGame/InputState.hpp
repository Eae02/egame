#pragma once

#include <atomic>
#include <bitset>
#include <cstdint>
#include <optional>
#include <string_view>

#include "API.hpp"
#include "Geometry/Rectangle.hpp"

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
	CtrlrA,
	CtrlrB,
	CtrlrX,
	CtrlrY,
	CtrlrBack,
	CtrlrGuide,
	CtrlrStart,
	CtrlrLeftStick,
	CtrlrRightStick,
	CtrlrLeftShoulder,
	CtrlrRightShoulder,
	CtrlrDPadUp,
	CtrlrDPadDown,
	CtrlrDPadLeft,
	CtrlrDPadRight,
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
	Grave,
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

enum class ControllerAxis
{
	LeftX,
	LeftY,
	RightX,
	RightY,
	LeftTrigger,
	RightTrigger,
};

EG_API std::string_view ButtonToString(Button button);
EG_API Button ButtonFromString(std::string_view str);
EG_API std::string_view ButtonDisplayName(Button button);

constexpr size_t NUM_BUTTONS = static_cast<size_t>(Button::NUM_BUTTONS);

class InputState;

namespace detail
{
EG_API extern InputState* currentIS;
EG_API extern InputState* previousIS;
EG_API extern std::string inputtedText;
} // namespace detail

class EG_API InputState
{
public:
	InputState() = default;

	Button PressedButton() const { return m_pressed; }

	bool IsButtonDown(Button button) const
	{
		return (m_isButtonDown[static_cast<size_t>(button) / 8] & (1 << (static_cast<size_t>(button) % 8))) != 0;
	}

	bool IsCtrlDown() const { return IsButtonDown(Button::LeftControl) || IsButtonDown(Button::RightControl); }

	bool IsShiftDown() const { return IsButtonDown(Button::LeftShift) || IsButtonDown(Button::RightShift); }

	bool IsAltDown() const { return IsButtonDown(Button::LeftAlt) || IsButtonDown(Button::RightAlt); }

	void OnButtonDown(Button button)
	{
		m_isButtonDown[static_cast<size_t>(button) / 8] |= static_cast<char>(1 << (static_cast<size_t>(button) % 8));
		m_pressed = button;
	}

	void OnButtonUp(Button button)
	{
		if (button == m_pressed)
			m_pressed = Button::Unknown;
		m_isButtonDown[static_cast<size_t>(button) / 8] &= static_cast<char>(~(1 << (static_cast<size_t>(button) % 8)));
	}

	void OnAxisMoved(ControllerAxis axis, float newValue) { m_axisValues[static_cast<int>(axis)] = newValue; }

	glm::vec2 CursorPos() const { return { cursorX, cursorY }; }

	glm::vec2 CursorPosDelta() const { return { cursorDeltaX, cursorDeltaY }; }

	glm::vec2 ScrollPos() const { return { scrollX, scrollY }; }

	float AxisValue(ControllerAxis axis) { return m_axisValues[static_cast<int>(axis)]; }

	glm::vec2 LeftAnalogValue() const { return { m_axisValues[0], m_axisValues[1] }; }

	glm::vec2 RightAnalogValue() const { return { m_axisValues[2], m_axisValues[3] }; }

	static const InputState& Current() { return *detail::currentIS; }

	static const InputState& Previous() { return *detail::previousIS; }

	float scrollX = 0;
	float scrollY = 0;
	float cursorX = 0;
	float cursorY = 0;
	float cursorDeltaX = 0;
	float cursorDeltaY = 0;

private:
	Button m_pressed = Button::Unknown;
	char m_isButtonDown[13] = {};
	float m_axisValues[6] = {};

	static_assert(sizeof(m_isButtonDown) * 8 >= NUM_BUTTONS);
};

EG_API void SetRelativeMouseMode(bool relMouseMode);
EG_API bool RelativeMouseModeActive();

EG_API void TextInputActive(const std::optional<Rectangle>& textInputRect = {});

inline const std::string& InputtedText()
{
	return detail::inputtedText;
}

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

inline glm::vec2 CursorPos()
{
	return detail::currentIS->CursorPos();
}

inline float CursorX()
{
	return detail::currentIS->cursorX;
}

inline float CursorY()
{
	return detail::currentIS->cursorY;
}

inline float CursorDeltaX()
{
	return detail::currentIS->cursorDeltaX;
}

inline float CursorDeltaY()
{
	return detail::currentIS->cursorDeltaY;
}

inline glm::vec2 CursorPosDelta()
{
	return detail::currentIS->CursorPosDelta();
}

inline glm::vec2 PrevCursorPos()
{
	return detail::previousIS->CursorPos();
}

inline float PrevCursorX()
{
	return detail::previousIS->cursorX;
}

inline float PrevCursorY()
{
	return detail::previousIS->cursorY;
}

inline float AxisValue(ControllerAxis axis)
{
	return detail::currentIS->AxisValue(axis);
}

inline glm::vec2 LeftAnalogValue()
{
	return detail::currentIS->LeftAnalogValue();
}

inline glm::vec2 RightAnalogValue()
{
	return detail::currentIS->RightAnalogValue();
}

inline float PrevAxisValue(ControllerAxis axis)
{
	return detail::previousIS->AxisValue(axis);
}

inline glm::vec2 PrevLeftAnalogValue()
{
	return detail::previousIS->LeftAnalogValue();
}

inline glm::vec2 PrevRightAnalogValue()
{
	return detail::previousIS->RightAnalogValue();
}
} // namespace eg
