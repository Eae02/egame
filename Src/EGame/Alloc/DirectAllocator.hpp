#pragma once

#include <cstddef>

namespace eg
{
	template<typename T, size_t DirectCapacity>
	class DirectAllocator : public std::allocator<T>
	{
	public:
		DirectAllocator() = default;
		
		template <typename U, size_t OtherCapacity>
		DirectAllocator(const DirectAllocator<U, OtherCapacity>&) { }
		
		template<typename U>
		struct rebind
		{
			typedef DirectAllocator<U, DirectCapacity> other;
		};
		
		T* allocate(size_t n, void* hint = nullptr)
		{
			if (!m_bufferUsed && n <= BUFFER_SIZE)
			{
				m_bufferUsed = true;
				return reinterpret_cast<T*>(m_directBuffer);
			}
			return std::allocator<T>::allocate(n, hint);
		}
		
		void deallocate(T* p, size_t n)
		{
			if (reinterpret_cast<char*>(p) == m_directBuffer)
			{
				m_bufferUsed = false;
			}
			else
			{
				std::allocator<T>::deallocate(p, n);
			}
		}
		
	private:
		static constexpr size_t BUFFER_SIZE = sizeof(T) * DirectCapacity;
		
		alignas(T) char m_directBuffer[BUFFER_SIZE];
		bool m_bufferUsed = false;
	};
	
	template <typename T, size_t DirectCapacity>
	struct DirectVector : std::vector<T, DirectAllocator<T, DirectCapacity>>
	{
		DirectVector()
		{
			this->reserve(DirectCapacity);
		}
	};
}
