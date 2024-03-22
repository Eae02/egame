#pragma once

#include "../Abstraction.hpp"
#include "../SpirvCrossUtils.hpp"
#include "WGPU.hpp"

namespace eg::graphics_api::webgpu
{
struct CachedBindGroupLayout;

struct GraphicsPipeline
{
	WGPURenderPipeline pipeline;

	bool enableScissorTest;

	std::optional<std::array<WGPURenderPipeline, 3>> dynamicCullModePipelines;

	void Destroy();

	bool HasDynamicCullMode() const { return dynamicCullModePipelines.has_value(); }

	void Bind(struct CommandContext& cc);
};

struct ComputePipeline
{
	WGPUComputePipeline pipeline;

	void Destroy();

	void Bind(struct CommandContext& cc);
};

struct AbstractPipeline
{
	WGPUPipelineLayout pipelineLayout;

	std::array<const CachedBindGroupLayout*, MAX_DESCRIPTOR_SETS> bindGroupLayouts;

	std::variant<GraphicsPipeline, ComputePipeline> pipeline;

	AbstractPipeline(const DescriptorSetBindings& bindings, const char* label);

	static AbstractPipeline& Unwrap(PipelineHandle handle) { return *reinterpret_cast<AbstractPipeline*>(handle); }
	static PipelineHandle Wrap(AbstractPipeline* pipeline) { return reinterpret_cast<PipelineHandle>(pipeline); }
};

} // namespace eg::graphics_api::webgpu
