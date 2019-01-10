#include "PoolAllocator.hpp"

namespace eg
{
	PoolAllocator::PoolAllocator(uint64_t elementCount)
	{
		m_availableBlocks.emplace_back(0, elementCount);
	}
	
	PoolAllocator::FindAvailableResult PoolAllocator::FindAvailable(uint64_t elementCount, uint64_t alignment)
	{
		long blockIndex = -1;
		uint64_t bestPadding = 0;
		
		for (size_t i = 0; i < m_availableBlocks.size(); i++)
		{
			//Don't use this block if the current one is a better fit
			if (blockIndex != -1 && m_availableBlocks[blockIndex].m_elementCount < m_availableBlocks[i].m_elementCount)
				continue;
			
			uint64_t padding = m_availableBlocks[i].m_firstElement % alignment;
			if (padding != 0)
				padding = alignment - padding;
			
			//Checks the size of the block
			uint64_t requiredSize = elementCount + padding;
			if (m_availableBlocks[i].m_elementCount < requiredSize)
				continue;
			
			blockIndex = i;
			bestPadding = padding;
			
			if (m_availableBlocks[i].m_elementCount == requiredSize)
				break;
		}
		
		if (blockIndex == -1)
			return { };
		
		return PoolAllocator::FindAvailableResult(m_availableBlocks[blockIndex], bestPadding);
	}
	
	void PoolAllocator::Allocate(const PoolAllocator::FindAvailableResult& availableResult, uint64_t elementCount)
	{
		elementCount += availableResult.m_padding;
		
		if (availableResult.m_block->m_elementCount == elementCount)
		{
			*availableResult.m_block = m_availableBlocks.back();
			m_availableBlocks.pop_back();
		}
		else
		{
			availableResult.m_block->m_firstElement += elementCount;
			availableResult.m_block->m_elementCount -= elementCount;
		}
		
		if (availableResult.m_padding != 0)
		{
			m_availableBlocks.emplace_back(availableResult.m_firstElement, availableResult.m_padding);
		}
	}
	
	void PoolAllocator::Free(uint64_t firstElement, uint64_t elementCount)
	{
		long prevBlockIndex = -1;
		long nextBlockIndex = -1;
		
		const uint64_t nextBlockFirstElement = firstElement + elementCount;
		
		for (size_t i = 0; i < m_availableBlocks.size(); i++)
		{
			if (m_availableBlocks[i].m_firstElement == nextBlockFirstElement)
			{
				nextBlockIndex = i;
				if (prevBlockIndex != -1)
					break; //Both previous and next block have been found.
			}
			
			if (m_availableBlocks[i].m_firstElement + m_availableBlocks[i].m_elementCount == firstElement)
			{
				prevBlockIndex = i;
				if (nextBlockIndex != -1)
					break; //Both previous and next block have been found.
			}
		}
		
		if (prevBlockIndex == -1 && nextBlockIndex == -1)
		{
			//Neither a next or previous block exist.
			m_availableBlocks.emplace_back(firstElement, elementCount);
		}
		else if (prevBlockIndex != -1 && nextBlockIndex != -1)
		{
			//Both a next and previous block exist.
			
			//Increases the span of the previous block to cover the freed block and the next block.
			m_availableBlocks[static_cast<size_t>(prevBlockIndex)].m_elementCount +=
				elementCount + m_availableBlocks[static_cast<size_t>(nextBlockIndex)].m_elementCount;
			
			//Removes the next block.
			m_availableBlocks[static_cast<size_t>(nextBlockIndex)] = m_availableBlocks.back();
			m_availableBlocks.pop_back();
		}
		else if (prevBlockIndex != -1)
		{
			//Only a previous block exists.
			
			//Increases the span of the previous block to cover the freed block.
			m_availableBlocks[prevBlockIndex].m_elementCount += elementCount;
		}
		else
		{
			//Only a next block exists.
			
			//Increases the span of the next block and moves it back so it also covers the freed block.
			m_availableBlocks[nextBlockIndex].m_firstElement -= elementCount;
			m_availableBlocks[nextBlockIndex].m_elementCount += elementCount;
		}
	}
}
