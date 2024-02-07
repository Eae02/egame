#include "MetalPipeline.hpp"
#include "MetalCommandContext.hpp"

namespace eg::graphics_api::mtl
{
ConcurrentObjectPool<Pipeline> pipelinePool;

void DestroyPipeline(PipelineHandle handle)
{
	Pipeline& mpipeline = UnwrapPipeline(handle);
	if (GraphicsPipeline* pipeline = std::get_if<GraphicsPipeline>(&mpipeline.variant))
		pipeline->pso->release();
	if (ComputePipeline* pipeline = std::get_if<ComputePipeline>(&mpipeline.variant))
		pipeline->pso->release();
	pipelinePool.Delete(&mpipeline);
}

void BindPipeline(CommandContextHandle ctx, PipelineHandle handle)
{
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(ctx);
	Pipeline& mpipeline = UnwrapPipeline(handle);

	if (GraphicsPipeline* pipeline = std::get_if<GraphicsPipeline>(&mpipeline.variant))
	{
		pipeline->Bind(mcc);
	}
}

std::optional<uint32_t> StageBindingsTable::GetResourceMetalIndex(uint32_t set, uint32_t binding) const
{
	if (set >= bindingsMetalIndexTable.size() || binding >= bindingsMetalIndexTable[set].size())
		return std::nullopt;
	int index = bindingsMetalIndexTable[set][binding];
	if (index < 0)
		return std::nullopt;
	return static_cast<uint32_t>(index);
}
} // namespace eg::graphics_api::mtl
