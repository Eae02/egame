#include "../Abstraction.hpp"
#include "../SpirvCrossUtils.hpp"
#include "EGame/String.hpp"
#include "WGPUBuffer.hpp"
#include "WGPUCommandContext.hpp"
#include "WGPUDescriptorSet.hpp"
#include "WGPUPipeline.hpp"
#include "WGPUShaderModule.hpp"
#include "WGPUTranslation.hpp"

namespace eg::graphics_api::webgpu
{
PipelineHandle CreateComputePipeline(const ComputePipelineCreateInfo& createInfo)
{
	if (createInfo.dynamicDescriptorSetIndex.has_value())
	{
		std::string labelWithParen;
		if (createInfo.label)
			labelWithParen = Concat({ "(", createInfo.label, ") " });
		eg::Log(
			eg::LogLevel::Warning, "webgpu",
			"Pipeline{0} uses dynamic descriptor set, which is not supported in WebGPU", labelWithParen);
	}

	ShaderModule* shaderModule = reinterpret_cast<ShaderModule*>(createInfo.computeShader.shaderModule);
	EG_ASSERT(shaderModule != nullptr);

	auto computeShaderModule = shaderModule->GetSpecializedShaderModule(createInfo.computeShader.specConstants);

	DescriptorSetBindings bindings = shaderModule->bindings;
	bindings.SortByBinding();

	AbstractPipeline* pipeline = new AbstractPipeline(bindings, createInfo.label);

	WGPUComputePipelineDescriptor pipelineDescriptor = {
		.label = createInfo.label,
		.layout = pipeline->pipelineLayout,
		.compute =
			WGPUProgrammableStageDescriptor{
				.module = computeShaderModule.get(),
				.entryPoint = "main",
			},
	};
	WGPUComputePipeline computePipeline = wgpuDeviceCreateComputePipeline(wgpuctx.device, &pipelineDescriptor);

	pipeline->pipeline = ComputePipeline{ .pipeline = computePipeline };
	return AbstractPipeline::Wrap(pipeline);
}

void ComputePipeline::Destroy() {}

void ComputePipeline::Bind(CommandContext& cc)
{
	if (cc.computePassEncoder == nullptr)
	{
		WGPUComputePassDescriptor passDescriptor = {};
		cc.computePassEncoder = wgpuCommandEncoderBeginComputePass(cc.encoder, &passDescriptor);
	}

	wgpuComputePassEncoderSetPipeline(cc.computePassEncoder, pipeline);
}

void DispatchCompute(CommandContextHandle cc, uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ)
{
	CommandContext& wcc = CommandContext::Unwrap(cc);
	EG_ASSERT(wcc.computePassEncoder != nullptr);
	wgpuComputePassEncoderDispatchWorkgroups(wcc.computePassEncoder, sizeX, sizeY, sizeZ);
}

void DispatchComputeIndirect(CommandContextHandle cc, BufferHandle argsBuffer, uint64_t argsBufferOffset)
{
	CommandContext& wcc = CommandContext::Unwrap(cc);
	EG_ASSERT(wcc.computePassEncoder != nullptr);
	wgpuComputePassEncoderDispatchWorkgroupsIndirect(
		wcc.computePassEncoder, Buffer::Unwrap(argsBuffer).buffer, argsBufferOffset);
}
} // namespace eg::graphics_api::webgpu
