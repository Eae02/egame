#include "../../Alloc/ObjectPool.hpp"
#include "../SpirvCrossUtils.hpp"
#include "OpenGLBuffer.hpp"
#include "Pipeline.hpp"

namespace eg::graphics_api::gl
{
#ifdef EG_GLES
PipelineHandle CreateComputePipeline(const ComputePipelineCreateInfo& createInfo)
{
	Log(LogLevel::Error, "gl", "Compute shaders are not supported in GLES");
	return nullptr;
}

void DispatchCompute(CommandContextHandle, uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ)
{
	Log(LogLevel::Error, "gl", "Compute shaders are not supported in GLES");
}

void DispatchComputeIndirect(CommandContextHandle, BufferHandle argsBuffer, uint64_t argsBufferOffset)
{
	Log(LogLevel::Error, "gl", "Compute shaders are not supported in GLES");
}
#else
struct ComputePipeline : public AbstractPipeline
{
	GLuint shaderModule;
	void Free() override;
	void Bind() override;
};

static ObjectPool<ComputePipeline> computePipelinePool;

PipelineHandle CreateComputePipeline(const ComputePipelineCreateInfo& createInfo)
{
	ComputePipeline* pipeline = computePipelinePool.New();

	pipeline->shaderModule = glCreateShader(GL_COMPUTE_SHADER);

	ShaderModule* computeShaderModule = UnwrapShaderModule(createInfo.computeShader.shaderModule);

	spirv_cross::CompilerGLSL spvCompiler(*computeShaderModule->parsedIR);
	SetSpecializationConstants(createInfo.computeShader, spvCompiler);

	std::pair<spirv_cross::CompilerGLSL*, GLuint> shaderStagePair(&spvCompiler, pipeline->shaderModule);
	pipeline->Initialize({ &shaderStagePair, 1 });

	if (createInfo.label != nullptr)
	{
		glObjectLabel(GL_PROGRAM, pipeline->program, -1, createInfo.label);
		glObjectLabel(GL_SHADER, pipeline->shaderModule, -1, createInfo.label);
	}

	return WrapPipeline(pipeline);
}

void ComputePipeline::Free()
{
	glDeleteShader(shaderModule);
	computePipelinePool.Delete(this);
}

void ComputePipeline::Bind() {}

void DispatchCompute(CommandContextHandle, uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ)
{
	AssertAllBindingsSatisfied();
	glDispatchCompute(sizeX, sizeY, sizeZ);
	ClearBarriers();
}

void DispatchComputeIndirect(CommandContextHandle cc, BufferHandle argsBuffer, uint64_t argsBufferOffset)
{
	AssertAllBindingsSatisfied();
	glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, UnwrapBuffer(argsBuffer)->buffer);
	glDispatchComputeIndirect(static_cast<GLintptr>(argsBufferOffset));
	ClearBarriers();
}
#endif
} // namespace eg::graphics_api::gl
