#pragma once

#include "../Abstraction.hpp"

#include <GL/gl.h>

namespace eg::graphics_api::gl
{
	GLenum Translate(BlendFunc f);
	GLenum Translate(BlendFactor f);
	GLenum Translate(Topology t);
}
