#include "ComponentAllocator.hpp"
#include "EntitySignature.hpp"
#include "../Utils.hpp"

namespace eg
{
	static constexpr size_t INITIAL_PAGE_SIZE = 4;
	static constexpr size_t MAX_PAGE_SIZE = 1024;
	
	ComponentPage* CreateComponentPage(size_t pageSize, size_t compAlignment, size_t compSize)
	{
		size_t availIndicesOffset = RoundToNextMultiple(sizeof(ComponentPage), alignof(uint16_t));
		size_t componentsOffset = RoundToNextMultiple(availIndicesOffset + sizeof(uint16_t) * pageSize, compAlignment);
		
		char* memory = static_cast<char*>(std::malloc(componentsOffset + pageSize * compSize));
		ComponentPage* page = reinterpret_cast<ComponentPage*>(memory);
		page->next = nullptr;
		page->numAvailable = pageSize;
		page->pageSize = pageSize;
		page->componentSize = compSize;
		page->availIndices = reinterpret_cast<uint16_t*>(memory + availIndicesOffset);
		page->components = memory + componentsOffset;
		
		for (uint16_t i = 0; i < pageSize; i++)
			page->availIndices[i] = pageSize - i - 1;
		
		return page;
	}
	
	ComponentRef ComponentAllocator::Allocate(const ComponentType& componentType)
	{
		auto it = std::lower_bound(m_pageLists.begin(), m_pageLists.end(), componentType.typeIndex);
		if (it == m_pageLists.end() || it->type != componentType.typeIndex)
		{
			it = m_pageLists.emplace(it, componentType.typeIndex);
		}
		
		ComponentPage* page = it->first;
		while (page != nullptr && page->numAvailable == 0)
		{
			page = page->next;
		}
		
		if (page == nullptr)
		{
			size_t size = it->first == nullptr ? INITIAL_PAGE_SIZE : std::min(it->first->pageSize * 2, MAX_PAGE_SIZE);
			page = CreateComponentPage(size, componentType.alignment, componentType.size);
			page->next = it->first;
			it->first = page;
		}
		
		page->numAvailable--;
		uint16_t index = page->availIndices[page->numAvailable];
		
		return { page, index };
	}
}
