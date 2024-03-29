#pragma once

#include "../Abstraction.hpp"
#include "GL.hpp"

namespace eg::graphics_api::gl
{
struct Buffer
{
	GLuint buffer;
	uint64_t size;
	char* persistentMapping;
	BufferUsage currentUsage;
	bool isFakeHostBuffer; // Used for faked mappings in GLES mode

	void ChangeUsage(BufferUsage newUsage);

	void AssertRange(uint64_t begin, uint64_t length) const;
};

inline Buffer* UnwrapBuffer(BufferHandle handle)
{
	return reinterpret_cast<Buffer*>(handle);
}
} // namespace eg::graphics_api::gl
