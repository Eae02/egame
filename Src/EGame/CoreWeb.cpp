#ifdef __EMSCRIPTEN__
#include "Core.hpp"
#include "Event.hpp"
#include "Graphics/AbstractionHL.hpp"
#include "Graphics/SpriteFont.hpp"
#include "InputState.hpp"

#include <emscripten.h>
#include <emscripten/html5.h>
#include <unordered_map>
#include <utf8.h>

#ifdef EG_ENABLE_WEBGPU
#include "Graphics/WebGPU/WGPUPlatform.hpp"
#endif

namespace eg
{
int detail::PlatformInit(const RunConfig& runConfig, bool _headless, std::function<void()> initCompleteCallback)
{
	GraphicsAPIInitArguments apiInitArguments = {};
	apiInitArguments.defaultFramebufferSRGB = HasFlag(runConfig.flags, RunFlags::DefaultFramebufferSRGB);
	apiInitArguments.forceDepthZeroToOne = HasFlag(runConfig.flags, RunFlags::ForceDepthZeroToOne);
	apiInitArguments.initDoneCallback = std::move(initCompleteCallback);
	if (!InitializeGraphicsAPI(eg::GraphicsAPI::WebGPU, apiInitArguments))
	{
		return 1;
	}
	return 0;
}

extern bool shouldClose;

static std::unique_ptr<IGame> game;

static std::unordered_map<std::string_view, Button> keyCodeMap = {
	{ "Digit0", Button::D0 },
	{ "Digit1", Button::D1 },
	{ "Digit2", Button::D2 },
	{ "Digit3", Button::D3 },
	{ "Digit4", Button::D4 },
	{ "Digit5", Button::D5 },
	{ "Digit6", Button::D6 },
	{ "Digit7", Button::D7 },
	{ "Digit8", Button::D8 },
	{ "Digit9", Button::D9 },
	{ "KeyA", Button::A },
	{ "KeyB", Button::B },
	{ "KeyC", Button::C },
	{ "KeyD", Button::D },
	{ "KeyE", Button::E },
	{ "KeyF", Button::F },
	{ "KeyG", Button::G },
	{ "KeyH", Button::H },
	{ "KeyI", Button::I },
	{ "KeyJ", Button::J },
	{ "KeyK", Button::K },
	{ "KeyL", Button::L },
	{ "KeyM", Button::M },
	{ "KeyN", Button::N },
	{ "KeyO", Button::O },
	{ "KeyP", Button::P },
	{ "KeyQ", Button::Q },
	{ "KeyR", Button::R },
	{ "KeyS", Button::S },
	{ "KeyT", Button::T },
	{ "KeyU", Button::U },
	{ "KeyV", Button::V },
	{ "KeyW", Button::W },
	{ "KeyX", Button::X },
	{ "KeyY", Button::Y },
	{ "KeyZ", Button::Z },
	{ "F1", Button::F1 },
	{ "F2", Button::F2 },
	{ "F3", Button::F3 },
	{ "F4", Button::F4 },
	{ "F5", Button::F5 },
	{ "F6", Button::F6 },
	{ "F7", Button::F7 },
	{ "F8", Button::F8 },
	{ "F9", Button::F9 },
	{ "F10", Button::F10 },
	{ "F11", Button::F11 },
	{ "F12", Button::F12 },
	{ "F13", Button::F13 },
	{ "F14", Button::F14 },
	{ "F15", Button::F15 },
	{ "F16", Button::F16 },
	{ "F17", Button::F17 },
	{ "F18", Button::F18 },
	{ "F19", Button::F19 },
	{ "F20", Button::F20 },
	{ "F21", Button::F21 },
	{ "F22", Button::F22 },
	{ "F23", Button::F23 },
	{ "ShiftLeft", Button::LeftShift },
	{ "ShiftRight", Button::RightShift },
	{ "ControlLeft", Button::LeftControl },
	{ "ControlRight", Button::RightControl },
	{ "AltLeft", Button::LeftAlt },
	{ "AltRight", Button::RightAlt },
	{ "Escape", Button::Escape },
	{ "Enter", Button::Enter },
	{ "Space", Button::Space },
	{ "Tab", Button::Tab },
	{ "Backspace", Button::Backspace },
	{ "ArrowLeft", Button::LeftArrow },
	{ "ArrowUp", Button::UpArrow },
	{ "ArrowRight", Button::RightArrow },
	{ "ArrowDown", Button::DownArrow },
	{ "Backquote", Button::Grave },
	{ "PageUp", Button::PageUp },
	{ "PageDown", Button::PageDown },
	{ "Home", Button::Home },
	{ "End", Button::End },
	{ "Delete", Button::Delete },
};

static const std::string_view nonTextKeys[] = {
	"F1",       "F2",   "F3",  "F4",     "F5",        "F6",        "F7",      "F8",         "F9",        "F10",
	"F11",      "F12",  "F13", "F14",    "F15",       "F16",       "F17",     "F18",        "F19",       "F20",
	"F21",      "F22",  "F23", "Tab",    "Backspace", "ArrowLeft", "ArrowUp", "ArrowRight", "ArrowDown", "PageUp",
	"PageDown", "Home", "End", "Delete", "CapsLock",  "AltGraph",  "Enter"
};

static std::string newInputtedText;
static std::vector<std::pair<Button, bool>> newButtonDownEvents;
static std::vector<std::pair<Button, bool>> newButtonUpEvents;
static glm::ivec2 pendingCursorDelta;
static double scrollX = 0;
static double scrollY = 0;

extern bool g_relMouseMode;

static inline Button TranslateEmscriptenMouseButton(int button)
{
	return static_cast<Button>(static_cast<int>(Button::MouseLeft) + button);
}

static void WebMainLoopCallback()
{
	if (!gal::IsLoadingComplete() || !SpriteFont::IsDevFontLoaded())
		return;
	detail::RunFrame(*game);
}

void detail::PlatformRunGameLoop(std::unique_ptr<IGame> _game)
{
	game = std::move(_game);

	emscripten_set_keydown_callback(
		EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true,
		[](int eventType, const EmscriptenKeyboardEvent* keyEvent, void* userData)
		{
			auto keyIt = keyCodeMap.find(keyEvent->code);
			if (keyIt != keyCodeMap.end())
				newButtonDownEvents.emplace_back(keyIt->second, keyEvent->repeat);
			if (keyEvent->location == DOM_KEY_LOCATION_STANDARD && !Contains(nonTextKeys, keyEvent->key))
				newInputtedText += keyEvent->key;
			return EM_TRUE;
		});

	emscripten_set_keyup_callback(
		EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true,
		[](int eventType, const EmscriptenKeyboardEvent* keyEvent, void* userData)
		{
			auto keyIt = keyCodeMap.find(keyEvent->code);
			if (keyIt != keyCodeMap.end())
				newButtonUpEvents.emplace_back(keyIt->second, keyEvent->repeat);
			return EM_TRUE;
		});

	emscripten_set_mousedown_callback(
		EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true,
		[](int eventType, const EmscriptenMouseEvent* mouseEvent, void* userData)
		{
			if (mouseEvent->button >= 0 && mouseEvent->button <= 2)
				newButtonDownEvents.emplace_back(TranslateEmscriptenMouseButton(mouseEvent->button), false);
			return EM_TRUE;
		});

	emscripten_set_mouseup_callback(
		EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true,
		[](int eventType, const EmscriptenMouseEvent* mouseEvent, void* userData)
		{
			if (mouseEvent->button >= 0 && mouseEvent->button <= 2)
				newButtonUpEvents.emplace_back(TranslateEmscriptenMouseButton(mouseEvent->button), false);
			return EM_TRUE;
		});

	emscripten_set_mousemove_callback(
		EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true,
		[](int eventType, const EmscriptenMouseEvent* mouseEvent, void* userData)
		{
			if (detail::currentIS)
			{
				detail::currentIS->cursorX = mouseEvent->clientX;
				detail::currentIS->cursorY = mouseEvent->clientY;
				pendingCursorDelta.x += mouseEvent->movementX;
				pendingCursorDelta.y += mouseEvent->movementY;
			}
			return EM_TRUE;
		});

	emscripten_set_wheel_callback(
		EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true,
		[](int eventType, const EmscriptenWheelEvent* wheelEvent, void* userData)
		{
			if (detail::currentIS)
			{
				scrollY -= wheelEvent->deltaY;
				scrollX += wheelEvent->deltaX;
			}
			return EM_TRUE;
		});

	emscripten_set_pointerlockchange_callback(
		EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true,
		[](int eventType, const EmscriptenPointerlockChangeEvent* pointerlockChangeEvent, void* userData)
		{
			if (g_relMouseMode && !pointerlockChangeEvent->isActive)
			{
				g_relMouseMode = false;
				RaiseEvent<RelativeMouseModeLostEvent>({});
			}
			return EM_TRUE;
		});

#ifdef EG_ENABLE_WEBGPU
	if (eg::CurrentGraphicsAPI() == eg::GraphicsAPI::WebGPU)
	{
		graphics_api::webgpu::StartWebMainLoop(WebMainLoopCallback);
	}
	else
#endif
	{
		emscripten_set_main_loop(WebMainLoopCallback, 0, 0);
	}
}

void detail::PlatformStartFrame()
{
	detail::inputtedText = newInputtedText;
	newInputtedText.clear();

	detail::currentIS->scrollX = static_cast<int>(std::round(scrollX));
	detail::currentIS->scrollY = static_cast<int>(std::round(scrollY));

	detail::currentIS->cursorDeltaX = pendingCursorDelta.x;
	detail::currentIS->cursorDeltaY = pendingCursorDelta.y;
	pendingCursorDelta = glm::ivec2(0);

	for (const std::pair<Button, bool>& event : newButtonDownEvents)
	{
		detail::ButtonDownEvent(event.first, event.second);
	}

	for (const std::pair<Button, bool>& event : newButtonUpEvents)
	{
		detail::ButtonUpEvent(event.first, event.second);
	}

	newButtonDownEvents.clear();
	newButtonUpEvents.clear();
}

std::string GetClipboardText()
{
	return "";
}

void SetClipboardText(const char* text) {}

void SetDisplayModeWindowed() {}
void SetDisplayModeFullscreenDesktop() {}
void SetDisplayModeFullscreen(const FullscreenDisplayMode& displayMode) {}

void SetWindowIcon(uint32_t width, uint32_t height, const void* rgbaData) {}

bool VulkanAppearsSupported()
{
	return false;
}
} // namespace eg

#endif
