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
		ComputePipeline* pipeline = computePipelinePool.New();
		
		pipeline->shaderModule = glCreateShader(GL_COMPUTE_SHADER);
		
		ShaderModule* computeShaderModule = UnwrapShaderModule(createInfo.computeShader);
		
		spirv_cross::CompilerGLSL* spvCompilerPtr = &computeShaderModule->spvCompiler;
		pipeline->Initialize(1, &spvCompilerPtr, &pipeline->shaderModule);
		
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
		computePipelinePool.Free(this);
	}
	
	void ComputePipeline::Bind() { }
	
	void DispatchCompute(CommandContextHandle, uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ)
	{
		glDispatchCompute(sizeX, sizeY, sizeZ);
		ClearBarriers();
	}
}
