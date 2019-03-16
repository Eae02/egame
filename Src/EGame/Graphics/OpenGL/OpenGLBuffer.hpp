#pragma once

#include <GL/gl3w.h>

namespace eg::graphics_api::gl
{
	struct Buffer
	{
		GLuint buffer;
		uint64_t size;
		char* persistentMapping;
	};
	
	inline Buffer* UnwrapBuffer(BufferHandle handle)
	{
		return reinterpret_cast<Buffer*>(handle);
	}
}
