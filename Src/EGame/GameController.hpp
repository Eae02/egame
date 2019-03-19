#pragma once

#include "API.hpp"
#include "Span.hpp"

namespace eg
{
	struct GameController
	{
		const char* name;
		void* _data;
	};
	
	EG_API Span<const GameController> GameControllers();
}
