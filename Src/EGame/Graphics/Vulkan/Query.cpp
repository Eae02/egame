#ifndef EG_NO_VULKAN
#include "Common.hpp"
#include "Buffer.hpp"
#include "../Abstraction.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "../../Assert.hpp"

namespace eg::graphics_api::vk
{
	static inline VkQueryType TranslateQueryType(QueryType type)
	{
		switch (type)
		{
		case QueryType::Timestamp: return VK_QUERY_TYPE_TIMESTAMP;
		case QueryType::Occlusion: return VK_QUERY_TYPE_OCCLUSION;
		}
		EG_UNREACHABLE
	}
	
	struct QueryPool : Resource
	{
		VkQueryPool pool;
		
		void Free() override;
	};
	
	static ObjectPool<QueryPool> queryPoolsPool;
	
	void QueryPool::Free()
	{
		vkDestroyQueryPool(ctx.device, pool, nullptr);
		queryPoolsPool.Delete(this);
	}
	
	QueryPoolHandle CreateQueryPool(QueryType type, uint32_t queryCount)
	{
		VkQueryType vkType = TranslateQueryType(type);
		
		QueryPool* queryPool = queryPoolsPool.New();
		queryPool->refCount = 1;
		
		VkQueryPoolCreateInfo poolCI = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
		poolCI.queryType = vkType;
		poolCI.queryCount = queryCount;
		CheckRes(vkCreateQueryPool(ctx.device, &poolCI, nullptr, &queryPool->pool));
		
		return reinterpret_cast<QueryPoolHandle>(queryPool);
	}
	
	inline QueryPool* UnwrapQueryPool(QueryPoolHandle handle)
	{
		return reinterpret_cast<QueryPool*>(handle);
	}
	
	void DestroyQueryPool(QueryPoolHandle queryPool)
	{
		UnwrapQueryPool(queryPool)->UnRef();
	}
	
	bool GetQueryResults(QueryPoolHandle queryPool, uint32_t firstQuery, uint32_t numQueries, uint64_t dataSize, void* data)
	{
		VkQueryPool pool = UnwrapQueryPool(queryPool)->pool;
		VkResult res = vkGetQueryPoolResults(ctx.device, pool, firstQuery, numQueries, dataSize, data,
		                                     sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
		if (res == VK_NOT_READY)
			return false;
		CheckRes(res);
		return true;
	}
	
	void CopyQueryResults(CommandContextHandle cctx, QueryPoolHandle queryPoolHandle,
		uint32_t firstQuery, uint32_t numQueries, BufferHandle dstBufferHandle, uint64_t dstOffset)
	{
		Buffer* dstBuffer = UnwrapBuffer(dstBufferHandle);
		dstBuffer->AutoBarrier(GetCB(cctx), BufferUsage::CopyDst);
		
		QueryPool* queryPool = UnwrapQueryPool(queryPoolHandle);
		RefResource(cctx, *queryPool);
		vkCmdCopyQueryPoolResults(GetCB(cctx), queryPool->pool, firstQuery, numQueries,
		                          dstBuffer->buffer, dstOffset, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
	}
	
	void WriteTimestamp(CommandContextHandle cctx, QueryPoolHandle queryPoolHandle, uint32_t query)
	{
		QueryPool* queryPool = UnwrapQueryPool(queryPoolHandle);
		RefResource(cctx, *queryPool);
		vkCmdWriteTimestamp(GetCB(cctx), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, queryPool->pool, query);
	}
	
	void ResetQueries(CommandContextHandle cctx, QueryPoolHandle queryPoolHandle, uint32_t firstQuery, uint32_t numQueries)
	{
		QueryPool* queryPool = UnwrapQueryPool(queryPoolHandle);
		RefResource(cctx, *queryPool);
		vkCmdResetQueryPool(GetCB(cctx), queryPool->pool, firstQuery, numQueries);
	}
	
	void BeginQuery(CommandContextHandle cctx, QueryPoolHandle queryPoolHandle, uint32_t query)
	{
		QueryPool* queryPool = UnwrapQueryPool(queryPoolHandle);
		RefResource(cctx, *queryPool);
		vkCmdBeginQuery(GetCB(cctx), queryPool->pool, query, 0);
	}
	
	void EndQuery(CommandContextHandle cctx, QueryPoolHandle queryPoolHandle, uint32_t query)
	{
		QueryPool* queryPool = UnwrapQueryPool(queryPoolHandle);
		RefResource(cctx, *queryPool);
		vkCmdEndQuery(GetCB(cctx), queryPool->pool, query);
	}
}

#endif
