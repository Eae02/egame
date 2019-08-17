#pragma once

#include "OpenGL.hpp"
#include "Utils.hpp"
#include "ShaderModule.hpp"

namespace eg::graphics_api::gl
{
	struct MappedBinding
	{
		uint32_t set;
		uint32_t binding;
		BindingType type;
		uint32_t glBinding;
		
		bool operator<(const MappedBinding& other) const
		{
			if (set != other.set)
				return set < other.set;
			return binding < other.binding;
		}
	};
	
	struct PipelineDescriptorSet
	{
		uint32_t maxBinding;
		uint32_t numUniformBuffers;
		uint32_t numStorageBuffers;
		uint32_t numTextures;
		uint32_t numStorageImages;
		uint32_t firstUniformBuffer;
		uint32_t firstStorageBuffer;
		uint32_t firstTexture;
		uint32_t firstStorageImage;
	};
	
	struct AbstractPipeline
	{
		GLuint program;
		std::vector<PushConstantMember> pushConstants;
		uint32_t numUniformBuffers = 0;
		uint32_t numTextures = 0;
		std::vector<MappedBinding> bindings;
		PipelineDescriptorSet sets[MAX_DESCRIPTOR_SETS];
		
		void Initialize(uint32_t numShaderModules, spirv_cross::CompilerGLSL* spvCompilers, GLuint* shaderModules);
		
		virtual void Free() = 0;
		
		virtual void Bind() = 0;
	};
	
	extern const AbstractPipeline* currentPipeline;
	
	void SetSpecializationConstants(const ShaderStageInfo& stageInfo, spirv_cross::CompilerGLSL& compiler);
	
	uint32_t ResolveBinding(const AbstractPipeline& pipeline, uint32_t set, uint32_t binding);
	
	inline uint32_t ResolveBinding(uint32_t set, uint32_t binding)
	{
		return ResolveBinding(*currentPipeline, set, binding);
	}
	
	inline AbstractPipeline* UnwrapPipeline(PipelineHandle handle)
	{
		return reinterpret_cast<AbstractPipeline*>(handle);
	}
	
	inline PipelineHandle WrapPipeline(AbstractPipeline* pipeline)
	{
		return reinterpret_cast<PipelineHandle>(pipeline);
	}
}
