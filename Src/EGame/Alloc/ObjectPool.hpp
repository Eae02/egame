#pragma once

#include <mutex>
#include <cstdint>
#include <cstddef>
#include <cstring>

#include "../Utils.hpp"

namespace eg
{
	template <typename T>
	class ObjectPool
	{
	public:
		ObjectPool() = default;
		
		~ObjectPool()
		{
			Reset();
		}
		
		ObjectPool(ObjectPool&& other)
			: m_firstPage(other.m_firstPage)
		{
			other.m_firstPage = nullptr;
		}
		
		ObjectPool& operator=(ObjectPool&& other)
		{
			Reset();
			m_firstPage = other.m_firstPage;
			other.m_firstPage = nullptr;
			return *this;
		}
		
		template <typename... Args>
		T* New(Args&&... args)
		{
			return new (Alloc()) T (std::forward<Args>(args)...);
		}
		
		void* Alloc()
		{
			for (Page* page = m_firstPage; page; page = page->next)
			{
				for (size_t o = 0; o < page->size / 8; o++)
				{
					for (size_t b = 0; b < 8; b++)
					{
						if ((page->inUse[o] & (1 << b)) == 0)
						{
							page->inUse[o] |= (uint8_t)(1 << b);
							return page->objects + (o * 8 + b);
						}
					}
				}
			}
			
			const size_t objectsOffset = RoundToNextMultiple<size_t>(sizeof(Page) + m_nextPageSize / 8, alignof(T));
			
			char* mem = reinterpret_cast<char*>(std::malloc(objectsOffset + m_nextPageSize * sizeof(T)));
			
			Page& newPage = *reinterpret_cast<Page*>(mem);
			newPage.size = m_nextPageSize;
			newPage.next = m_firstPage;
			newPage.inUse = reinterpret_cast<uint8_t*>(mem + sizeof(Page));
			newPage.objects = reinterpret_cast<T*>(mem + objectsOffset);
			
			std::memset(newPage.inUse, 0, m_nextPageSize);
			newPage.inUse[0] = 1; //Marks the first item as allocated
			
			m_firstPage = &newPage;
			m_nextPageSize *= 2;
			
			return newPage.objects;
		}
		
		void Delete(T* t)
		{
			t->~T();
			for (Page* page = m_firstPage; page; page = page->next)
			{
				if (t >= page->objects && t < page->objects + page->size)
				{
					const size_t idx = static_cast<size_t>(t - page->objects);
					page->inUse[idx / 8] &= (uint8_t)~InUseMask(idx);
					return;
				}
			}
		}
		
		void Reset()
		{
			for (Page* page = m_firstPage; page;)
			{
				for (size_t i = 0; i < page->size; i++)
				{
					if (page->inUse[i / 8] & InUseMask(i))
					{
						page->objects[i].~T();
					}
				}
				
				Page* nextPage = page->next;
				std::free(page);
				page = nextPage;
			}
			m_firstPage = nullptr;
		}
		
	private:
		inline static size_t InUseMask(size_t i)
		{
			return (size_t)1 << (size_t)(i % 8);
		}
		
		struct Page
		{
			Page* next;
			size_t size;
			uint8_t* inUse;
			T* objects;
		};
		
		size_t m_nextPageSize = 8; //Initial page size must be a multiple of 8
		Page* m_firstPage = nullptr;
	};
	
	template <typename T>
	class ConcurrentObjectPool
	{
	public:
		ConcurrentObjectPool() = default;
		
		template <typename... Args>
		inline T* New(Args&&... args)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_pool.New(std::forward<Args>(args)...);
		}
		
		inline void* Alloc()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_pool.Alloc();
		}
		
		inline void Delete(T* t)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_pool.Delete(t);
		}
		
		inline void Reset()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_pool.Reset();
		}
		
	private:
		std::mutex m_mutex;
		ObjectPool<T> m_pool;
	};
}
