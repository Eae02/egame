#include "Core.hpp"
#include "MainThreadInvoke.hpp"
#include "Platform/FontConfig.hpp"
#include "Graphics/AbstractionHL.hpp"
#include "Graphics/SpriteBatch.hpp"
#include "Graphics/SpriteFont.hpp"
#include "InputState.hpp"
#include "Event.hpp"
#include "Console.hpp"
#include "TranslationGizmo.h"
#include "GameController.hpp"

#include <SDL.h>
#include <iostream>

using namespace std::chrono;

namespace eg
{
	std::mutex detail::mutexMTI;
	LinearAllocator detail::allocMTI;
	detail::MTIBase* detail::firstMTI;
	detail::MTIBase* detail::lastMTI;
	std::thread::id detail::mainThreadId;
	
	bool detail::shouldClose;
	std::string detail::gameName;
	std::string_view detail::exeDirPath;
	uint64_t detail::frameIndex;
	
	extern bool createAssetPackage;
	
	static const char* exeDirPathPtr;
	
	static Button TranslateSDLKey(SDL_Scancode scancode)
	{
		switch (scancode)
		{
		case SDL_SCANCODE_LSHIFT: return Button::LeftShift;
		case SDL_SCANCODE_RSHIFT: return Button::RightShift;
		case SDL_SCANCODE_LCTRL: return Button::LeftControl;
		case SDL_SCANCODE_RCTRL: return Button::RightControl;
		case SDL_SCANCODE_LALT: return Button::LeftAlt;
		case SDL_SCANCODE_RALT: return Button::RightAlt;
		case SDL_SCANCODE_ESCAPE: return Button::Escape;
		case SDL_SCANCODE_BACKSPACE: return Button::Backspace;
		case SDL_SCANCODE_RETURN: return Button::Enter;
		case SDL_SCANCODE_TAB: return Button::Tab;
		case SDL_SCANCODE_SPACE: return Button::Space;
		case SDL_SCANCODE_LEFT: return Button::LeftArrow;
		case SDL_SCANCODE_UP: return Button::UpArrow;
		case SDL_SCANCODE_RIGHT: return Button::RightArrow;
		case SDL_SCANCODE_DOWN: return Button::DownArrow;
		case SDL_SCANCODE_GRAVE: return Button::Grave;
		case SDL_SCANCODE_PAGEUP: return Button::PageUp;
		case SDL_SCANCODE_PAGEDOWN: return Button::PageDown;
		case SDL_SCANCODE_HOME: return Button::Home;
		case SDL_SCANCODE_END: return Button::End;
		case SDL_SCANCODE_DELETE: return Button::Delete;
		case SDL_SCANCODE_0: return Button::D0;
		case SDL_SCANCODE_1: return Button::D1;
		case SDL_SCANCODE_2: return Button::D2;
		case SDL_SCANCODE_3: return Button::D3;
		case SDL_SCANCODE_4: return Button::D4;
		case SDL_SCANCODE_5: return Button::D5;
		case SDL_SCANCODE_6: return Button::D6;
		case SDL_SCANCODE_7: return Button::D7;
		case SDL_SCANCODE_8: return Button::D8;
		case SDL_SCANCODE_9: return Button::D9;
		case SDL_SCANCODE_A: return Button::A;
		case SDL_SCANCODE_B: return Button::B;
		case SDL_SCANCODE_C: return Button::C;
		case SDL_SCANCODE_D: return Button::D;
		case SDL_SCANCODE_E: return Button::E;
		case SDL_SCANCODE_F: return Button::F;
		case SDL_SCANCODE_G: return Button::G;
		case SDL_SCANCODE_H: return Button::H;
		case SDL_SCANCODE_I: return Button::I;
		case SDL_SCANCODE_J: return Button::J;
		case SDL_SCANCODE_K: return Button::K;
		case SDL_SCANCODE_L: return Button::L;
		case SDL_SCANCODE_M: return Button::M;
		case SDL_SCANCODE_N: return Button::N;
		case SDL_SCANCODE_O: return Button::O;
		case SDL_SCANCODE_P: return Button::P;
		case SDL_SCANCODE_Q: return Button::Q;
		case SDL_SCANCODE_R: return Button::R;
		case SDL_SCANCODE_S: return Button::S;
		case SDL_SCANCODE_T: return Button::T;
		case SDL_SCANCODE_U: return Button::U;
		case SDL_SCANCODE_V: return Button::V;
		case SDL_SCANCODE_W: return Button::W;
		case SDL_SCANCODE_X: return Button::X;
		case SDL_SCANCODE_Y: return Button::Y;
		case SDL_SCANCODE_Z: return Button::Z;
		case SDL_SCANCODE_F1: return Button::F1;
		case SDL_SCANCODE_F2: return Button::F2;
		case SDL_SCANCODE_F3: return Button::F3;
		case SDL_SCANCODE_F4: return Button::F4;
		case SDL_SCANCODE_F5: return Button::F5;
		case SDL_SCANCODE_F6: return Button::F6;
		case SDL_SCANCODE_F7: return Button::F7;
		case SDL_SCANCODE_F8: return Button::F8;
		case SDL_SCANCODE_F9: return Button::F9;
		case SDL_SCANCODE_F10: return Button::F10;
		case SDL_SCANCODE_F11: return Button::F11;
		case SDL_SCANCODE_F12: return Button::F12;
		case SDL_SCANCODE_F13: return Button::F13;
		case SDL_SCANCODE_F14: return Button::F14;
		case SDL_SCANCODE_F15: return Button::F15;
		case SDL_SCANCODE_F16: return Button::F16;
		case SDL_SCANCODE_F17: return Button::F17;
		case SDL_SCANCODE_F18: return Button::F18;
		case SDL_SCANCODE_F19: return Button::F19;
		case SDL_SCANCODE_F20: return Button::F20;
		case SDL_SCANCODE_F21: return Button::F21;
		case SDL_SCANCODE_F22: return Button::F22;
		case SDL_SCANCODE_F23: return Button::F23;
		case SDL_SCANCODE_F24: return Button::F24;
		default: return Button::Unknown;
		}
	}
	
