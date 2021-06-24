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
}
