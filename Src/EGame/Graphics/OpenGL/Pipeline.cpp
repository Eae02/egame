#include "Pipeline.hpp"
#include "../../Assert.hpp"
#include "../../MainThreadInvoke.hpp"
#include "../../String.hpp"
#include "../SpirvCrossUtils.hpp"

#include <algorithm>
#include <spirv_glsl.hpp>

namespace eg::graphics_api::gl
{
const AbstractPipeline* currentPipeline;

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
			glDeleteProgram(pipeline->program);
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

	glUseProgram(pipeline->program);
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

void LinkShaderProgram(GLuint program, const std::vector<std::string>& glslCodeStages)
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

		for (const std::string& code : glslCodeStages)
		{
			std::cout << "\n\n --- GLSL ---\n" << code;
		}

		std::abort();
	}
}

std::optional<size_t> AbstractPipeline::FindBindingIndex(uint32_t set, uint32_t binding) const
{
	auto it = std::lower_bound(bindings.begin(), bindings.end(), MappedBinding{ set, binding });
	if (it != bindings.end() && it->set == set && it->binding == binding)
		return it - bindings.begin();
	return {};
}

std::optional<uint32_t> AbstractPipeline::ResolveBinding(uint32_t set, uint32_t binding) const
{
	if (std::optional<size_t> bindingIndex = FindBindingIndex(set, binding))
		return bindings[*bindingIndex].glBinding;
	return {};
}

size_t AbstractPipeline::FindBindingsSetStartIndex(uint32_t set) const
{
	return std::lower_bound(bindings.begin(), bindings.end(), MappedBinding{ set, 0 }) - bindings.begin();
}

uint32_t ResolveBindingForBind(uint32_t set, uint32_t binding)
{
	std::optional<size_t> bindingIndex = currentPipeline->FindBindingIndex(set, binding);
	if (!bindingIndex.has_value())
		EG_PANIC("Attempted to bind to invalid binding " << set << "," << binding);
	MarkBindingAsSatisfied(*bindingIndex);
	return currentPipeline->bindings[*bindingIndex].glBinding;
}

static const std::pair<spirv_cross::SmallVector<spirv_cross::Resource> spirv_cross::ShaderResources::*, BindingType>
	bindingTypes[] = {
		{ &spirv_cross::ShaderResources::uniform_buffers, BindingType::UniformBuffer },
		{ &spirv_cross::ShaderResources::storage_buffers, BindingType::StorageBuffer },
		{ &spirv_cross::ShaderResources::sampled_images, BindingType::Texture },
		{ &spirv_cross::ShaderResources::storage_images, BindingType::StorageImage },
	};

