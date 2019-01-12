#pragma once


namespace eg
{
	class LinearAllocator
	{
	public:
		inline explicit LinearAllocator(size_t poolSize = STD_POOL_SIZE) noexcept
			: m_poolSize(poolSize) { }
		
		~LinearAllocator() noexcept;
		
		LinearAllocator(LinearAllocator&& other) noexcept
			: m_poolSize(other.m_poolSize), m_firstPool(other.m_firstPool)
		{
			other.m_firstPool = nullptr;
		}
		
		LinearAllocator& operator=(LinearAllocator&& other) noexcept
		{
			this->~LinearAllocator();
			m_poolSize = other.m_poolSize;
			m_firstPool = other.m_firstPool;
			other.m_firstPool = nullptr;
			return *this;
		}
		
		void* Allocate(size_t size, size_t alignment = alignof(max_align_t));
		
		template <typename  T>
		inline T* AllocateArray(size_t len)
		{
			return static_cast<T*>(Allocate(sizeof(T) * len, alignof(T)));
		}
		
		template <typename T, typename... Args>
		inline T* New(Args&&... args)
		{
			T* memory = static_cast<T*>(Allocate(sizeof(T), alignof(T)));
			new (memory) T(std::forward<Args>(args)...);
			return memory;
		}
		
		std::string_view MakeStringCopy(std::string_view string)
		{
			char* data = AllocateArray<char>(string.size());
			std::memcpy(data, string.data(), string.size());
			return std::string_view(data, string.size());
		}
		
		void Reset();
		
		static constexpr size_t STD_POOL_SIZE = 16 * 1024 * 1024; //16 MiB
		
	private:
		struct Pool
		{
			char* memory;
			Pool* next;
			size_t size;
			size_t pos;
		};
		
		static Pool* AllocatePool(size_t size);
		static void FreePool(Pool* pool);
		
		size_t m_poolSize;
		Pool* m_firstPool = nullptr;
	};
}
