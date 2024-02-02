#ifndef __EMSCRIPTEN__
#include "Core.hpp"
#include "Event.hpp"
#include "GameController.hpp"
#include "Graphics/AbstractionHL.hpp"
#include "Graphics/Vulkan/VulkanMain.hpp"
#include "InputState.hpp"
#include "SDLTranslation.hpp"

#include <SDL.h>
#include <iostream>
#include <list>

using namespace std::chrono;

namespace eg
{
static const char* exeDirPathPtr;

static bool firstMouseMotionEvent = true;
static bool firstControllerAxisEvent = true;
static SDL_Window* sdlWindow;

bool VulkanAppearsSupported()
{
#ifdef EG_NO_VULKAN
	return false;
#else
	return graphics_api::vk::EarlyInitializeMemoized();
#endif
}

int detail::PlatformInit(const RunConfig& runConfig)
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER))
	{
		std::cerr << "SDL failed to initialize: " << SDL_GetError() << std::endl;
		return 1;
	}

	if (DevMode())
	{
		SDL_SetHint("SDL_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR", "0");
	}

	graphics_api::vk::EarlyInitializeMemoized();

	constexpr int DISPLAY_INDEX = 0;
	SDL_DisplayMode currentDisplayMode;
	SDL_GetCurrentDisplayMode(DISPLAY_INDEX, &currentDisplayMode);
	int numDisplayModes = SDL_GetNumDisplayModes(DISPLAY_INDEX);
	for (int i = 0; i < numDisplayModes; i++)
	{
		SDL_DisplayMode mode;
		SDL_GetDisplayMode(DISPLAY_INDEX, i, &mode);
		if (mode.w == 0 || mode.h == 0 || mode.refresh_rate == 0 || ToUnsigned(mode.w) < runConfig.minWindowW ||
		    ToUnsigned(mode.h) < runConfig.minWindowH)
		{
			continue;
		}
		FullscreenDisplayMode dm = { ToUnsigned(mode.w), ToUnsigned(mode.h), ToUnsigned(mode.refresh_rate) };
		if (!Contains(detail::fullscreenDisplayModes, dm))
		{
			if (currentDisplayMode.w == mode.w && currentDisplayMode.h == mode.h &&
			    currentDisplayMode.refresh_rate == mode.refresh_rate)
			{
				detail::nativeDisplayModeIndex = static_cast<int>(detail::fullscreenDisplayModes.size());
			}
			detail::fullscreenDisplayModes.push_back(dm);
		}
	}
	if (detail::nativeDisplayModeIndex == -1 && !detail::fullscreenDisplayModes.empty())
	{
		detail::nativeDisplayModeIndex = 0;
	}

	if (exeDirPathPtr == nullptr)
	{
		exeDirPathPtr = SDL_GetBasePath();
		detail::exeDirPath = exeDirPathPtr;
	}

	uint32_t windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;

	Format defaultDSFormat = runConfig.defaultDepthStencilFormat;
	if (GetFormatType(defaultDSFormat) != FormatTypes::DepthStencil && defaultDSFormat != Format::Undefined)
	{
		Log(LogLevel::Error, "gfx", "Invalid default depth/stencil format");
		defaultDSFormat = Format::Depth16;
	}

	if (runConfig.graphicsAPI == GraphicsAPI::OpenGL)
	{
		int contextFlags = SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
		if (DevMode())
			contextFlags |= SDL_GL_CONTEXT_DEBUG_FLAG;

#ifdef __APPLE__
		constexpr std::pair<uint32_t, uint32_t> OPEN_GL_VERSION = { 3, 3 };
#else
		constexpr std::pair<uint32_t, uint32_t> OPEN_GL_VERSION = { 4, 3 };
#endif

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, OPEN_GL_VERSION.first);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, OPEN_GL_VERSION.second);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, contextFlags);
		SDL_GL_SetAttribute(
			SDL_GL_FRAMEBUFFER_SRGB_CAPABLE,
			static_cast<int>(HasFlag(runConfig.flags, RunFlags::DefaultFramebufferSRGB)));

		windowFlags |= SDL_WINDOW_OPENGL;
	}
	else if (runConfig.graphicsAPI == GraphicsAPI::Vulkan)
	{
		windowFlags |= SDL_WINDOW_VULKAN;
	}

	int windowW = std::max(currentDisplayMode.w * 3 / 5, ToInt(runConfig.minWindowW));
	int windowH = std::max(windowW * 2 / 3, ToInt(runConfig.minWindowH));
	sdlWindow = SDL_CreateWindow(
		detail::gameName.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowW, windowH, windowFlags);

	if (runConfig.minWindowW != 0 && runConfig.minWindowH != 0)
	{
		SDL_SetWindowMinimumSize(sdlWindow, runConfig.minWindowW, runConfig.minWindowH);
	}

	if (sdlWindow == nullptr)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error creating window", SDL_GetError(), nullptr);
		return 1;
	}

	GraphicsAPIInitArguments apiInitArguments;
	apiInitArguments.window = sdlWindow;
	apiInitArguments.defaultFramebufferSRGB = HasFlag(runConfig.flags, RunFlags::DefaultFramebufferSRGB);
	apiInitArguments.forceDepthZeroToOne = HasFlag(runConfig.flags, RunFlags::ForceDepthZeroToOne);
	apiInitArguments.defaultDepthStencilFormat = defaultDSFormat;
	apiInitArguments.preferIntegrated = HasFlag(runConfig.flags, RunFlags::PreferIntegratedGPU);
	apiInitArguments.preferGLESPath = HasFlag(runConfig.flags, RunFlags::PreferGLESPath);
	apiInitArguments.preferredDeviceName = runConfig.preferredGPUName;

	if (!InitializeGraphicsAPI(runConfig.graphicsAPI, apiInitArguments))
	{
		return 1;
	}

	gal::SetEnableVSync(HasFlag(runConfig.flags, RunFlags::VSync));

	firstMouseMotionEvent = true;
	firstControllerAxisEvent = true;

	return 0;
}

