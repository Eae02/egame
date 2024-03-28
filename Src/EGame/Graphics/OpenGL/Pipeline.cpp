// this is needed to stop gcc from complaining about something being maybe uninitialized in std::sort
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#include <vector>
#pragma GCC diagnostic pop
#endif

#include "../../Assert.hpp"
#include "../../Hash.hpp"
#include "../../MainThreadInvoke.hpp"
#include "../../String.hpp"
#include "../SpirvCrossUtils.hpp"
#include "Pipeline.hpp"

#include <algorithm>
#include <spirv_glsl.hpp>
#include <unordered_map>

namespace eg::graphics_api::gl
{
AbstractPipeline* currentPipeline;

std::vector<uint8_t> satisfiedBindings;
size_t remainingBindingsUnsatisfied = 0;

void MarkBindingAsSatisfied(size_t resolvedBindingIndex)
{
	if (resolvedBindingIndex < satisfiedBindings.size() && !satisfiedBindings[resolvedBindingIndex])
	{
		satisfiedBindings[resolvedBindingIndex] = true;
		remainingBindingsUnsatisfied--;
	}
}

void AssertAllBindingsSatisfied()
{
	if (remainingBindingsUnsatisfied == 0)
		return;

	for (size_t binding = 0; binding < satisfiedBindings.size(); binding++)
	{
		if (!satisfiedBindings[binding])
		{
			EG_PANIC(
				"Binding not satisfied: " << currentPipeline->bindings[binding].set << ","
										  << currentPipeline->bindings[binding].binding << "");
		}
	}
}

void DestroyPipeline(PipelineHandle handle)
{
	MainThreadInvoke(
		[pipeline = UnwrapPipeline(handle)]
		{
			glDeleteProgram(pipeline->program.MTGet());
			pipeline->Free();
		});
}

void BindPipeline(CommandContextHandle, PipelineHandle handle)
{
	AbstractPipeline* pipeline = UnwrapPipeline(handle);
	if (pipeline == currentPipeline)
		return;
	currentPipeline = pipeline;

	remainingBindingsUnsatisfied = pipeline->bindings.size();
	satisfiedBindings.resize(pipeline->bindings.size());
	std::fill(satisfiedBindings.begin(), satisfiedBindings.end(), 0);

	glUseProgram(pipeline->program.MTGet());
	pipeline->Bind();
}

std::optional<uint32_t> GetPipelineSubgroupSize(PipelineHandle pipeline)
{
	return std::nullopt;
}

// There is a bug in spirv cross that can cause it to emit GLSL that uses gl_WorkGroupSize before declaring the size of
// the workgroup using "layout(local_size...) in", which is not valid GLSL. This function fixes this by moving the
// workgroup size declaration before the first use of gl_WorkGroupSize.
std::optional<std::string> FixWorkGroupSizeUsedBeforeDeclared(std::string_view glslCode)
{
	size_t workGroupSizeDeclPosition = glslCode.find("layout(local_size");
	size_t firstUseOfWorkGroupSize = glslCode.find("gl_WorkGroupSize");
	if (workGroupSizeDeclPosition == std::string_view::npos || firstUseOfWorkGroupSize == std::string_view::npos ||
	    firstUseOfWorkGroupSize > workGroupSizeDeclPosition)
	{
		return std::nullopt;
	}

	size_t workGroupSizeDeclLineBegin = glslCode.rfind('\n', workGroupSizeDeclPosition);
	size_t workGroupSizeDeclLineEnd = glslCode.find('\n', workGroupSizeDeclPosition);

	size_t firstUseOfWorkGroupSizeLineBegin = glslCode.rfind('\n', firstUseOfWorkGroupSize);

	if (workGroupSizeDeclLineBegin == std::string_view::npos || workGroupSizeDeclLineEnd == std::string_view::npos ||
	    firstUseOfWorkGroupSizeLineBegin == std::string_view::npos)
	{
		return std::nullopt;
	}

	std::ostringstream newCodeStream;
	newCodeStream << glslCode.substr(0, firstUseOfWorkGroupSizeLineBegin);
	newCodeStream << glslCode.substr(
		workGroupSizeDeclLineBegin, workGroupSizeDeclLineEnd - workGroupSizeDeclLineBegin + 1);
	newCodeStream << glslCode.substr(
		firstUseOfWorkGroupSizeLineBegin, workGroupSizeDeclLineBegin - firstUseOfWorkGroupSizeLineBegin);
	newCodeStream << glslCode.substr(workGroupSizeDeclLineEnd);

	return newCodeStream.str();
}

void CompileShaderStage(GLuint shader, std::string_view glslCode)
{
	std::optional<std::string> fixedCode = FixWorkGroupSizeUsedBeforeDeclared(glslCode);
	if (fixedCode.has_value())
		glslCode = *fixedCode;

	const GLchar* glslCodeC = glslCode.data();
	const GLint glslCodeLen = ToInt(glslCode.size());
	glShaderSource(shader, 1, &glslCodeC, &glslCodeLen);

	glCompileShader(shader);

	// Checks the shader's compile status.
	GLint compileStatus = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
	if (!compileStatus)
	{
		GLint infoLogLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);

		std::vector<char> infoLog(static_cast<size_t>(infoLogLen) + 1);
		glGetShaderInfoLog(shader, infoLogLen, nullptr, infoLog.data());
		infoLog.back() = '\0';

		std::cout << "Shader failed to compile!\n\n --- GLSL Code --- \n"
				  << glslCode << "\n\n --- Info Log --- \n"
				  << infoLog.data() << std::endl;

		std::abort();
	}
}

