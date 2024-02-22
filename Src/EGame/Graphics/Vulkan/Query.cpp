#ifndef EG_NO_VULKAN
#include "../../Alloc/ObjectPool.hpp"
#include "../../Assert.hpp"
#include "../Abstraction.hpp"
#include "Buffer.hpp"
#include "Common.hpp"
#include "VulkanCommandContext.hpp"

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
	VkResult res = vkGetQueryPoolResults(
		ctx.device, pool, firstQuery, numQueries, dataSize, data, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
	if (res == VK_NOT_READY)
		return false;
	CheckRes(res);
	return true;
}

void CopyQueryResults(
	CommandContextHandle cc, QueryPoolHandle queryPoolHandle, uint32_t firstQuery, uint32_t numQueries,
	BufferHandle dstBufferHandle, uint64_t dstOffset)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	Buffer* dstBuffer = UnwrapBuffer(dstBufferHandle);
	dstBuffer->AutoBarrier(cc, BufferUsage::CopyDst);

	QueryPool* queryPool = UnwrapQueryPool(queryPoolHandle);
	vcc.referencedResources.Add(*queryPool);
	vkCmdCopyQueryPoolResults(
		vcc.cb, queryPool->pool, firstQuery, numQueries, dstBuffer->buffer, dstOffset, sizeof(uint64_t),
		VK_QUERY_RESULT_64_BIT);
}

void WriteTimestamp(CommandContextHandle cc, QueryPoolHandle queryPoolHandle, uint32_t query)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	QueryPool* queryPool = UnwrapQueryPool(queryPoolHandle);
	vcc.referencedResources.Add(*queryPool);
	vkCmdWriteTimestamp(vcc.cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, queryPool->pool, query);
}

void ResetQueries(CommandContextHandle cc, QueryPoolHandle queryPoolHandle, uint32_t firstQuery, uint32_t numQueries)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	QueryPool* queryPool = UnwrapQueryPool(queryPoolHandle);
	vcc.referencedResources.Add(*queryPool);
	vkCmdResetQueryPool(vcc.cb, queryPool->pool, firstQuery, numQueries);
}

void BeginQuery(CommandContextHandle cc, QueryPoolHandle queryPoolHandle, uint32_t query)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	QueryPool* queryPool = UnwrapQueryPool(queryPoolHandle);
	vcc.referencedResources.Add(*queryPool);
	vkCmdBeginQuery(vcc.cb, queryPool->pool, query, 0);
}

void EndQuery(CommandContextHandle cc, QueryPoolHandle queryPoolHandle, uint32_t query)
{
	VulkanCommandContext& vcc = UnwrapCC(cc);
	QueryPool* queryPool = UnwrapQueryPool(queryPoolHandle);
	vcc.referencedResources.Add(*queryPool);
	vkCmdEndQuery(vcc.cb, queryPool->pool, query);
}
} // namespace eg::graphics_api::vk

#endif
