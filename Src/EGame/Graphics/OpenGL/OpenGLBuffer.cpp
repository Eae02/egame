#include "OpenGL.hpp"
#include "OpenGLBuffer.hpp"
#include "Pipeline.hpp"
#include "Framebuffer.hpp"
#include "../Graphics.hpp"
#include "../../Assert.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "../../MainThreadInvoke.hpp"

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
		buffer->isFakeHostBuffer = false;
		
		if (useGLESPath && HasFlag(createInfo.flags, BufferFlags::HostAllocate))
		{
			buffer->buffer = 0;
			buffer->isFakeHostBuffer = true;
			buffer->persistentMapping = new char[createInfo.size];
			if (createInfo.initialData != nullptr)
			{
				std::memcpy(buffer->persistentMapping, createInfo.initialData, createInfo.size);
			}
			return reinterpret_cast<BufferHandle>(buffer);
		}
		
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
		
		if (useGLESPath)
		{
			GLenum usageFlags = GL_DYNAMIC_DRAW;
			if (HasFlag(createInfo.flags, BufferFlags::Update))
			{
				usageFlags = GL_STREAM_DRAW;
			}
			glBufferData(target, createInfo.size, createInfo.initialData, usageFlags);
		}
		else
		{
#ifndef EG_GLES
			GLenum mapFlags = 0;
			
			GLenum storageFlags = 0;
			if (HasFlag(createInfo.flags, BufferFlags::MapWrite))
			{
				storageFlags |= GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;
				mapFlags |= GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT | GL_MAP_PERSISTENT_BIT;
			}
			if (HasFlag(createInfo.flags, BufferFlags::MapRead))
			{
				storageFlags |= GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
				mapFlags |= GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
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
		}
		
		if (createInfo.label != nullptr)
		{
			glObjectLabel(GL_BUFFER, buffer->buffer, -1, createInfo.label);
		}
		
		return reinterpret_cast<BufferHandle>(buffer);
	}
	
	void DestroyBuffer(BufferHandle handle)
	{
		Buffer* buffer = UnwrapBuffer(handle);
		MainThreadInvoke([buffer]
		{
			if (buffer->isFakeHostBuffer)
			{
				delete[] buffer->persistentMapping;
			}
			if (buffer->buffer == currentTempBuffer)
			{
				currentTempBuffer = UINT32_MAX;
			}
			glDeleteBuffers(1, &buffer->buffer);
			bufferPool.Delete(buffer);
		});
	}
	
	void* MapBuffer(BufferHandle handle, uint64_t offset, uint64_t range)
	{
		Buffer* buffer = UnwrapBuffer(handle);
		if (offset + range > buffer->size)
			EG_PANIC("MapBuffer out of range!");
		if (useGLESPath && !buffer->isFakeHostBuffer)
			EG_PANIC("Attempted to map a non host buffer!")
		return buffer->persistentMapping + offset;
	}
	
	void FlushBuffer(BufferHandle handle, uint64_t modOffset, uint64_t modRange)
	{
		if (!useGLESPath)
		{
			Buffer* buffer = UnwrapBuffer(handle);
			BindTempBuffer(buffer->buffer);
			glFlushMappedBufferRange(TEMP_BUFFER_BINDING, modOffset, modRange);
		}
	}
	
	void InvalidateBuffer(BufferHandle handle, uint64_t modOffset, uint64_t modRange) { }
	
	void UpdateBuffer(CommandContextHandle, BufferHandle handle, uint64_t offset, uint64_t size, const void* data)
	{
		AssertRenderPassNotActive("UpdateBuffer");
		
		Buffer* buffer = UnwrapBuffer(handle);
		
		if (buffer->isFakeHostBuffer)
		{
			std::memcpy(buffer->persistentMapping + offset, data, size);
			return;
		}
		
		buffer->ChangeUsage(BufferUsage::CopyDst);
		
		BindTempBuffer(buffer->buffer);
		glBufferSubData(TEMP_BUFFER_BINDING, offset, size, data);
	}
	
	void FillBuffer(CommandContextHandle, BufferHandle handle, uint64_t offset, uint64_t size, uint32_t data)
	{
		AssertRenderPassNotActive("FillBuffer");
		
		Buffer* buffer = UnwrapBuffer(handle);
		if (buffer->isFakeHostBuffer)
		{
			std::memset(buffer->persistentMapping + offset, data, size);
			return;
		}
		
		buffer->ChangeUsage(BufferUsage::CopyDst);
		
		BindTempBuffer(buffer->buffer);
		if (useGLESPath)
		{
			std::vector<char> dataVec(size);
			memset(dataVec.data(), data, size);
			glBufferSubData(TEMP_BUFFER_BINDING, offset, size, dataVec.data());
		}
		else
		{
#ifndef EG_GLES
			glClearBufferData(TEMP_BUFFER_BINDING, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &data);
#endif
		}
	}
	
	void CopyBuffer(CommandContextHandle, BufferHandle src, BufferHandle dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size)
	{
		AssertRenderPassNotActive("CopyBuffer");
		
		Buffer* srcBuffer = UnwrapBuffer(src);
		Buffer* dstBuffer = UnwrapBuffer(dst);
		
		srcBuffer->ChangeUsage(BufferUsage::CopySrc);
		dstBuffer->ChangeUsage(BufferUsage::CopyDst);
		
		if (useGLESPath)
		{
			if (srcBuffer->isFakeHostBuffer && !dstBuffer->isFakeHostBuffer)
			{
				BindTempBuffer(dstBuffer->buffer);
				glBufferSubData(TEMP_BUFFER_BINDING, dstOffset, size, srcBuffer->persistentMapping + srcOffset);
				return;
			}
			if (!srcBuffer->isFakeHostBuffer && dstBuffer->isFakeHostBuffer)
			{
				Log(LogLevel::Warning, "gl", "Device to host buffer copy is not implemented in GLES.");
				return;
			}
			if (srcBuffer->isFakeHostBuffer && dstBuffer->isFakeHostBuffer)
			{
				std::memcpy(dstBuffer->persistentMapping + dstOffset, srcBuffer->persistentMapping + srcOffset, size);
				return;
			}
		}
		
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
	
	void BindStorageBuffer(CommandContextHandle, BufferHandle handle, uint32_t set, uint32_t binding,
		uint64_t offset, uint64_t range)
	{
		Buffer* buffer = UnwrapBuffer(handle);
		glBindBufferRange(GL_SHADER_STORAGE_BUFFER, ResolveBinding(set, binding), buffer->buffer, offset, range);
	}
	
	inline void MaybeBarrierAfterSSBO(BufferUsage newUsage)
	{
		switch (newUsage)
		{
		case BufferUsage::Undefined:break;
		case BufferUsage::CopySrc:
		case BufferUsage::CopyDst:
			MaybeInsertBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
			break;
		case BufferUsage::VertexBuffer:
			MaybeInsertBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
			break;
		case BufferUsage::IndexBuffer:
			MaybeInsertBarrier(GL_ELEMENT_ARRAY_BARRIER_BIT);
			break;
		case BufferUsage::UniformBuffer:
			MaybeInsertBarrier(GL_UNIFORM_BARRIER_BIT);
			break;
		case BufferUsage::StorageBufferRead:
		case BufferUsage::StorageBufferWrite:
		case BufferUsage::StorageBufferReadWrite:
			MaybeInsertBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			break;
		case BufferUsage::HostRead: break;
		}
	}
	
	void BufferUsageHint(BufferHandle handle, BufferUsage newUsage, ShaderAccessFlags shaderAccessFlags)
	{
		UnwrapBuffer(handle)->ChangeUsage(newUsage);
	}
	
	void BufferBarrier(CommandContextHandle ctx, BufferHandle handle, const eg::BufferBarrier& barrier)
	{
		if (barrier.oldUsage == BufferUsage::StorageBufferWrite || barrier.oldUsage == BufferUsage::StorageBufferReadWrite)
		{
			MaybeBarrierAfterSSBO(barrier.newUsage);
		}
	}
	
	void Buffer::ChangeUsage(BufferUsage newUsage)
	{
#ifndef EG_GLES
		if (currentUsage == BufferUsage::StorageBufferWrite || currentUsage == BufferUsage::StorageBufferReadWrite)
		{
			MaybeBarrierAfterSSBO(newUsage);
		}
		if (newUsage == BufferUsage::HostRead)
		{
			MaybeInsertBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
		}
#endif
		currentUsage = newUsage;
	}
	
	void Buffer::AssertRange(uint64_t begin, uint64_t length) const
	{
		if (begin >= size || length > size || begin + length > size)
		{
			EG_PANIC("Buffer range starting at " << begin << " with length " << length <<
			         " is out of range for buffer with length " << size << ".");
		}
	}
}
