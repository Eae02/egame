#include "GameController.hpp"
#include "Log.hpp"

#ifdef __EMSCRIPTEN__
namespace eg
{
	void LoadGameControllers() { }
}
#else

#include <SDL.h>
#include <vector>

namespace eg
{
	std::vector<GameController> controllers;
	SDL_GameController* activeController = nullptr;
	
	void AddGameController(SDL_GameController* controller)
	{
		controllers.push_back({ SDL_GameControllerName(controller), controller });
		if (activeController == nullptr)
		{
			eg::Log(LogLevel::Info, "in", "Using game controller: {0}", controllers.back().name);
			activeController = controller;
		}
	}
	
	void LoadGameControllers()
	{
		SDL_GameControllerEventState(SDL_ENABLE);
		SDL_GameControllerUpdate();
		SDL_JoystickEventState(SDL_ENABLE);
		SDL_JoystickUpdate();
		
		for (int i = 0; i < SDL_NumJoysticks(); i++)
		{
			if (!SDL_IsGameController(i))
			{
				Log(LogLevel::Info, "in", "Joystick '{0}' is not a game controller", SDL_JoystickNameForIndex(i));
				continue;
			}
			SDL_GameController* controller = SDL_GameControllerOpen(i);
			if (controller == nullptr)
			{
				Log(LogLevel::Error, "in", "Could not open game controller {0}: {1}", i, SDL_GetError());
				continue;
			}
			AddGameController(controller);
		}
	}
}
#endif
