#pragma once

#include "GL.hpp"
#include "../Abstraction.hpp"

namespace eg::graphics_api::gl
{
	GLenum Translate(BlendFunc f);
	GLenum Translate(BlendFactor f);
	GLenum Translate(Topology t);
}
