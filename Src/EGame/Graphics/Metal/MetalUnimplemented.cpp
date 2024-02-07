#include "../../Assert.hpp"
#include "MetalMain.hpp"

namespace eg::graphics_api::mtl
{
FormatCapabilities GetFormatCapabilities(Format format)
{
	return (FormatCapabilities)0b11111111111;
}

PipelineHandle CreateComputePipeline(const ComputePipelineCreateInfo& createInfo)
{
	EG_PANIC("Unimplemented: CreateComputePipeline")
}

void DispatchCompute(CommandContextHandle ctx, uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ){
	EG_PANIC("Unimplemented: DispatchCompute")
}

QueryPoolHandle CreateQueryPool(QueryType type, uint32_t queryCount)
{
	return nullptr;
}
void DestroyQueryPool(QueryPoolHandle queryPool) {}
bool GetQueryResults(QueryPoolHandle queryPool, uint32_t firstQuery, uint32_t numQueries, uint64_t dataSize, void* data)
{
	std::memset(data, 0, dataSize);
	return true;
}
void CopyQueryResults(
	CommandContextHandle cctx, QueryPoolHandle queryPoolHandle, uint32_t firstQuery, uint32_t numQueries,
	BufferHandle dstBufferHandle, uint64_t dstOffset)
{
}
void WriteTimestamp(CommandContextHandle cctx, QueryPoolHandle queryPoolHandle, uint32_t query) {}
void ResetQueries(CommandContextHandle cctx, QueryPoolHandle queryPoolHandle, uint32_t firstQuery, uint32_t numQueries)
{
}
void BeginQuery(CommandContextHandle cctx, QueryPoolHandle queryPoolHandle, uint32_t query) {}
void EndQuery(CommandContextHandle cctx, QueryPoolHandle queryPoolHandle, uint32_t query) {}
} // namespace eg::graphics_api::mtl
