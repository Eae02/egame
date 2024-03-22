#include "WGPUPipeline.hpp"
#include "WGPUCommandContext.hpp"
#include "WGPUDescriptorSet.hpp"

namespace eg::graphics_api::webgpu
{
AbstractPipeline::AbstractPipeline(const DescriptorSetBindings& bindings, const char* label)
{
	// Gets bind group layouts
	std::array<WGPUBindGroupLayout, MAX_DESCRIPTOR_SETS> wgpuBindGroupLayouts;
	uint32_t maxUsedBindGroup = 0;
	for (uint32_t set = 0; set < MAX_DESCRIPTOR_SETS; set++)
	{
		if (!bindings.sets[set].empty())
		{
			const CachedBindGroupLayout& cachedLayout = GetBindGroupLayout(bindings.sets[set]);
			bindGroupLayouts[set] = &cachedLayout;
			wgpuBindGroupLayouts[set] = cachedLayout.bindGroupLayout;
			maxUsedBindGroup = set;
		}
	}

	// Creates the pipeline layout
	const WGPUPipelineLayoutDescriptor pipelineLayoutDescriptor = {
		.label = label,
		.bindGroupLayoutCount = maxUsedBindGroup + 1,
		.bindGroupLayouts = wgpuBindGroupLayouts.data(),
	};
	pipelineLayout = wgpuDeviceCreatePipelineLayout(wgpuctx.device, &pipelineLayoutDescriptor);
}

std::optional<uint32_t> GetPipelineSubgroupSize(PipelineHandle pipeline)
{
	return std::nullopt;
}

void DestroyPipeline(PipelineHandle handle)
{
	AbstractPipeline* pipeline = &AbstractPipeline::Unwrap(handle);
	OnFrameEnd(
		[pipeline]
		{
			wgpuPipelineLayoutRelease(pipeline->pipelineLayout);
			std::visit([&](auto& p) { p.Destroy(); }, pipeline->pipeline);
			delete pipeline;
		});
}

void BindPipeline(CommandContextHandle cc, PipelineHandle handle)
{
	AbstractPipeline& pipeline = AbstractPipeline::Unwrap(handle);
	CommandContext& wcc = CommandContext::Unwrap(cc);

	std::visit([&](auto& p) { p.Bind(wcc); }, AbstractPipeline::Unwrap(handle).pipeline);

	wcc.currentPipeline = &pipeline;
}
} // namespace eg::graphics_api::webgpu