void LinkShaderProgram(GLuint program)
{
	glLinkProgram(program);

	// Checks that the program linked successfully
	int linkStatus = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
	if (!linkStatus)
	{
		int infoLogLen = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLen);

		std::vector<char> infoLog(static_cast<size_t>(infoLogLen) + 1);
		glGetProgramInfoLog(program, infoLogLen, nullptr, infoLog.data());
		infoLog.back() = '\0';

		std::cout << "Shader program failed to link: \n\n --- Info Log --- \n" << infoLog.data();

		std::abort();
	}
}

std::optional<size_t> AbstractPipeline::FindBindingIndex(uint32_t set, uint32_t binding) const
{
	auto it = std::lower_bound(bindings.begin(), bindings.end(), std::make_pair(set, binding));
	if (it != bindings.end() && it->set == set && it->binding == binding)
		return it - bindings.begin();
	return std::nullopt;
}

std::span<const uint32_t> AbstractPipeline::ResolveBindingMulti(uint32_t set, uint32_t binding) const
{
	if (std::optional<size_t> bindingIndex = FindBindingIndex(set, binding))
		return bindings[*bindingIndex].GetGLBindings();
	return {};
}

std::optional<uint32_t> AbstractPipeline::ResolveBindingSingle(uint32_t set, uint32_t binding) const
{
	std::span<const uint32_t> resolved = ResolveBindingMulti(set, binding);
	EG_ASSERT(resolved.size() <= 1);
	if (resolved.empty())
		return std::nullopt;
	return resolved[0];
}

size_t AbstractPipeline::FindBindingsSetStartIndex(uint32_t set) const
{
	return ToUnsigned(std::lower_bound(bindings.begin(), bindings.end(), std::make_pair(set, 0)) - bindings.begin());
}

std::span<const uint32_t> ResolveBindingMulti(uint32_t set, uint32_t binding)
{
	std::optional<size_t> bindingIndex = currentPipeline->FindBindingIndex(set, binding);
	if (!bindingIndex.has_value())
		EG_PANIC("Attempted to bind to invalid binding " << set << "," << binding);
	MarkBindingAsSatisfied(*bindingIndex);
	return currentPipeline->bindings[*bindingIndex].GetGLBindings();
}

uint32_t ResolveBindingSingle(uint32_t set, uint32_t binding)
{
	std::span<const uint32_t> bindings = ResolveBindingMulti(set, binding);
	EG_ASSERT(bindings.size() == 1);
	return bindings[0];
}

