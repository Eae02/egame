#pragma once

#include "GL.hpp"
#include "../Abstraction.hpp"

namespace eg::graphics_api::gl
{
	struct Buffer
	{
		GLuint buffer;
		uint64_t size;
		char* persistentMapping;
#ifdef EG_GLES
		bool isHostBuffer;
#endif
	};
	
	inline Buffer* UnwrapBuffer(BufferHandle handle)
	{
		return reinterpret_cast<Buffer*>(handle);
	}
}