void AbstractPipeline::Initialize(std::span<std::pair<spirv_cross::CompilerGLSL*, GLuint>> shaderStages)
{
	// Detects resources used in shaders
	DescriptorSetBindings dsBindings;
	for (auto [compiler, _] : shaderStages)
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
				.type = binding.type,
				.glBinding = 0,
			});
		}
	}

	std::sort(bindings.begin(), bindings.end());

	// Assigns gl bindings to resources
	uint32_t nextTextureBinding = 0;
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
		case BindingType::UniformBufferDynamicOffset:
			sets[set].numUniformBuffers++;
			bindings[i].glBinding = nextUniformBufferBinding++;
			break;
		case BindingType::StorageBuffer:
		case BindingType::StorageBufferDynamicOffset:
			sets[set].numStorageBuffers++;
			bindings[i].glBinding = nextStorageBufferBinding++;
			usesGL4Resources = true;
			break;
		case BindingType::Texture:
			sets[set].numTextures++;
			bindings[i].glBinding = nextTextureBinding++;
			break;
		case BindingType::StorageImage:
			sets[set].numStorageImages++;
			bindings[i].glBinding = nextStorageImageBinding++;
			usesGL4Resources = true;
			break;
		}
	}

	program = glCreateProgram();

	std::vector<std::string> glslCodeStages;

	// Updates the bindings used by resources and uploads code to shader modules
	for (auto [compiler, shader] : shaderStages)
	{
		const spirv_cross::ShaderResources& shResources = compiler->get_shader_resources();
		for (auto [resourceFieldPtr, bindingType] : bindingTypes)
		{
			for (const spirv_cross::Resource& res : shResources.*resourceFieldPtr)
			{
				const uint32_t set = compiler->get_decoration(res.id, spv::DecorationDescriptorSet);
				const uint32_t binding = compiler->get_decoration(res.id, spv::DecorationBinding);
				auto it = std::lower_bound(bindings.begin(), bindings.end(), MappedBinding{ set, binding });
				compiler->set_decoration(res.id, spv::DecorationDescriptorSet, 0);
				compiler->set_decoration(res.id, spv::DecorationBinding, it->glBinding);
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

		CompileShaderStage(shader, glslCode);

		glAttachShader(program, shader);
		glslCodeStages.push_back(std::move(glslCode));
	}

#ifdef __EMSCRIPTEN__
	// Webgl doesn't seem to support having only a vertex shader, so add a dummy fragment shader
	if (shaderStages.size() == 1)
	{
		static std::optional<GLuint> dummyFragmentShader;
		if (!dummyFragmentShader.has_value())
		{
			dummyFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
			CompileShaderStage(*dummyFragmentShader, "#version 300 es\nvoid main() { }\n");
		}
		glAttachShader(program, *dummyFragmentShader);
	}
#endif

	LinkShaderProgram(program, glslCodeStages);

	// Bindings for textures and uniform blocks cannot be set in the shader code for GLES,
	//  so they need to be set manually.
	if (useGLESPath && !usesGL4Resources)
	{
		glUseProgram(program);
		for (auto [compiler, _] : shaderStages)
		{
			const spirv_cross::ShaderResources& shResources = compiler->get_shader_resources();
			for (const spirv_cross::Resource& res : shResources.sampled_images)
			{
				const uint32_t binding = compiler->get_decoration(res.id, spv::DecorationBinding);
				int location = glGetUniformLocation(program, res.name.c_str());
				if (location == -1)
				{
					eg::Log(eg::LogLevel::Warning, "gl", "Texture uniform not found: '{0}'", res.name);
				}
				else
				{
					glUniform1i(location, static_cast<GLint>(binding));
				}
			}
			for (const spirv_cross::Resource& res : shResources.uniform_buffers)
			{
				const uint32_t binding = compiler->get_decoration(res.id, spv::DecorationBinding);
				GLuint blockIndex = glGetUniformBlockIndex(program, res.name.c_str());
				if (blockIndex == GL_INVALID_INDEX)
				{
					eg::Log(eg::LogLevel::Warning, "gl", "Uniform block not found: '{0}'", res.name);
				}
				else
				{
					glUniformBlockBinding(program, blockIndex, binding);
				}
			}
		}
		glUseProgram(currentPipeline ? currentPipeline->program : 0);
	}

	// Processes push constant blocks
	for (auto [compiler, _] : shaderStages)
	{
		const spirv_cross::ShaderResources& resources = compiler->get_shader_resources();

		for (const spirv_cross::Resource& pcBlock : resources.push_constant_buffers)
		{
			const SPIRType& type = compiler->get_type(pcBlock.base_type_id);
			const uint32_t numMembers = UnsignedNarrow<uint32_t>(type.member_types.size());

			std::string blockName = compiler->get_name(pcBlock.id);
			if (blockName.empty())
			{
				blockName = compiler->get_fallback_name(pcBlock.id);
			}

			auto activeRanges = compiler->get_active_buffer_ranges(pcBlock.id);

			for (uint32_t i = 0; i < numMembers; i++)
			{
				const SPIRType& memberType = compiler->get_type(type.member_types[i]);

				// Only process supported base types
				static const SPIRType::BaseType supportedBaseTypes[] = { SPIRType::Float, SPIRType::Int, SPIRType::UInt,
					                                                     SPIRType::Boolean };
				if (!Contains(supportedBaseTypes, memberType.basetype))
					continue;

				const uint32_t offset = compiler->type_struct_member_offset(type, i);

				bool active = false;
				for (const spirv_cross::BufferRange& range : activeRanges)
				{
					if (offset >= range.offset && offset < range.offset + range.range)
					{
						active = true;
						break;
					}
				}
				if (!active)
					continue;

				// Gets the name and uniform location of this member
				const std::string& name = compiler->get_member_name(type.self, i);
				std::string uniformName = Concat({ blockName, ".", name });
				int location = glGetUniformLocation(program, uniformName.c_str());
				if (location == -1)
				{
					if (DevMode())
					{
						std::cout << "Push constant uniform not found: '" << name << "' (expected '" << uniformName
								  << "'). All uniforms:\n";

						GLint numUniforms;
						glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &numUniforms);
						for (int u = 0; u < numUniforms; u++)
						{
							GLsizei uniformNameLen;
							GLint uniformSize;
							GLenum uniformType;

							char uniformNameBuffer[512];
							glGetActiveUniform(
								program, u, sizeof(uniformNameBuffer), &uniformNameLen, &uniformSize, &uniformType,
								uniformNameBuffer);

							std::cout << "  " << uniformNameBuffer << "\n";
						}

						for (const std::string& code : glslCodeStages)
						{
							std::cout << "\n\n --- GLSL ---\n" << code;
						}

						std::cout << std::flush;
					}

					continue;
				}

				static const std::pair<uint32_t, uint32_t> SUPPORTED_DIMENSIONS[] = {
					{ 1, 1 }, { 1, 2 }, { 1, 3 }, { 1, 4 }, { 2, 2 }, { 3, 3 }, { 3, 4 }, { 4, 4 },
				};

				if (!Contains(SUPPORTED_DIMENSIONS, std::make_pair(memberType.columns, memberType.vecsize)))
				{
					Log(LogLevel::Error, "gl", "Unsupported push constant dimensions {0}x{1}", memberType.vecsize,
					    memberType.columns);
					continue;
				}

				PushConstantMember& pushConstant = pushConstants.emplace_back();
				pushConstant.uniformLocation = location;
				pushConstant.arraySize = 1;
				pushConstant.offset = offset;
				pushConstant.baseType = memberType.basetype;
				pushConstant.vectorSize = memberType.vecsize;
				pushConstant.columns = memberType.columns;

				for (uint32_t arraySize : memberType.array)
					pushConstant.arraySize *= arraySize;
			}
		}
	}
}