struct CombinedImageSamplerKey
{
	uint32_t textureSetIndex;
	uint32_t textureBindingIndex;
	uint32_t samplerSetIndex;
	uint32_t samplerBindingIndex;

	bool operator==(const CombinedImageSamplerKey&) const = default;

	size_t Hash() const
	{
		size_t h = 0;
		HashAppend(h, textureSetIndex);
		HashAppend(h, textureBindingIndex);
		HashAppend(h, samplerSetIndex);
		HashAppend(h, samplerBindingIndex);
		return h;
	}
};

static inline GLenum TranslateShaderStage(ShaderStage stage)
{
	switch (stage)
	{
	case ShaderStage::Vertex: return GL_VERTEX_SHADER;
	case ShaderStage::Fragment: return GL_FRAGMENT_SHADER;
	case ShaderStage::Geometry: return GL_GEOMETRY_SHADER;
	case ShaderStage::TessControl: return GL_TESS_CONTROL_SHADER;
	case ShaderStage::TessEvaluation: return GL_TESS_EVALUATION_SHADER;
	case ShaderStage::Compute: return GL_COMPUTE_SHADER;
	}
	EG_UNREACHABLE
}

void AbstractPipeline::Initialize(
	std::span<std::pair<spirv_cross::CompilerGLSL*, ShaderStage>> stageCompilers, const char* label)
{
	// Detects resources used in shaders
	DescriptorSetBindings dsBindings;
	for (auto [compiler, stage] : stageCompilers)
	{
		const spirv_cross::ShaderResources& resources = compiler->get_shader_resources();
		dsBindings.AppendFromReflectionInfo({}, *compiler, resources);
	}

	for (uint32_t set = 0; set < MAX_DESCRIPTOR_SETS; set++)
	{
		for (DescriptorSetBinding& binding : dsBindings.sets[set])
		{
			bindings.push_back(MappedBinding{
				.set = set,
				.binding = binding.binding,
				.type = binding.GetBindingType(),
				.glBindings = std::monostate(),
			});
		}
	}

	std::sort(bindings.begin(), bindings.end());

	std::unordered_map<CombinedImageSamplerKey, GLuint, MemberFunctionHash<CombinedImageSamplerKey>>
		combinedImageSamplersBindingMap;

	uint32_t nextTextureBinding = 0;
	for (auto [compiler, stage] : stageCompilers)
	{
		spirv_cross::VariableID dummySamplerID = compiler->build_dummy_sampler_for_combined_images();

		compiler->build_combined_image_samplers();

		for (const auto& comb : compiler->get_combined_image_samplers())
		{
			std::string name = Concat({
				"SPIRV_Cross_Combined_",
				compiler->get_name(comb.image_id),
				"_",
				compiler->get_name(comb.sampler_id),
			});
			compiler->set_name(comb.combined_id, name);

			CombinedImageSamplerKey key;
			key.textureSetIndex = compiler->get_decoration(comb.image_id, spv::DecorationDescriptorSet);
			key.textureBindingIndex = compiler->get_decoration(comb.image_id, spv::DecorationBinding);
			if (comb.sampler_id == dummySamplerID)
			{
				key.samplerSetIndex = UINT32_MAX;
				key.samplerBindingIndex = UINT32_MAX;
			}
			else
			{
				key.samplerSetIndex = compiler->get_decoration(comb.sampler_id, spv::DecorationDescriptorSet);
				key.samplerBindingIndex = compiler->get_decoration(comb.sampler_id, spv::DecorationBinding);
			}

			uint32_t binding;
			auto bindingIt = combinedImageSamplersBindingMap.find(key);
			if (bindingIt != combinedImageSamplersBindingMap.end())
			{
				binding = bindingIt->second;
			}
			else
			{
				binding = nextTextureBinding++;
				combinedImageSamplersBindingMap.emplace(key, binding);
			}

			auto textureBindingIt = std::lower_bound(
				bindings.begin(), bindings.end(), std::make_pair(key.textureSetIndex, key.textureBindingIndex));
			if (textureBindingIt != bindings.end() && textureBindingIt->set == key.textureSetIndex &&
			    textureBindingIt->binding == key.textureBindingIndex)
			{
				textureBindingIt->PushGLBinding(binding);
			}

			if (comb.sampler_id != dummySamplerID)
			{
				auto samplerBindingIt = std::lower_bound(
					bindings.begin(), bindings.end(), std::make_pair(key.samplerSetIndex, key.samplerBindingIndex));
				if (samplerBindingIt != bindings.end() && samplerBindingIt->set == key.samplerSetIndex &&
				    samplerBindingIt->binding == key.samplerBindingIndex)
				{
					samplerBindingIt->PushGLBinding(binding);
				}
			}

			compiler->set_decoration(comb.combined_id, spv::DecorationBinding, binding);
		}
	}

	// Assigns gl bindings to resources
	uint32_t nextStorageImageBinding = 0;
	uint32_t nextUniformBufferBinding = 0;
	uint32_t nextStorageBufferBinding = 0;
	bool usesGL4Resources = false;
	for (uint32_t i = 0; i < bindings.size(); i++)
	{
		uint32_t set = bindings[i].set;
		if (i == 0 || bindings[i - 1].set != set)
		{
			sets[set] = {};
			sets[set].firstUniformBuffer = nextUniformBufferBinding;
			sets[set].firstStorageBuffer = nextStorageBufferBinding;
			sets[set].firstTexture = nextTextureBinding;
			sets[set].firstStorageImage = nextStorageImageBinding;
		}
		sets[set].maxBinding = std::max(sets[set].maxBinding, bindings[i].binding);
		switch (bindings[i].type)
		{
		case BindingType::UniformBuffer:
			sets[set].numUniformBuffers++;
			bindings[i].PushGLBinding(nextUniformBufferBinding++);
			break;
		case BindingType::StorageBuffer:
			sets[set].numStorageBuffers++;
			bindings[i].PushGLBinding(nextStorageBufferBinding++);
			usesGL4Resources = true;
			break;
		case BindingType::StorageImage:
			sets[set].numStorageImages++;
			bindings[i].PushGLBinding(nextStorageImageBinding++);
			usesGL4Resources = true;
			break;
		case BindingType::Texture: sets[set].numTextures++; break;
		case BindingType::Sampler: sets[set].numSamplers++; break;
		}
	}

	std::string labelCopy;
	if (label != nullptr)
		labelCopy = label;
	program = MainThreadInvokableUnsyncronized<GLuint>::Init(
		[labelCopy = std::move(labelCopy)]
		{
			GLuint p = glCreateProgram();
			if (!labelCopy.empty())
				glObjectLabel(GL_PROGRAM, p, -1, labelCopy.c_str());
			return p;
		});

	std::vector<std::string> glslCodeStages;

	// Updates the bindings used by resources and uploads code to shader modules
	for (auto [compiler, stage] : stageCompilers)
	{
		const spirv_cross::ShaderResources& shResources = compiler->get_shader_resources();

		// Updates bindings for resources with a single binding
		const spirv_cross::SmallVector<spirv_cross::Resource>* singleBindingResourceLists[] = {
			&shResources.uniform_buffers,
			&shResources.storage_buffers,
			&shResources.storage_images,
		};
		for (const auto* resourceList : singleBindingResourceLists)
		{
			for (const spirv_cross::Resource& res : *resourceList)
			{
				const uint32_t set = compiler->get_decoration(res.id, spv::DecorationDescriptorSet);
				const uint32_t binding = compiler->get_decoration(res.id, spv::DecorationBinding);
				auto it = std::lower_bound(bindings.begin(), bindings.end(), std::make_pair(set, binding));

				std::span<const uint32_t> glBindings = it->GetGLBindings();
				EG_ASSERT(glBindings.size() <= 1);

				compiler->set_decoration(res.id, spv::DecorationDescriptorSet, 0);
				compiler->set_decoration(res.id, spv::DecorationBinding, glBindings.empty() ? 0 : glBindings[0]);
			}
		}

		spirv_cross::CompilerGLSL::Options options = compiler->get_common_options();

#ifdef __APPLE__
		options.version = 330;
		options.enable_420pack_extension = false;
#else
		if (useGLESPath && !usesGL4Resources)
		{
			options.version = 300;
			options.es = true;
			options.fragment.default_float_precision = spirv_cross::CompilerGLSL::Options::Highp;
		}
		else
		{
			options.version = 430;
		}
#endif

		compiler->set_common_options(options);
		std::string glslCode = compiler->compile();

		program.OnMainThread(
			[glslCode = std::move(glslCode), stage](GLuint glProgram)
			{
				GLuint shader = glCreateShader(TranslateShaderStage(stage));
				CompileShaderStage(shader, glslCode);
				glAttachShader(glProgram, shader);
			});
	}

#ifdef __EMSCRIPTEN__
	// Webgl doesn't seem to support having only a vertex shader, so add a dummy fragment shader
	if (stageCompilers.size() == 1)
	{
		program.OnMainThread(
			[](GLuint glProgram)
			{
				static std::optional<GLuint> dummyFragmentShader;
				if (!dummyFragmentShader.has_value())
				{
					dummyFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
					CompileShaderStage(*dummyFragmentShader, "#version 300 es\nvoid main() { }\n");
				}
				glAttachShader(glProgram, *dummyFragmentShader);
			});
	}
#endif

	program.OnMainThread([](GLuint glProgram) { LinkShaderProgram(glProgram); });

	// Bindings for textures and uniform blocks cannot be set in the shader code for GLES,
	//  so they need to be set manually.
	if (useGLESPath && !usesGL4Resources)
	{
		program.OnMainThread([](GLuint glProgram) { glUseProgram(glProgram); });

		for (auto [compiler, _] : stageCompilers)
		{
			const spirv_cross::ShaderResources& shResources = compiler->get_shader_resources();
			for (const spirv_cross::Resource& res : shResources.sampled_images)
			{
				const uint32_t binding = compiler->get_decoration(res.id, spv::DecorationBinding);
				program.OnMainThread(
					[name = res.name, binding](GLuint glProgram)
					{
						int location = glGetUniformLocation(glProgram, name.c_str());
						if (location == -1)
						{
							eg::Log(eg::LogLevel::Warning, "gl", "Texture uniform not found: '{0}'", name);
						}
						else
						{
							glUniform1i(location, static_cast<GLint>(binding));
						}
					});
			}
			for (const spirv_cross::Resource& res : shResources.uniform_buffers)
			{
				const uint32_t binding = compiler->get_decoration(res.id, spv::DecorationBinding);
				program.OnMainThread(
					[name = res.name, binding](GLuint glProgram)
					{
						GLuint blockIndex = glGetUniformBlockIndex(glProgram, name.c_str());
						if (blockIndex == GL_INVALID_INDEX)
						{
							eg::Log(eg::LogLevel::Warning, "gl", "Uniform block not found: '{0}'", name);
						}
						else
						{
							glUniformBlockBinding(glProgram, blockIndex, binding);
						}
					});
			}
		}

		program.OnMainThread([](GLuint glProgram)
		                     { glUseProgram(currentPipeline ? currentPipeline->program.MTGet() : 0); });
	}
}

void PushConstants(CommandContextHandle, uint32_t offset, uint32_t range, const void* data)
{
	EG_PANIC("Unsupported: PushConstants")
}

void MappedBinding::PushGLBinding(uint32_t b)
{
	if (std::holds_alternative<std::monostate>(glBindings))
		glBindings = b;
	else if (std::holds_alternative<uint32_t>(glBindings))
		glBindings = std::vector<uint32_t>{ std::get<uint32_t>(glBindings), b };
	else
		std::get<std::vector<uint32_t>>(glBindings).push_back(b);
}
std::span<const uint32_t> MappedBinding::GetGLBindings() const
{
	if (std::holds_alternative<std::monostate>(glBindings))
		return {};
	else if (std::holds_alternative<uint32_t>(glBindings))
		return { &std::get<uint32_t>(glBindings), 1 };
	else
		return std::get<std::vector<uint32_t>>(glBindings);
}
} // namespace eg::graphics_api::gl
