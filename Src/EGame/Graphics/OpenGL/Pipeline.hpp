#pragma once

#include "OpenGL.hpp"
#include "ShaderModule.hpp"
#include "Utils.hpp"

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
	bool isGraphicsPipeline = false;
	GLuint program;
	std::vector<PushConstantMember> pushConstants;
	uint32_t numUniformBuffers = 0;
	uint32_t numTextures = 0;
	std::vector<MappedBinding> bindings;
	PipelineDescriptorSet sets[MAX_DESCRIPTOR_SETS];

	std::optional<uint32_t> ResolveBinding(uint32_t set, uint32_t binding) const;
	std::optional<size_t> FindBindingIndex(uint32_t set, uint32_t binding) const;
	size_t FindBindingsSetStartIndex(uint32_t set) const;

	void Initialize(std::span<std::pair<spirv_cross::CompilerGLSL*, GLuint>> shaderStages);

	virtual void Free() = 0;

	virtual void Bind() = 0;
};

extern const AbstractPipeline* currentPipeline;

void MarkBindingAsSatisfied(size_t resolvedBindingIndex);
void AssertAllBindingsSatisfied();

void CompileShaderStage(GLuint shader, std::string_view glslCode);
void LinkShaderProgram(GLuint program, const std::vector<std::string>& glslCodeStages);

uint32_t ResolveBindingForBind(uint32_t set, uint32_t binding);

inline AbstractPipeline* UnwrapPipeline(PipelineHandle handle)
{
	return reinterpret_cast<AbstractPipeline*>(handle);
}

inline PipelineHandle WrapPipeline(AbstractPipeline* pipeline)
{
	return reinterpret_cast<PipelineHandle>(pipeline);
}
} // namespace eg::graphics_api::gl
