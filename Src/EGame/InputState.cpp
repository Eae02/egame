#include "InputState.hpp"
#include "Utils.hpp"
#include "Log.hpp"
#include "Graphics/Graphics.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#else
#include <SDL.h>
#endif

namespace eg
{
	InputState* detail::currentIS;
	InputState* detail::previousIS;
	
	std::string detail::inputtedText;
	
	std::string_view ButtonDisplayName(Button button)
	{
		switch (button)
		{
		case Button::MouseLeft:
			return "Mouse Left";
		case Button::MouseRight:
			return "Mouse Right";
		case Button::MouseMiddle:
			return "Mouse Middle";
		case Button::MouseSide1:
			return "Mouse Side 1";
		case Button::MouseSide2:
			return "Mouse Side 2";
		case Button::CtrlrA:
			return "A";
		case Button::CtrlrB:
			return "B";
		case Button::CtrlrX:
			return "X";
		case Button::CtrlrY:
			return "Y";
		case Button::CtrlrBack:
			return "Back";
		case Button::CtrlrGuide:
			return "Guide";
		case Button::CtrlrStart:
			return "Start";
		case Button::CtrlrLeftStick:
			return "Left Stick";
		case Button::CtrlrRightStick:
			return "Right Stick";
		case Button::CtrlrLeftShoulder:
			return "Left Shoulder";
		case Button::CtrlrRightShoulder:
			return "Right Shoulder";
		case Button::CtrlrDPadUp:
			return "DPad Up";
		case Button::CtrlrDPadDown:
			return "DPad Down";
		case Button::CtrlrDPadLeft:
			return "DPad Left";
		case Button::CtrlrDPadRight:
			return "DPad Right";
		case Button::LeftShift:
			return "Left Shift";
		case Button::RightShift:
			return "Right Shift";
		case Button::LeftControl:
			return "Left Control";
		case Button::RightControl:
			return "Right Control";
		case Button::LeftAlt:
			return "Left Alt";
		case Button::RightAlt:
			return "Right Alt";
		case Button::Escape:
			return "Escape";
		case Button::Enter:
			return "Enter";
		case Button::Space:
			return "Space";
		case Button::Tab:
			return "Tab";
		case Button::Backspace:
			return "Backspace";
		case Button::LeftArrow:
			return "Left Arrow Key";
		case Button::UpArrow:
			return "Up Arrow Key";
		case Button::RightArrow:
			return "Right Arrow Key";
		case Button::DownArrow:
			return "Down Arrow Key";
		case Button::Grave:
			return "Grave";
		case Button::PageUp:
			return "Page Up";
		case Button::PageDown:
			return "Page Down";
		case Button::Home:
			return "Home";
		case Button::End:
			return "End";
		case Button::Delete:
			return "Delete";
		case Button::D0:
			return "0";
		case Button::D1:
			return "1";
		case Button::D2:
			return "2";
		case Button::D3:
			return "3";
		case Button::D4:
			return "4";
		case Button::D5:
			return "5";
		case Button::D6:
			return "6";
		case Button::D7:
			return "7";
		case Button::D8:
			return "8";
		case Button::D9:
			return "9";
		case Button::A:
			return "A";
		case Button::B:
			return "B";
		case Button::C:
			return "C";
		case Button::D:
			return "D";
		case Button::E:
			return "E";
		case Button::F:
			return "F";
		case Button::G:
			return "G";
		case Button::H:
			return "H";
		case Button::I:
			return "I";
		case Button::J:
			return "J";
		case Button::K:
			return "K";
		case Button::L:
			return "L";
		case Button::M:
			return "M";
		case Button::N:
			return "N";
		case Button::O:
			return "O";
		case Button::P:
			return "P";
		case Button::Q:
			return "Q";
		case Button::R:
			return "R";
		case Button::S:
			return "S";
		case Button::T:
			return "T";
		case Button::U:
			return "U";
		case Button::V:
			return "V";
		case Button::W:
			return "W";
		case Button::X:
			return "X";
		case Button::Y:
			return "Y";
		case Button::Z:
			return "Z";
		case Button::F1:
			return "F1";
		case Button::F2:
			return "F2";
		case Button::F3:
			return "F3";
		case Button::F4:
			return "F4";
		case Button::F5:
			return "F5";
		case Button::F6:
			return "F6";
		case Button::F7:
			return "F7";
		case Button::F8:
			return "F8";
		case Button::F9:
			return "F9";
		case Button::F10:
			return "F10";
		case Button::F11:
			return "F11";
		case Button::F12:
			return "F12";
		case Button::F13:
			return "F13";
		case Button::F14:
			return "F14";
		case Button::F15:
			return "F15";
		case Button::F16:
			return "F16";
		case Button::F17:
			return "F17";
		case Button::F18:
			return "F18";
		case Button::F19:
			return "F19";
		case Button::F20:
			return "F20";
		case Button::F21:
			return "F21";
		case Button::F22:
			return "F22";
		case Button::F23:
			return "F23";
		case Button::F24:
			return "F24";
		default:
			return "Unknown";
		}
	}
	
