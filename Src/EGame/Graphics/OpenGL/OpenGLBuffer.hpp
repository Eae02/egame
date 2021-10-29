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
		BufferUsage currentUsage;
		bool isFakeHostBuffer; //Used for faked mappings in GLES mode
		
		void ChangeUsage(BufferUsage newUsage);
	};
	
	inline Buffer* UnwrapBuffer(BufferHandle handle)
	{
		return reinterpret_cast<Buffer*>(handle);
	}
}
