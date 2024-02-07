#pragma once

#include "../../Alloc/ObjectPool.hpp"
#include "../SpirvCrossUtils.hpp"
#include "EGame/Graphics/Graphics.hpp"
#include "MetalMain.hpp"

#include <variant>

namespace eg::graphics_api::mtl
{
struct StageBindingsTable
{
	int pushConstantsBinding = -1;
	
	std::array<std::vector<int>, MAX_DESCRIPTOR_SETS> bindingsMetalIndexTable;

	std::optional<uint32_t> GetResourceMetalIndex(uint32_t set, uint32_t binding) const;
};

struct BoundGraphicsPipelineState
{
	MTL::PrimitiveType primitiveType;
	uint32_t vertexShaderPushConstantBytes;
	uint32_t fragmentShaderPushConstantBytes;

	bool enableScissorTest;

	StageBindingsTable bindingsTableVertexShader;
	StageBindingsTable bindingsTableFragmentShader;
};

struct GraphicsPipeline
{
	MTL::RenderPipelineState* pso;
	std::optional<MTL::CullMode> cullMode; // nullopt means that cull mode is set dynamically
	bool enableWireframeRasterization = false;
	bool enableDepthClamp = false;
	bool frontFaceCCW = false;
	MTL::DepthStencilState* depthStencilState;

	BoundGraphicsPipelineState boundState;

	void Bind(struct MetalCommandContext& mcc) const;
};

struct ComputePipeline
{
	MTL::ComputePipelineState* pso;
};

struct Pipeline
{
	std::array<uint32_t, MAX_DESCRIPTOR_SETS> descriptorSetsMaxBindingPlusOne;
	std::variant<GraphicsPipeline, ComputePipeline> variant;
};

inline Pipeline& UnwrapPipeline(PipelineHandle handle)
{
	return *reinterpret_cast<Pipeline*>(handle);
}

extern ConcurrentObjectPool<Pipeline> pipelinePool;
} // namespace eg::graphics_api::mtl