	static Button TranslateSDLControllerButton(int button)
	{
		switch (button)
		{
		case SDL_CONTROLLER_BUTTON_A: return Button::CtrlrA;
		case SDL_CONTROLLER_BUTTON_B: return Button::CtrlrB;
		case SDL_CONTROLLER_BUTTON_X: return Button::CtrlrX;
		case SDL_CONTROLLER_BUTTON_Y: return Button::CtrlrY;
		case SDL_CONTROLLER_BUTTON_BACK: return Button::CtrlrBack;
		case SDL_CONTROLLER_BUTTON_GUIDE: return Button::CtrlrGuide;
		case SDL_CONTROLLER_BUTTON_START: return Button::CtrlrStart;
		case SDL_CONTROLLER_BUTTON_LEFTSTICK: return Button::CtrlrLeftStick;
		case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return Button::CtrlrRightStick;
		case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return Button::CtrlrLeftShoulder;
		case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return Button::CtrlrRightShoulder;
		case SDL_CONTROLLER_BUTTON_DPAD_UP: return Button::CtrlrDPadUp;
		case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return Button::CtrlrDPadDown;
		case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return Button::CtrlrDPadLeft;
		case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return Button::CtrlrDPadRight;
		default: return Button::Unknown;
		}
	}
	
	static Button TranslateSDLMouseButton(int button)
	{
		switch (button)
		{
		case SDL_BUTTON_LEFT: return Button::MouseLeft;
		case SDL_BUTTON_RIGHT: return Button::MouseRight;
		case SDL_BUTTON_MIDDLE: return Button::MouseMiddle;
		case SDL_BUTTON_X1: return Button::MouseSide1;
		case SDL_BUTTON_X2: return Button::MouseSide2;
		default: return Button::Unknown;
		}
	}
	
	void LoadAssetGenLibrary();
	void RegisterAssetLoaders();
	void RegisterDefaultAssetGenerator();
	
	void LoadGameControllers();
	void AddGameController(SDL_GameController* controller);
	
	extern std::vector<GameController> controllers;
	extern SDL_GameController* activeController;
	
