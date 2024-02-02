#pragma once

#include "API.hpp"

#include <span>

namespace eg
{
struct GameController
{
	const char* name;
	void* _data;
};

EG_API std::span<const GameController> GameControllers();

namespace detail
{
void LoadGameControllers();
void AddGameController(void* controller);
extern void* activeController;
} // namespace detail
} // namespace eg
