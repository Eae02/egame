#pragma once

#include "../Abstraction.hpp"
#include "WGPU.hpp"

namespace eg::graphics_api::webgpu
{
struct CachedBindGroupLayout;

struct AbstractPipeline
{
	WGPUPipelineLayout pipelineLayout;

	std::array<const CachedBindGroupLayout*, MAX_DESCRIPTOR_SETS> bindGroupLayouts;

	virtual ~AbstractPipeline();

	virtual void Bind(struct CommandContext& cc) = 0;

	static AbstractPipeline& Unwrap(PipelineHandle handle) { return *reinterpret_cast<AbstractPipeline*>(handle); }
	static PipelineHandle Wrap(AbstractPipeline* pipeline) { return reinterpret_cast<PipelineHandle>(pipeline); }
};

struct GraphicsPipeline : AbstractPipeline
{
	WGPURenderPipeline pipeline;

	std::optional<std::array<WGPURenderPipeline, 3>> dynamicCullModePipelines;

	bool HasDynamicCullMode() const { return dynamicCullModePipelines.has_value(); }

	void Bind(struct CommandContext& cc) override;
};
} // namespace eg::graphics_api::webgpu
