#pragma once

#include <typeindex>

namespace eg
{
	struct ComponentPage
	{
		ComponentPage* next;
		size_t pageSize;
		size_t componentSize;
		size_t numAvailable;
		uint16_t* availIndices;
		char* components;
	};
	
	struct ComponentRef
	{
		ComponentPage* page;
		uint16_t index;
		
		void* Get() const
		{
			return page->components + index * page->componentSize;
		}
		
		void Free()
		{
			page->availIndices[page->numAvailable++] = index;
		}
	};
	
	class ComponentAllocator
	{
	public:
		ComponentAllocator() = default;
		
		ComponentRef Allocate(const struct ComponentType& componentType);
		
	private:
		struct ComponentPageList
		{
			std::type_index type;
			ComponentPage* first;
			
			explicit ComponentPageList(std::type_index _type)
				: type(_type), first(nullptr) { }
			
			bool operator<(std::type_index other) const
			{
				return type < other;
			}
		};
		
		std::vector<ComponentPageList> m_pageLists;
	};
}
