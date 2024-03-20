#include "WGPUPipeline.hpp"

namespace eg::graphics_api::webgpu
{
AbstractPipeline::~AbstractPipeline() {}

std::optional<uint32_t> GetPipelineSubgroupSize(PipelineHandle pipeline)
{
	return std::nullopt;
}

void DestroyPipeline(PipelineHandle handle)
{
	delete &AbstractPipeline::Unwrap(handle);
}

void BindPipeline(CommandContextHandle ctx, PipelineHandle handle)
{
	EG_PANIC("Unimplemented: BindPipeline")
}
} // namespace eg::graphics_api::webgpu
