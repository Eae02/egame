#include "Core.hpp"
#include "MainThreadInvoke.hpp"
#include "Graphics/AbstractionHL.hpp"
#include "InputState.hpp"
#include "Event.hpp"

#include <SDL2/SDL.h>
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
	
	int detail::Run(const RunConfig& runConfig, std::unique_ptr<IGame> (*createGame)())
	{
		if (SDL_Init(SDL_INIT_VIDEO))
		{
			std::cerr << "SDL failed to initialize: " << SDL_GetError() << std::endl;
			return 1;
		}
		
		if (exeDirPathPtr == nullptr)
		{
			exeDirPathPtr = SDL_GetBasePath();
			exeDirPath = exeDirPathPtr;
		}
		
		RegisterDefaultAssetGenerator();
		LoadAssetGenLibrary();
		RegisterAssetLoaders();
		
		eg::DefineEventType<ResolutionChangedEvent>();
		eg::DefineEventType<ButtonEvent>();
		
		uint32_t windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN;
		
		if (runConfig.graphicsAPI == GraphicsAPI::OpenGL)
		{
			int contextFlags = SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
			if (runConfig.debug)
				contextFlags |= SDL_GL_CONTEXT_DEBUG_FLAG;
			
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, contextFlags);
			SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
			SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
			SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
			SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
			
			windowFlags |= SDL_WINDOW_OPENGL;
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
		
		if (!InitializeGraphicsAPI(runConfig.graphicsAPI, window))
		{
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error initializing graphics",
				"The selected graphics API could not be initialized.", nullptr);
			return 1;
		}
		
		gal::GetCapabilities(detail::graphicsCapabilities);
		
		if (runConfig.initialize)
			runConfig.initialize();
		
		std::unique_ptr<IGame> game = createGame();
		
		InputState currentIS;
		InputState previousIS;
		detail::currentIS = &currentIS;
		detail::previousIS = &previousIS;
		
		auto ButtonDownEvent = [&] (Button button)
		{
			if (button != Button::Unknown && !currentIS.IsButtonDown(button))
			{
				currentIS.OnButtonDown(button);
				RaiseEvent<ButtonEvent>({ button, true });
			}
		};
		
		auto ButtonUpEvent = [&] (Button button)
		{
			if (button != Button::Unknown && currentIS.IsButtonDown(button))
			{
				currentIS.OnButtonUp(button);
				RaiseEvent<ButtonEvent>({ button, false });
			}
		};
		
		bool firstMouseMotionEvent = true;
		
		resolutionX = -1;
		resolutionY = -1;
		shouldClose = false;
		frameIndex = 0;
		float dt = 0.0f;
		while (!shouldClose)
		{
			auto frameBeginTime = high_resolution_clock::now();
			
			previousIS = currentIS;
			
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				switch (event.type)
				{
				case SDL_QUIT:
					shouldClose = true;
					break;
				case SDL_KEYDOWN:
					if (!event.key.repeat)
					{
						ButtonDownEvent(TranslateSDLKey(event.key.keysym.scancode));
					}
					break;
				case SDL_KEYUP:
					if (!event.key.repeat)
					{
						ButtonUpEvent(TranslateSDLKey(event.key.keysym.scancode));
					}
					break;
				case SDL_MOUSEBUTTONDOWN:
					ButtonDownEvent(TranslateSDLMouseButton(event.button.button));
					break;
				case SDL_MOUSEBUTTONUP:
					ButtonUpEvent(TranslateSDLMouseButton(event.button.button));
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
					break;
				case SDL_MOUSEWHEEL:
					currentIS.scrollX += event.wheel.x;
					currentIS.scrollY += event.wheel.y;
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
				RaiseEvent(ResolutionChangedEvent { resolutionX, resolutionY });
			}
			
			game->RunFrame(dt);
			
			//Processes main thread invokes
			for (MTIBase* mti = firstMTI; mti != nullptr; mti = mti->next)
				mti->Invoke();
			firstMTI = lastMTI = nullptr;
			allocMTI.Reset();
			
			gal::EndFrame();
			
			cFrameIdx = (cFrameIdx + 1) % MAX_CONCURRENT_FRAMES;
			frameIndex++;
			
			auto frameEndTime = high_resolution_clock::now();
			uint64_t deltaNS = duration_cast<nanoseconds>(frameEndTime - frameBeginTime).count();
			dt = deltaNS / 1E9f;
		}
		
		game.reset();
		
		DestroyUploadBuffers();
		DestroyGraphicsAPI();
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
