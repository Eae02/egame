#pragma once

#include "EGame/MainThreadInvoke.hpp"
#include "OpenGL.hpp"
#include "ShaderModule.hpp"
#include "Utils.hpp"

namespace eg::graphics_api::gl
{
struct MappedBinding
{
	uint32_t set{};
	uint32_t binding{};
	BindingType type{};

	// For textures and samplers there can be multiple bindings
	std::variant<std::monostate, uint32_t, std::vector<uint32_t>> glBindings = std::monostate();

	void PushGLBinding(uint32_t b);

	std::span<const uint32_t> GetGLBindings() const;

	std::optional<uint32_t> GetSingleGLBinding() const
	{
		std::span<const uint32_t> bindingsSpan = GetGLBindings();
		if (bindingsSpan.size() == 1)
			return bindingsSpan[0];
		return std::nullopt;
	}

	bool operator<(const MappedBinding& other) const
	{
		return std::make_pair(set, binding) < std::make_pair(other.set, other.binding);
	}

	bool operator<(const std::pair<uint32_t, uint32_t>& other) const { return std::make_pair(set, binding) < other; }
};

struct PipelineDescriptorSet
{
	uint32_t maxBinding;
	uint32_t numUniformBuffers;
	uint32_t numStorageBuffers;
	uint32_t numTextures;
	uint32_t numSamplers;
	uint32_t numStorageImages;
	uint32_t firstUniformBuffer;
	uint32_t firstStorageBuffer;
	uint32_t firstTexture;
	uint32_t firstStorageImage;
};

struct AbstractPipeline
{
	bool isGraphicsPipeline = false;
	MainThreadInvokableUnsyncronized<GLuint> program;
	uint32_t numUniformBuffers = 0;
	uint32_t numTextures = 0;
	std::vector<MappedBinding> bindings;
	PipelineDescriptorSet sets[MAX_DESCRIPTOR_SETS];

	std::optional<size_t> FindBindingIndex(uint32_t set, uint32_t binding) const;
	size_t FindBindingsSetStartIndex(uint32_t set) const;

	std::span<const uint32_t> ResolveBindingMulti(uint32_t set, uint32_t binding) const;
	std::optional<uint32_t> ResolveBindingSingle(uint32_t set, uint32_t binding) const;

	void Initialize(std::span<std::pair<spirv_cross::CompilerGLSL*, ShaderStage>> stageCompilers, const char* label);

	virtual void Free() = 0;

	virtual void Bind() = 0;
};

extern AbstractPipeline* currentPipeline;

void MarkBindingAsSatisfied(size_t resolvedBindingIndex);
void AssertAllBindingsSatisfied();

void CompileShaderStage(GLuint shader, std::string_view glslCode);
void LinkShaderProgram(GLuint program);

std::span<const uint32_t> ResolveBindingMulti(uint32_t set, uint32_t binding);
uint32_t ResolveBindingSingle(uint32_t set, uint32_t binding);

inline AbstractPipeline* UnwrapPipeline(PipelineHandle handle)
{
	return reinterpret_cast<AbstractPipeline*>(handle);
}

inline PipelineHandle WrapPipeline(AbstractPipeline* pipeline)
{
	return reinterpret_cast<PipelineHandle>(pipeline);
}
} // namespace eg::graphics_api::gl
