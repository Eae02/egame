#include "GameController.hpp"
#include "Log.hpp"

void* eg::detail::activeController;

#ifdef __EMSCRIPTEN__
void eg::detail::AddGameController(void* handle) {}
void eg::detail::LoadGameControllers() {}
std::span<const eg::GameController> eg::GameControllers()
{
	return {};
}
#else

#include <SDL.h>
#include <vector>

namespace eg
{
static std::vector<GameController> controllers;

void detail::AddGameController(void* handle)
{
	SDL_GameController* controller = static_cast<SDL_GameController*>(handle);
	controllers.push_back({ SDL_GameControllerName(controller), controller });
	if (activeController == nullptr)
	{
		eg::Log(LogLevel::Info, "in", "Using game controller: {0}", controllers.back().name);
		activeController = controller;
	}
}

void detail::LoadGameControllers()
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

std::span<const GameController> GameControllers()
{
	return controllers;
}
} // namespace eg
#endif
