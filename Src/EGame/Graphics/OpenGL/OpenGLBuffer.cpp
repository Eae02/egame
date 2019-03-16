#include "OpenGL.hpp"
#include "OpenGLBuffer.hpp"
#include "OpenGLShader.hpp"
#include "../Graphics.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "../../MainThreadInvoke.hpp"

namespace eg::graphics_api::gl
{
	static ObjectPool<Buffer> bufferPool;
	
	BufferHandle CreateBuffer(BufferFlags flags, uint64_t size, const void* initialData)
	{
		Buffer* buffer = bufferPool.New();
		buffer->size = size;
		
		glCreateBuffers(1, &buffer->buffer);
		
		GLenum mapFlags = 0;
		GLenum storageFlags = 0;
		if (HasFlag(flags, BufferFlags::MapWrite))
		{
			storageFlags |= GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;
			mapFlags |= GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT | GL_MAP_PERSISTENT_BIT;
		}
		if (HasFlag(flags, BufferFlags::MapRead))
		{
			storageFlags |= GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT;
			mapFlags |= GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT;
		}
		if (HasFlag(flags, BufferFlags::Update))
		{
			storageFlags |= GL_DYNAMIC_STORAGE_BIT;
		}
		if (HasFlag(flags, BufferFlags::HostAllocate))
		{
			storageFlags |= GL_CLIENT_STORAGE_BIT;
		}
		
		glNamedBufferStorage(buffer->buffer, size, initialData, storageFlags);
		if (mapFlags)
			buffer->persistentMapping = reinterpret_cast<char*>(glMapNamedBufferRange(buffer->buffer, 0, size, mapFlags));
		else
			buffer->persistentMapping = nullptr;
		
		return reinterpret_cast<BufferHandle>(buffer);
	}
	
	void DestroyBuffer(BufferHandle handle)
	{
		Buffer* buffer = UnwrapBuffer(handle);
		MainThreadInvoke([buffer]
		{
			glDeleteBuffers(1, &buffer->buffer);
			bufferPool.Free(buffer);
		});
	}
	
	void* MapBuffer(BufferHandle handle, uint64_t offset, uint64_t range)
	{
		Buffer* buffer = UnwrapBuffer(handle);
		if (offset + range > buffer->size)
			EG_PANIC("MapBuffer out of range!");
		return buffer->persistentMapping + offset;
	}
	
	void UnmapBuffer(BufferHandle handle, uint64_t modOffset, uint64_t modRange)
	{
		Buffer* buffer = UnwrapBuffer(handle);
		glFlushMappedNamedBufferRange(buffer->buffer, modOffset, modRange);
	}
	
	void UpdateBuffer(CommandContextHandle, BufferHandle handle, uint64_t offset, uint64_t size, const void* data)
	{
		Buffer* buffer = UnwrapBuffer(handle);
		glNamedBufferSubData(buffer->buffer, offset, size, data);
	}
	
	void CopyBuffer(CommandContextHandle, BufferHandle src, BufferHandle dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size)
	{
		glCopyNamedBufferSubData(UnwrapBuffer(src)->buffer, UnwrapBuffer(dst)->buffer, srcOffset, dstOffset, size);
	}
	
	void BindUniformBuffer(CommandContextHandle, BufferHandle handle, uint32_t set, uint32_t binding,
		uint64_t offset, uint64_t range)
	{
		Buffer* buffer = UnwrapBuffer(handle);
		glBindBufferRange(GL_UNIFORM_BUFFER, ResolveBinding(set, binding), buffer->buffer, offset, range);
	}
	
	void BufferUsageHint(BufferHandle handle, BufferUsage newUsage, ShaderAccessFlags shaderAccessFlags) { }
	void BufferBarrier(CommandContextHandle ctx, BufferHandle handle, const eg::BufferBarrier& barrier) { }
}
