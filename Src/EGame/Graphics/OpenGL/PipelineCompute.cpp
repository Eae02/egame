#include "Pipeline.hpp"
#include "../../Alloc/ObjectPool.hpp"

namespace eg::graphics_api::gl
{
	struct ComputePipeline : public AbstractPipeline
	{
		GLuint shaderModule;
		
		void Free() override;
		
		void Bind() override;
	};
	
	static ObjectPool<ComputePipeline> computePipelinePool;
	
	PipelineHandle CreateComputePipeline(const ComputePipelineCreateInfo& createInfo)
	{
#ifdef __EMSCRIPTEN__
		Log(LogLevel::Error, "gl", "Compute shaders are not supported in WebGL");
		return nullptr;
#else
		ComputePipeline* pipeline = computePipelinePool.New();
		
		pipeline->shaderModule = glCreateShader(GL_COMPUTE_SHADER);
		
		ShaderModule* computeShaderModule = UnwrapShaderModule(createInfo.computeShader.shaderModule);
		
		spirv_cross::CompilerGLSL spvCompiler(computeShaderModule->parsedIR);
		SetSpecializationConstants(createInfo.computeShader, spvCompiler);
		pipeline->Initialize(1, &spvCompiler, &pipeline->shaderModule);
		
		if (createInfo.label != nullptr)
		{
			glObjectLabel(GL_PROGRAM, pipeline->program, -1, createInfo.label);
			glObjectLabel(GL_SHADER, pipeline->shaderModule, -1, createInfo.label);
		}
		
		return WrapPipeline(pipeline);
#endif
	}
	
	void ComputePipeline::Free()
	{
		glDeleteShader(shaderModule);
		computePipelinePool.Delete(this);
	}
	
	void ComputePipeline::Bind() { }
	
	void DispatchCompute(CommandContextHandle, uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ)
	{
#ifdef __EMSCRIPTEN__
		Log(LogLevel::Error, "gl", "Compute shaders are not supported in WebGL");
#else
		glDispatchCompute(sizeX, sizeY, sizeZ);
		ClearBarriers();
#endif
	}
}
