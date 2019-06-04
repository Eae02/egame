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
		buffer->persistentMapping = nullptr;
		
#ifdef EG_GLES
		if (HasFlag(createInfo.flags, BufferFlags::HostAllocate))
		{
			buffer->buffer = 0;
			buffer->isHostBuffer = true;
			buffer->persistentMapping = new char[createInfo.size];
			if (createInfo.initialData != nullptr)
			{
				std::memcpy(buffer->persistentMapping, createInfo.initialData, createInfo.size);
			}
			return reinterpret_cast<BufferHandle>(buffer);
		}
		buffer->isHostBuffer = false;
#endif
		
		glGenBuffers(1, &buffer->buffer);
		
		GLenum target = GL_ARRAY_BUFFER;
		if (HasFlag(createInfo.flags, BufferFlags::IndexBuffer))
		{
			target = GL_ELEMENT_ARRAY_BUFFER;
		}
		if (HasFlag(createInfo.flags, BufferFlags::UniformBuffer))
		{
			target = GL_UNIFORM_BUFFER;
		}
		
		glBindBuffer(target, buffer->buffer);
		
#ifdef EG_GLES
		GLenum usageFlags = GL_DYNAMIC_DRAW;
		if (HasFlag(createInfo.flags, BufferFlags::Update))
		{
			usageFlags = GL_STREAM_DRAW;
		}
		
		glBufferData(target, createInfo.size, createInfo.initialData, usageFlags);
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
		
		glBufferStorage(target, createInfo.size, createInfo.initialData, storageFlags);
		
		if (mapFlags)
		{
			buffer->persistentMapping = reinterpret_cast<char*>(
				glMapBufferRange(target, 0, createInfo.size, mapFlags));
		}
		else
		{
			buffer->persistentMapping = nullptr;
		}
#endif
		
#ifndef __EMSCRIPTEN__
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
#ifdef EG_GLES
			if (buffer->isHostBuffer)
			{
				delete[] buffer->persistentMapping;
			}
			else
			{
#endif
				if (buffer->buffer == currentTempBuffer)
					currentTempBuffer = UINT32_MAX;
				glDeleteBuffers(1, &buffer->buffer);
#ifdef EG_GLES
			}
#endif
			bufferPool.Free(buffer);
		});
	}
	
	void* MapBuffer(BufferHandle handle, uint64_t offset, uint64_t range)
	{
		Buffer* buffer = UnwrapBuffer(handle);
		if (offset + range > buffer->size)
			EG_PANIC("MapBuffer out of range!");
#ifdef EG_GLES
		if (!buffer->isHostBuffer)
			EG_PANIC("Attempted to map a non host buffer!")
#endif
		return buffer->persistentMapping + offset;
	}
	
	void FlushBuffer(BufferHandle handle, uint64_t modOffset, uint64_t modRange)
	{
#ifndef EG_GLES
		Buffer* buffer = UnwrapBuffer(handle);
		BindTempBuffer(buffer->buffer);
		glFlushMappedBufferRange(TEMP_BUFFER_BINDING, modOffset, modRange);
#endif
	}
	
	void UpdateBuffer(CommandContextHandle, BufferHandle handle, uint64_t offset, uint64_t size, const void* data)
	{
		Buffer* buffer = UnwrapBuffer(handle);
#ifdef EG_GLES
		if (buffer->isHostBuffer)
		{
			std::memcpy(buffer->persistentMapping + offset, data, size);
			return;
		}
#endif
		BindTempBuffer(buffer->buffer);
		glBufferSubData(TEMP_BUFFER_BINDING, offset, size, data);
	}
	
	void CopyBuffer(CommandContextHandle, BufferHandle src, BufferHandle dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size)
	{
		Buffer* srcBuffer = UnwrapBuffer(src);
		Buffer* dstBuffer = UnwrapBuffer(dst);
		
#ifdef EG_GLES
		if (srcBuffer->isHostBuffer && !dstBuffer->isHostBuffer)
		{
			BindTempBuffer(dstBuffer->buffer);
			glBufferSubData(TEMP_BUFFER_BINDING, dstOffset, size, srcBuffer->persistentMapping + srcOffset);
			return;
		}
		if (!srcBuffer->isHostBuffer && dstBuffer->isHostBuffer)
		{
			Log(LogLevel::Warning, "gl", "Device to host buffer copy is not implemented.");
			return;
		}
		if (srcBuffer->isHostBuffer && dstBuffer->isHostBuffer)
		{
			std::memcpy(dstBuffer->persistentMapping + dstOffset, srcBuffer->persistentMapping + srcOffset, size);
			return;
		}
#endif
		
		glBindBuffer(GL_COPY_READ_BUFFER, srcBuffer->buffer);
		BindTempBuffer(dstBuffer->buffer);
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
