#pragma once

#include "../../Alloc/ObjectPool.hpp"
#include "../Graphics.hpp"
#include "MetalMain.hpp"

#include <variant>

namespace eg::graphics_api::mtl
{
static constexpr uint32_t PUSH_CONSTANTS_BUFFER_INDEX = 30;
	
inline uint32_t GetVertexBindingBufferIndex(uint32_t binding)
{
	return 29 - binding;
}

struct StageBindingsTable
{
	uint32_t pushConstantBytes = 0;

	std::array<std::vector<int>, MAX_DESCRIPTOR_SETS> bindingsMetalIndexTable;

	std::optional<uint32_t> GetResourceMetalIndex(uint32_t set, uint32_t binding) const;
};

struct BoundGraphicsPipelineState
{
	MTL::PrimitiveType primitiveType;

	bool enableScissorTest;

	std::shared_ptr<StageBindingsTable> bindingsTableVS;
	std::shared_ptr<StageBindingsTable> bindingsTableFS;

	std::optional<uint32_t> GetResourceMetalIndexVS(uint32_t set, uint32_t binding) const
	{
		return bindingsTableVS->GetResourceMetalIndex(set, binding);
	}

	std::optional<uint32_t> GetResourceMetalIndexFS(uint32_t set, uint32_t binding) const
	{
		return bindingsTableFS ? bindingsTableFS->GetResourceMetalIndex(set, binding) : std::nullopt;
	}
};

struct GraphicsPipeline
{
	MTL::RenderPipelineState* pso;
	std::optional<MTL::CullMode> cullMode; // nullopt means that cull mode is set dynamically
	bool enableWireframeRasterization = false;
	bool enableDepthClamp = false;
	bool frontFaceCCW = false;
	MTL::DepthStencilState* depthStencilState;
	std::array<float, 4> blendColor;

	BoundGraphicsPipelineState boundState;

	void Bind(struct MetalCommandContext& mcc) const;
};

struct ComputePipeline
{
	MTL::ComputePipelineState* pso;
	MTL::Size workGroupSize;

	std::shared_ptr<StageBindingsTable> bindingsTable;

	void Bind(struct MetalCommandContext& mcc) const;
};

struct Pipeline
{
	std::array<uint32_t, MAX_DESCRIPTOR_SETS> descriptorSetsMaxBindingPlusOne;
	std::variant<GraphicsPipeline, ComputePipeline> variant;

	static std::pair<MTL::Function*, std::shared_ptr<StageBindingsTable>> PrepareShaderModule(
		const ShaderStageInfo& stageInfo);
};

inline Pipeline& UnwrapPipeline(PipelineHandle handle)
{
	return *reinterpret_cast<Pipeline*>(handle);
}

extern ConcurrentObjectPool<Pipeline> pipelinePool;
} // namespace eg::graphics_api::mtl
