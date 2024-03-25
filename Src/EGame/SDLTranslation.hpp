#pragma once

#include "InputState.hpp"

namespace eg::detail
{
Button TranslateSDLKey(int scancode);
Button TranslateSDLControllerButton(int button);
Button TranslateSDLMouseButton(int button);
} // namespace eg::detail