void SetDisplayModeFullscreen(const FullscreenDisplayMode& displayMode)
{
	SDL_DisplayMode wantedDM = {};
	wantedDM.w = displayMode.resolutionX;
	wantedDM.h = displayMode.resolutionY;
	wantedDM.refresh_rate = displayMode.refreshRate;
	SDL_DisplayMode closestDM;
	if (SDL_GetClosestDisplayMode(0, &wantedDM, &closestDM))
	{
		SDL_SetWindowDisplayMode(sdlWindow, &closestDM);
		SDL_SetWindowFullscreen(sdlWindow, SDL_WINDOW_FULLSCREEN);
	}
}

void SetDisplayModeFullscreenDesktop()
{
	SDL_SetWindowFullscreen(sdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
}

void SetDisplayModeWindowed()
{
	SDL_SetWindowFullscreen(sdlWindow, 0);
}

static SDL_Surface* sdlWindowSurface = nullptr;

void SetWindowIcon(uint32_t width, uint32_t height, const void* rgbaData)
{
	// sdlWindowSurface = SDL_CreateRGBSurfaceFrom(rgbaData, width, height, 4, 0, 0xFFU << 24, 0xFFU << 16, 0xFFU << 8,
	// 0xFFU);

	SDL_SetWindowIcon(sdlWindow, sdlWindowSurface);
}

extern bool hasCalledTextInputActive;
extern bool hasSetTextInputRect;
extern bool textInputActive;

void detail::PlatformRunGameLoop(std::unique_ptr<IGame> game)
{
	while (!gal::IsLoadingComplete())
	{
		std::this_thread::sleep_for(milliseconds(100));
	}

	while (!detail::shouldClose)
	{
		hasCalledTextInputActive = false;
		hasSetTextInputRect = false;

		int windowWidth, windowHeight, displayWidth, displayHeight;
		SDL_GetWindowSize(sdlWindow, &windowWidth, &windowHeight);
		gal::GetDrawableSize(displayWidth, displayHeight);
		detail::displayScaleFactor = static_cast<float>(displayWidth) / static_cast<float>(windowWidth);

		detail::RunFrame(*game);
		if (!hasCalledTextInputActive && textInputActive)
		{
			textInputActive = false;
			SDL_StopTextInput();
		}
	}

	game.reset();

	detail::CoreUninitialize();

	SDL_DestroyWindow(sdlWindow);
	SDL_Quit();
}

void detail::PlatformStartFrame()
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		case SDL_QUIT:
			detail::shouldClose = true;
			break;
		case SDL_KEYDOWN:
			detail::ButtonDownEvent(detail::TranslateSDLKey(event.key.keysym.scancode), event.key.repeat);

			if (RelativeMouseModeActive() && DevMode() && !event.key.repeat &&
			    event.key.keysym.scancode == SDL_SCANCODE_F10)
			{
				bool rel = SDL_GetRelativeMouseMode();
				SDL_SetRelativeMouseMode(rel ? SDL_FALSE : SDL_TRUE);
			}
			break;
		case SDL_KEYUP:
			detail::ButtonUpEvent(detail::TranslateSDLKey(event.key.keysym.scancode), event.key.repeat);
			break;
		case SDL_CONTROLLERBUTTONDOWN:
			if (SDL_GameControllerFromInstanceID(event.cbutton.which) == activeController)
			{
				detail::ButtonDownEvent(detail::TranslateSDLControllerButton(event.cbutton.button), false);
			}
			break;
		case SDL_CONTROLLERBUTTONUP:
			if (SDL_GameControllerFromInstanceID(event.cbutton.which) == activeController)
			{
				detail::ButtonUpEvent(detail::TranslateSDLControllerButton(event.cbutton.button), false);
			}
			break;
		case SDL_CONTROLLERAXISMOTION:
			if (SDL_GameControllerFromInstanceID(event.caxis.which) == activeController)
			{
				const ControllerAxis axis = static_cast<ControllerAxis>(event.caxis.axis);
				const float valueF = event.caxis.value / SDL_JOYSTICK_AXIS_MAX;
				if (firstControllerAxisEvent)
				{
					detail::previousIS->OnAxisMoved(axis, valueF);
					firstControllerAxisEvent = false;
				}
				detail::currentIS->OnAxisMoved(axis, valueF);
			}
			break;
		case SDL_CONTROLLERDEVICEADDED:
			AddGameController(SDL_GameControllerFromInstanceID(event.cdevice.which));
			break;
		case SDL_MOUSEBUTTONDOWN:
			detail::ButtonDownEvent(detail::TranslateSDLMouseButton(event.button.button), false);
			break;
		case SDL_MOUSEBUTTONUP:
			detail::ButtonUpEvent(detail::TranslateSDLMouseButton(event.button.button), false);
			break;
		case SDL_MOUSEMOTION:
			if (firstMouseMotionEvent)
			{
				detail::previousIS->cursorX = static_cast<float>(event.motion.x) * detail::displayScaleFactor;
				detail::previousIS->cursorY = static_cast<float>(event.motion.y) * detail::displayScaleFactor;
				firstMouseMotionEvent = false;
			}
			detail::currentIS->cursorX = static_cast<float>(event.motion.x) * detail::displayScaleFactor;
			detail::currentIS->cursorY = static_cast<float>(event.motion.y) * detail::displayScaleFactor;
			detail::currentIS->cursorDeltaX += static_cast<float>(event.motion.xrel) * detail::displayScaleFactor;
			detail::currentIS->cursorDeltaY += static_cast<float>(event.motion.yrel) * detail::displayScaleFactor;
			break;
		case SDL_MOUSEWHEEL:
			detail::currentIS->scrollX += static_cast<float>(event.wheel.x);
			detail::currentIS->scrollY += static_cast<float>(event.wheel.y);
			break;
		case SDL_TEXTINPUT:
			detail::inputtedText.append(event.text.text);
			break;
		}
	}
}

std::string GetClipboardText()
{
	char* sdlClipboardText = SDL_GetClipboardText();
	std::string ret = sdlClipboardText;
	SDL_free(sdlClipboardText);
	return ret;
}

void SetClipboardText(const char* text)
{
	SDL_SetClipboardText(text);
}
} // namespace eg

#endif
