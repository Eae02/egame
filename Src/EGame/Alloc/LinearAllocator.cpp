#include "LinearAllocator.hpp"
#include "../Utils.hpp"
#include "../Log.hpp"

#include <cstdlib>
#include <iostream>

namespace eg
{
	LinearAllocator::~LinearAllocator() noexcept
	{
		Pool* pool = m_firstPool;
		while (pool != nullptr)
		{
			Pool* nextPool = pool->next;
			FreePool(pool);
			pool = nextPool;
		}
	}
	
	void* LinearAllocator::Allocate(size_t size, size_t alignment)
	{
		for (Pool* pool = m_firstPool; pool != nullptr; pool = pool->next)
		{
			size_t allocPos = RoundToNextMultiple(pool->pos, alignment);
			size_t newPos = allocPos + size;
			if (newPos > pool->size)
				continue;
			
			pool->pos = newPos;
			return pool->memory + allocPos;
		}
		
	#ifndef NDEBUG
		if (m_firstPool != nullptr)
		{
			Log(LogLevel::Warning, "gen", "Linear allocator creating multiple pools. Consider increasing pool size.");
		}
	#endif
		
		Pool* pool = AllocatePool(std::max(m_poolSize, size));
		pool->pos = size;
		
		pool->next = m_firstPool;
		m_firstPool = pool;
		
		return pool->memory;
	}
	
	void LinearAllocator::Reset()
	{
		for (Pool* pool = m_firstPool; pool != nullptr; pool = pool->next)
			pool->pos = 0;
	}
	
	LinearAllocator::Pool* LinearAllocator::AllocatePool(size_t size)
	{
		size_t dataBeginOffset = RoundToNextMultiple(sizeof(Pool), alignof(std::max_align_t));
		
		Pool* pool = static_cast<Pool*>(std::malloc(dataBeginOffset + size));
		pool->memory = reinterpret_cast<char*>(pool) + dataBeginOffset;
		pool->size = size;
		pool->pos = 0;
		pool->next = nullptr;
		
		return pool;
	}
	
	void LinearAllocator::FreePool(LinearAllocator::Pool* pool)
	{
		std::free(pool);
	}
}
