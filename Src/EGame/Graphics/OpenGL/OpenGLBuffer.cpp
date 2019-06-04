#include "OpenGL.hpp"
#include "OpenGLBuffer.hpp"
#include "../Graphics.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "../../MainThreadInvoke.hpp"
#include "Pipeline.hpp"

namespace eg::graphics_api::gl
{
	static ObjectPool<Buffer> bufferPool;
	
	static GLenum TEMP_BUFFER_BINDING = GL_COPY_WRITE_BUFFER;
	
	GLuint currentTempBuffer = UINT32_MAX;
	inline void BindTempBuffer(GLuint buffer)
	{
		if (currentTempBuffer != buffer)
		{
			glBindBuffer(TEMP_BUFFER_BINDING, buffer);
			currentTempBuffer = buffer;
		}
	}
	
	BufferHandle CreateBuffer(const BufferCreateInfo& createInfo)
	{
		Buffer* buffer = bufferPool.New();
		buffer->size = createInfo.size;
		
		glGenBuffers(1, &buffer->buffer);
		BindTempBuffer(buffer->buffer);
		
#ifdef EG_GLES
		GLenum usageFlags = GL_STATIC_DRAW;
		if (HasFlag(createInfo.flags, BufferFlags::Update))
		{
			usageFlags = GL_STREAM_DRAW;
		}
		if (HasFlag(createInfo.flags, BufferFlags::MapRead))
		{
			usageFlags = GL_DYNAMIC_READ;
		}
		if (HasFlag(createInfo.flags, BufferFlags::MapWrite))
		{
			usageFlags = GL_DYNAMIC_COPY;
		}
		
		glBufferData(TEMP_BUFFER_BINDING, createInfo.size, createInfo.initialData, usageFlags);
		
		if (HasFlag(createInfo.flags, BufferFlags::MapRead) || HasFlag(createInfo.flags, BufferFlags::MapWrite))
		{
			buffer->persistentMapping = reinterpret_cast<char*>(std::malloc(createInfo.size));
		}
		else
		{
			buffer->persistentMapping = nullptr;
		}
#else
		GLenum mapFlags = 0;
		
		GLenum storageFlags = 0;
		if (HasFlag(createInfo.flags, BufferFlags::MapWrite))
		{
			storageFlags |= GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;
			mapFlags |= GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT | GL_MAP_PERSISTENT_BIT;
		}
		if (HasFlag(createInfo.flags, BufferFlags::MapRead))
		{
			storageFlags |= GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT;
			mapFlags |= GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT;
		}
		if (HasFlag(createInfo.flags, BufferFlags::Update))
		{
			storageFlags |= GL_DYNAMIC_STORAGE_BIT;
		}
		if (HasFlag(createInfo.flags, BufferFlags::HostAllocate))
		{
			storageFlags |= GL_CLIENT_STORAGE_BIT;
		}
		
		glBufferStorage(TEMP_BUFFER_BINDING, createInfo.size, createInfo.initialData, storageFlags);
		
		if (mapFlags)
		{
			buffer->persistentMapping = reinterpret_cast<char*>(
				glMapBufferRange(TEMP_BUFFER_BINDING, 0, createInfo.size, mapFlags));
		}
		else
		{
			buffer->persistentMapping = nullptr;
		}
#endif
		
#ifndef EG_WEB
		if (createInfo.label != nullptr)
		{
			glObjectLabel(GL_BUFFER, buffer->buffer, -1, createInfo.label);
		}
#endif
		
		return reinterpret_cast<BufferHandle>(buffer);
	}
	
	void DestroyBuffer(BufferHandle handle)
	{
		Buffer* buffer = UnwrapBuffer(handle);
		MainThreadInvoke([buffer]
		{
			if (buffer->buffer == currentTempBuffer)
				currentTempBuffer = UINT32_MAX;
#ifdef EG_GLES
			std::free(buffer->persistentMapping);
#endif
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
	
	void FlushBuffer(BufferHandle handle, uint64_t modOffset, uint64_t modRange)
	{
		Buffer* buffer = UnwrapBuffer(handle);
		BindTempBuffer(buffer->buffer);
#ifdef EG_GLES
		glBufferSubData(TEMP_BUFFER_BINDING, modOffset, modRange, buffer->persistentMapping + modOffset);
#else
		glFlushMappedBufferRange(TEMP_BUFFER_BINDING, modOffset, modRange);
#endif
	}
	
	void UpdateBuffer(CommandContextHandle, BufferHandle handle, uint64_t offset, uint64_t size, const void* data)
	{
		Buffer* buffer = UnwrapBuffer(handle);
		BindTempBuffer(buffer->buffer);
		glBufferSubData(TEMP_BUFFER_BINDING, offset, size, data);
	}
	
	void CopyBuffer(CommandContextHandle, BufferHandle src, BufferHandle dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size)
	{
		glBindBuffer(GL_COPY_READ_BUFFER, UnwrapBuffer(src)->buffer);
		BindTempBuffer(UnwrapBuffer(dst)->buffer);
		glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, srcOffset, dstOffset, size);
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