	static std::string_view buttonNames[] =
		{
			"Unknown",
			"MouseLeft",
			"MouseRight",
			"MouseMiddle",
			"MouseSide1",
			"MouseSide2",
			"ControllerA",
			"ControllerB",
			"ControllerX",
			"ControllerY",
			"ControllerBack",
			"ControllerGuide",
			"ControllerStart",
			"ControllerLeftStick",
			"ControllerRightStick",
			"ControllerLeftShoulder",
			"ControllerRightShoulder",
			"ControllerDPadUp",
			"ControllerDPadDown",
			"ControllerDPadLeft",
			"ControllerDPadRight",
			"LeftShift",
			"RightShift",
			"LeftControl",
			"RightControl",
			"LeftAlt",
			"RightAlt",
			"Escape",
			"Enter",
			"Space",
			"Tab",
			"Backspace",
			"LeftArrow",
			"UpArrow",
			"RightArrow",
			"DownArrow",
			"Grave",
			"PageUp",
			"PageDown",
			"Home",
			"End",
			"Delete",
			"0",
			"1",
			"2",
			"3",
			"4",
			"5",
			"6",
			"7",
			"8",
			"9",
			"A",
			"B",
			"C",
			"D",
			"E",
			"F",
			"G",
			"H",
			"I",
			"J",
			"K",
			"L",
			"M",
			"N",
			"O",
			"P",
			"Q",
			"R",
			"S",
			"T",
			"U",
			"V",
			"W",
			"X",
			"Y",
			"Z",
			"F1",
			"F2",
			"F3",
			"F4",
			"F5",
			"F6",
			"F7",
			"F8",
			"F9",
			"F10",
			"F11",
			"F12",
			"F13",
			"F14",
			"F15",
			"F16",
			"F17",
			"F18",
			"F19",
			"F20",
			"F21",
			"F22",
			"F23",
			"F24"
		};
	
	Button ButtonFromString(std::string_view str)
	{
		for (size_t i = 0; i < ArrayLen(buttonNames); i++)
		{
			if (StringEqualCaseInsensitive(buttonNames[i], str))
				return (Button)i;
		}
		return Button::Unknown;
	}
	
	std::string_view ButtonToString(Button button)
	{
		if ((size_t)button >= ArrayLen(buttonNames))
			return buttonNames[0];
		return buttonNames[(int)button];
	}
	
	bool g_relMouseMode = false;
	
	void SetRelativeMouseMode(bool relMouseMode)
	{
		if (g_relMouseMode == relMouseMode)
			return;
		g_relMouseMode = relMouseMode;
#ifdef __EMSCRIPTEN__
		if (relMouseMode)
			emscripten_request_pointerlock(nullptr, true);
		else
			emscripten_exit_pointerlock();
#else
		SDL_SetRelativeMouseMode((SDL_bool)relMouseMode);
#endif
	}
	
	bool RelativeMouseModeActive()
	{
		return g_relMouseMode;
	}
	
	bool hasCalledTextInputActive = false;
	bool textInputActive = false;
	bool hasSetTextInputRect = false;
	
	void TextInputActive(const std::optional<Rectangle>& textInputRect)
	{
#ifndef __EMSCRIPTEN__
		if (!textInputActive)
		{
			SDL_StartTextInput();
			textInputActive = true;
		}
		if (!hasSetTextInputRect && textInputRect)
		{
			SDL_Rect rect;
			rect.x = (int)textInputRect->x;
			rect.y = (int)textInputRect->y;
			rect.w = (int)std::ceil(textInputRect->w);
			rect.h = (int)std::ceil(textInputRect->h);
			SDL_SetTextInputRect(&rect);
			hasSetTextInputRect = true;
		}
		hasCalledTextInputActive = true;
#endif
	}
}
