#pragma once

#include "InputState.hpp"
#include <SDL.h>

namespace eg::detail
{
Button TranslateSDLKey(SDL_Scancode scancode);
Button TranslateSDLControllerButton(int button);
Button TranslateSDLMouseButton(int button);
} // namespace eg::detail
