#pragma once

#include <SDL.h>
#include "InputState.hpp"

namespace eg::detail
{
	Button TranslateSDLKey(SDL_Scancode scancode);
	Button TranslateSDLControllerButton(int button);
	Button TranslateSDLMouseButton(int button);
}
