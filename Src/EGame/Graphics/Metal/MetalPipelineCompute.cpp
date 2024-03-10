#include "../../String.hpp"
#include "../SpirvCrossUtils.hpp"
#include "MetalBuffer.hpp"
#include "MetalCommandContext.hpp"
#include "MetalPipeline.hpp"
#include "MetalShaderModule.hpp"
#include "MetalTranslation.hpp"

#include <Metal/MTLLibrary.hpp>
#include <optional>
#include <spirv.hpp>
#include <spirv_cross.hpp>
#include <spirv_msl.hpp>

namespace eg::graphics_api::mtl
{
static glm::uvec3 GetWorkGroupSize(const ShaderStageInfo& shaderStageInfo)
{
	ShaderModule& shaderModule = *ShaderModule::Unwrap(shaderStageInfo.shaderModule);
	glm::uvec3 workGroupSize;
	for (uint32_t i = 0; i < 3; i++)
	{
		if (shaderModule.workGroupSize[i].isSpecializationConstant)
		{
			uint32_t constantID = shaderModule.workGroupSize[i].valueOrID;
			auto valueOpt = GetSpecConstantValueByID(shaderStageInfo.specConstants, constantID);
			if (!valueOpt.has_value())
				EG_PANIC("workgroup size needs specialization constant " << constantID << ", but not specified");

			int32_t size = std::visit([&](auto v) { return static_cast<int32_t>(v); }, *valueOpt);
			if (size <= 0)
				EG_PANIC("invalid workgroup size: " << size);

			workGroupSize[i] = size;
		}
		else
		{
			workGroupSize[i] = shaderModule.workGroupSize[i].valueOrID;
		}
	}
	return workGroupSize;
}

PipelineHandle CreateComputePipeline(const ComputePipelineCreateInfo& createInfo)
{
	glm::uvec3 workGroupSize = GetWorkGroupSize(createInfo.computeShader);

	auto [function, bindingTable] = Pipeline::PrepareShaderModule(createInfo.computeShader);

	MTL::ComputePipelineDescriptor* descriptor = MTL::ComputePipelineDescriptor::alloc()->init();

	uint32_t workGroupSizeProduct = workGroupSize.x * workGroupSize.y * workGroupSize.z;
	bool groupSizeIsMultipleOfThreadExecutionWidth = (workGroupSizeProduct % 32) == 0;
	descriptor->setThreadGroupSizeIsMultipleOfThreadExecutionWidth(groupSizeIsMultipleOfThreadExecutionWidth);

	NS::Error* error = nullptr;
	MTL::ComputePipelineState* computePipelineState = metalDevice->newComputePipelineState(function, &error);
	if (computePipelineState == nullptr)
	{
		EG_PANIC("Error creating compute pipeline: " << error->localizedDescription()->utf8String());
	}

	if (createInfo.label != nullptr)
		computePipelineState->label()->init(createInfo.label, NS::UTF8StringEncoding);

	if (groupSizeIsMultipleOfThreadExecutionWidth &&
	    (workGroupSizeProduct % computePipelineState->threadExecutionWidth()) != 0)
	{
		EG_PANIC(
			"metal compute pipeline was created with threadGroupSizeIsMultipleOfThreadExecutionWidth=true, but the "
			"workgroup size "
			<< workGroupSizeProduct << " is not a multiple of the received threadExecutionWidth "
			<< computePipelineState->threadExecutionWidth());
	}

	if (computePipelineState->maxTotalThreadsPerThreadgroup() < workGroupSizeProduct)
	{
		EG_PANIC(
			"metal compute pipeline created with maximum threads "
			<< computePipelineState->maxTotalThreadsPerThreadgroup() << " but the workgroups size is "
			<< workGroupSizeProduct);
	}

	Pipeline* pipeline = pipelinePool.New();

	for (uint32_t i = 0; i < MAX_DESCRIPTOR_SETS; i++)
	{
		pipeline->descriptorSetsMaxBindingPlusOne[i] = bindingTable->bindingsMetalIndexTable[i].size();
	}

	pipeline->variant = ComputePipeline{
		.pso = computePipelineState,
		.workGroupSize = MTL::Size::Make(workGroupSize.x, workGroupSize.y, workGroupSize.z),
		.bindingsTable = std::move(bindingTable),
	};

	return reinterpret_cast<PipelineHandle>(pipeline);
}

void ComputePipeline::Bind(MetalCommandContext& mcc) const
{
	mcc.currentComputePipeline = this;
	mcc.GetComputeCmdEncoder().setComputePipelineState(pso);
}

void DispatchCompute(CommandContextHandle ctx, uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ)
{
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(ctx);

	mcc.FlushPushConstantsForCompute();

	mcc.GetComputeCmdEncoder().dispatchThreadgroups(
		MTL::Size::Make(sizeX, sizeY, sizeZ), mcc.currentComputePipeline->workGroupSize);
}

void DispatchComputeIndirect(CommandContextHandle ctx, BufferHandle argsBuffer, uint64_t argsBufferOffset)
{
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(ctx);

	mcc.FlushPushConstantsForCompute();

	mcc.GetComputeCmdEncoder().dispatchThreadgroups(
		UnwrapBuffer(argsBuffer), argsBufferOffset, mcc.currentComputePipeline->workGroupSize);
}

std::optional<uint32_t> GetPipelineSubgroupSize(PipelineHandle pipeline)
{
	if (auto* computePipeline = std::get_if<ComputePipeline>(&UnwrapPipeline(pipeline).variant))
	{
		return computePipeline->pso->threadExecutionWidth();
	}
	else
	{
		return std::nullopt;
	}
}
} // namespace eg::graphics_api::mtl