template <typename T>
struct SetUniformFunctions
{
	void (*Set1)(GLint location, GLsizei count, const T* value);
	void (*Set2)(GLint location, GLsizei count, const T* value);
	void (*Set3)(GLint location, GLsizei count, const T* value);
	void (*Set4)(GLint location, GLsizei count, const T* value);
	void (*Set2x2)(GLint location, GLsizei count, GLboolean transpose, const T* value);
	void (*Set3x3)(GLint location, GLsizei count, GLboolean transpose, const T* value);
	void (*Set3x4)(GLint location, GLsizei count, GLboolean transpose, const T* value);
	void (*Set4x4)(GLint location, GLsizei count, GLboolean transpose, const T* value);
};

template <typename T>
inline void SetPushConstantUniform(
	const SetUniformFunctions<T>& func, const PushConstantMember& pushConst, uint32_t offset, uint32_t range,
	const void* data)
{
	const T* value = reinterpret_cast<const T*>(reinterpret_cast<const char*>(data) + (pushConst.offset - offset));
	if (pushConst.columns == 1)
	{
		if (pushConst.vectorSize == 1)
		{
			func.Set1(pushConst.uniformLocation, pushConst.arraySize, value);
		}
		else if (pushConst.vectorSize == 2)
		{
			func.Set2(pushConst.uniformLocation, pushConst.arraySize, value);
		}
		else if (pushConst.vectorSize == 3)
		{
			T* packedValues = reinterpret_cast<T*>(alloca(pushConst.arraySize * sizeof(T) * 3));
			for (uint32_t i = 0; i < pushConst.arraySize; i++)
			{
				std::memcpy(packedValues + (i * 3), value + (i * 4), sizeof(T) * 3);
			}
			func.Set3(pushConst.uniformLocation, pushConst.arraySize, packedValues);
		}
		else if (pushConst.vectorSize == 4)
		{
			func.Set4(pushConst.uniformLocation, pushConst.arraySize, value);
		}
	}
	else if (pushConst.columns == 2 && pushConst.vectorSize == 2)
	{
		func.Set2x2(pushConst.uniformLocation, pushConst.arraySize, GL_FALSE, value);
	}
	else if (pushConst.columns == 3 && pushConst.vectorSize == 3)
	{
		T* packedValues = reinterpret_cast<T*>(alloca(pushConst.arraySize * sizeof(T) * 9));
		for (uint32_t i = 0; i < pushConst.arraySize * 3; i++)
		{
			std::memcpy(packedValues + (i * 3), value + (i * 4), sizeof(T) * 3);
		}
		func.Set3x3(pushConst.uniformLocation, pushConst.arraySize, GL_FALSE, packedValues);
	}
	else if (pushConst.columns == 3 && pushConst.vectorSize == 4)
	{
		func.Set3x4(pushConst.uniformLocation, pushConst.arraySize, GL_FALSE, value);
	}
	else if (pushConst.columns == 4)
	{
		func.Set4x4(pushConst.uniformLocation, pushConst.arraySize, GL_FALSE, value);
	}
}

void PushConstants(CommandContextHandle, uint32_t offset, uint32_t range, const void* data)
{
	for (const PushConstantMember& pushConst : currentPipeline->pushConstants)
	{
		if (pushConst.offset < offset || pushConst.offset >= offset + range)
			continue;

		switch (pushConst.baseType)
		{
		case SPIRType::Float:
		{
			SetUniformFunctions<float> func = {
				.Set1 = glUniform1fv,
				.Set2 = glUniform2fv,
				.Set3 = glUniform3fv,
				.Set4 = glUniform4fv,
				.Set2x2 = glUniformMatrix2fv,
				.Set3x3 = glUniformMatrix3fv,
				.Set3x4 = glUniformMatrix3x4fv,
				.Set4x4 = glUniformMatrix4fv,
			};
			SetPushConstantUniform<float>(func, pushConst, offset, range, data);
			break;
		}
		case SPIRType::Boolean:
		case SPIRType::Int:
		{
			SetUniformFunctions<int32_t> func = {
				.Set1 = glUniform1iv,
				.Set2 = glUniform2iv,
				.Set3 = glUniform3iv,
				.Set4 = glUniform4iv,
			};
			SetPushConstantUniform<int32_t>(func, pushConst, offset, range, data);
			break;
		}
		case SPIRType::UInt:
		{
			SetUniformFunctions<uint32_t> func = {
				.Set1 = glUniform1uiv,
				.Set2 = glUniform2uiv,
				.Set3 = glUniform3uiv,
				.Set4 = glUniform4uiv,
			};
			SetPushConstantUniform<uint32_t>(func, pushConst, offset, range, data);
			break;
		}
		default: EG_PANIC("Unknown push constant type.");
		}
	}
}
} // namespace eg::graphics_api::gl