	int detail::Run(const RunConfig& runConfig, std::unique_ptr<IGame> (*createGame)())
	{
		devMode = HasFlag(runConfig.flags, RunFlags::DevMode);
		createAssetPackage = HasFlag(runConfig.flags, RunFlags::CreateAssetPackage);
		
		if (const char* devEnv = getenv("EG_DEV"))
		{
			if (std::strcmp(devEnv, "true") == 0)
				devMode = true;
			else if (std::strcmp(devEnv, "false") == 0)
				devMode = false;
			else
			{
				Log(LogLevel::Warning, "misc",
				    R"(Could not parse EG_DEV environment variable, should be either "true" or "false".)");
			}
		}
		
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER))
		{
			std::cerr << "SDL failed to initialize: " << SDL_GetError() << std::endl;
			return 1;
		}
		
		if (exeDirPathPtr == nullptr)
		{
			exeDirPathPtr = SDL_GetBasePath();
			exeDirPath = exeDirPathPtr;
		}
		
		eg::DefineEventType<ResolutionChangedEvent>();
		eg::DefineEventType<ButtonEvent>();
		
		if (devMode)
		{
			console::Init();
		}
		
		InitPlatformFontConfig();
		RegisterDefaultAssetGenerator();
		LoadAssetGenLibrary();
		RegisterAssetLoaders();
		LoadGameControllers();
		
		uint32_t windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN;
		
		Format defaultDSFormat = runConfig.defaultDepthStencilFormat;
		if (GetFormatType(defaultDSFormat) != FormatTypes::DepthStencil &&
		    defaultDSFormat != Format::Undefined)
		{
			Log(LogLevel::Error, "gfx", "Invalid default depth/stencil format");
			defaultDSFormat = Format::Depth16;
		}
		
		if (runConfig.graphicsAPI == GraphicsAPI::OpenGL)
		{
			int contextFlags = SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
			if (devMode)
				contextFlags |= SDL_GL_CONTEXT_DEBUG_FLAG;
			
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, contextFlags);
			SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE,
				(int)HasFlag(runConfig.flags, RunFlags::DefaultFramebufferSRGB));
			
			windowFlags |= SDL_WINDOW_OPENGL;
		}
		else if (runConfig.graphicsAPI == GraphicsAPI::Vulkan)
		{
			windowFlags |= SDL_WINDOW_VULKAN;
		}
		
		if (runConfig.gameName != nullptr)
			gameName = runConfig.gameName;
		else
			gameName = "Untitled Game";
		
		SDL_Window* window = SDL_CreateWindow(gameName.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			1200, 800, windowFlags);
		
		if (window == nullptr)
		{
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error creating window", SDL_GetError(), nullptr);
			return 1;
		}
		
		GraphicsAPIInitArguments apiInitArguments;
		apiInitArguments.window = window;
		apiInitArguments.enableVSync = HasFlag(runConfig.flags, RunFlags::VSync);
		apiInitArguments.defaultFramebufferSRGB = HasFlag(runConfig.flags, RunFlags::DefaultFramebufferSRGB);
		apiInitArguments.defaultDepthStencilFormat = defaultDSFormat;
		
		if (!InitializeGraphicsAPI(runConfig.graphicsAPI, apiInitArguments))
		{
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error initializing graphics",
				"The selected graphics API could not be initialized.", nullptr);
			return 1;
		}
		
		gal::GetCapabilities(detail::graphicsCapabilities);
		
		SpriteBatch::InitStatic();
		TranslationGizmo::InitStatic();
		if (DevMode())
		{
			SpriteFont::LoadDevFont();
		}
		
		if (runConfig.initialize)
			runConfig.initialize();
		
		for (CallbackNode* node = onInit; node != nullptr; node = node->next)
		{
			node->callback();
		}
		
		std::unique_ptr<IGame> game = createGame();
		
		InputState currentIS;
		InputState previousIS;
		detail::currentIS = &currentIS;
		detail::previousIS = &previousIS;
		
		auto ButtonDownEvent = [&] (Button button, bool isRepeat)
		{
			if (!isRepeat && button != Button::Unknown && !currentIS.IsButtonDown(button))
				currentIS.OnButtonDown(button);
			RaiseEvent<ButtonEvent>({ button, true, isRepeat });
		};
		
		auto ButtonUpEvent = [&] (Button button, bool isRepeat)
		{
			if (!isRepeat && button != Button::Unknown && currentIS.IsButtonDown(button))
				currentIS.OnButtonUp(button);
			RaiseEvent<ButtonEvent>({ button, false, isRepeat });
		};
		
		gal::EndLoading();
		while (!gal::IsLoadingComplete())
			SDL_Delay(100);
		
		MarkUploadBuffersAvailable();
		
		bool firstMouseMotionEvent = true;
		bool firstControllerAxisEvent = true;
		
		resolutionX = -1;
		resolutionY = -1;
		shouldClose = false;
		frameIndex = 0;
		float dt = 0.0f;
		while (!shouldClose)
		{
			auto frameBeginTime = high_resolution_clock::now();
			
			previousIS = currentIS;
			currentIS.cursorDeltaX = 0;
			currentIS.cursorDeltaY = 0;
			inputtedText.clear();
			
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				switch (event.type)
				{
				case SDL_QUIT:
					shouldClose = true;
					break;
				case SDL_KEYDOWN:
					ButtonDownEvent(TranslateSDLKey(event.key.keysym.scancode), event.key.repeat);
					
					if (RelativeMouseModeActive() && DevMode() && !event.key.repeat &&
					    event.key.keysym.scancode == SDL_SCANCODE_F10)
					{
						bool rel = SDL_GetRelativeMouseMode();
						SDL_SetRelativeMouseMode((SDL_bool)(!rel));
					}
					break;
				case SDL_KEYUP:
					ButtonUpEvent(TranslateSDLKey(event.key.keysym.scancode), event.key.repeat);
					break;
				case SDL_CONTROLLERBUTTONDOWN:
					if (SDL_GameControllerFromInstanceID(event.cbutton.which) == activeController)
					{
						ButtonDownEvent(TranslateSDLControllerButton(event.cbutton.button), false);
					}
					break;
				case SDL_CONTROLLERBUTTONUP:
					if (SDL_GameControllerFromInstanceID(event.cbutton.which) == activeController)
					{
						ButtonUpEvent(TranslateSDLControllerButton(event.cbutton.button), false);
					}
					break;
				case SDL_CONTROLLERAXISMOTION:
					if (SDL_GameControllerFromInstanceID(event.caxis.which) == activeController)
					{
						const ControllerAxis axis = (ControllerAxis)event.caxis.axis;
						const float valueF = event.caxis.value / SDL_JOYSTICK_AXIS_MAX;
						if (firstControllerAxisEvent)
						{
							previousIS.OnAxisMoved(axis, valueF);
							firstControllerAxisEvent = false;
						}
						currentIS.OnAxisMoved(axis, valueF);
					}
					break;
				case SDL_CONTROLLERDEVICEADDED:
					AddGameController(SDL_GameControllerFromInstanceID(event.cdevice.which));
					break;
				case SDL_MOUSEBUTTONDOWN:
					ButtonDownEvent(TranslateSDLMouseButton(event.button.button), false);
					break;
				case SDL_MOUSEBUTTONUP:
					ButtonUpEvent(TranslateSDLMouseButton(event.button.button), false);
					break;
				case SDL_MOUSEMOTION:
					if (firstMouseMotionEvent)
					{
						previousIS.cursorX = event.motion.x;
						previousIS.cursorY = event.motion.y;
						firstMouseMotionEvent = false;
					}
					currentIS.cursorX = event.motion.x;
					currentIS.cursorY = event.motion.y;
					currentIS.cursorDeltaX += event.motion.xrel;
					currentIS.cursorDeltaY += event.motion.yrel;
					break;
				case SDL_MOUSEWHEEL:
					currentIS.scrollX += event.wheel.x;
					currentIS.scrollY += event.wheel.y;
					break;
				case SDL_TEXTINPUT:
					inputtedText.append(event.text.text);
					break;
				}
			}
			
			gal::BeginFrame();
			
			int newDrawableW, newDrawableH;
			gal::GetDrawableSize(newDrawableW, newDrawableH);
			if (newDrawableW != resolutionX || newDrawableH != resolutionY)
			{
				resolutionX = newDrawableW;
				resolutionY = newDrawableH;
				game->ResolutionChanged(resolutionX, resolutionY);
				RaiseEvent(ResolutionChangedEvent { resolutionX, resolutionY });
			}
			
			SpriteBatch::overlay.Begin();
			
			game->RunFrame(dt);
			
			console::Update(dt);
			console::Draw(SpriteBatch::overlay, resolutionX, resolutionY);
			
			//Processes main thread invokes
			for (MTIBase* mti = firstMTI; mti != nullptr; mti = mti->next)
				mti->Invoke();
			firstMTI = lastMTI = nullptr;
			allocMTI.Reset();
			
			eg::RenderPassBeginInfo rpBeginInfo;
			rpBeginInfo.colorAttachments[0].loadOp = AttachmentLoadOp::Load;
			rpBeginInfo.depthLoadOp = AttachmentLoadOp::Load;
			SpriteBatch::overlay.End(resolutionX, resolutionY, rpBeginInfo);
			
			gal::EndFrame();
			
			cFrameIdx = (cFrameIdx + 1) % MAX_CONCURRENT_FRAMES;
			frameIndex++;
			
			auto frameEndTime = high_resolution_clock::now();
			uint64_t deltaNS = duration_cast<nanoseconds>(frameEndTime - frameBeginTime).count();
			dt = deltaNS / 1E9f;
		}
		
		for (CallbackNode* node = onShutdown; node != nullptr; node = node->next)
		{
			node->callback();
		}
		
		game.reset();
		
		console::Destroy();
		SpriteBatch::overlay = {};
		SpriteFont::UnloadDevFont();
		SpriteBatch::DestroyStatic();
		TranslationGizmo::DestroyStatic();
		UnloadAssets();
		DestroyUploadBuffers();
		DestroyGraphicsAPI();
		DestroyPlatformFontConfig();
		SDL_DestroyWindow(window);
		SDL_Quit();
		
		return 0;
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
}
