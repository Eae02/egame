#pragma once

#include <GL/gl3w.h>

namespace eg::graphics_api::gl
{
	struct Buffer
	{
		GLuint buffer;
		uint64_t size;
		uint64_t mapOffset;
		char* persistentMapping;
	};
}
