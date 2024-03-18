#include "../EGame/Assets/AssetGenerator.hpp"
#include "../EGame/Assets/ShaderModule.hpp"
#include "../EGame/IOUtils.hpp"
#include "../EGame/Log.hpp"
#include "../EGame/Platform/DynamicLibrary.hpp"
#include "../EGame/Platform/FileSystem.hpp"
#include "../Shaders/Build/Inc/Deferred.glh.h"
#include "../Shaders/Build/Inc/EGame.glh.h"
#include "ShaderResource.hpp"

#include <algorithm>
#include <fstream>
#include <span>

#include <glslang_c_interface.h>
#include <yaml-cpp/yaml.h>

namespace eg::asset_gen
{
static std::optional<glslang_stage_t> DeduceShaderStage(std::string_view sourcePath, const YAML::Node& yamlNode)
{
	if (const YAML::Node& stageNode = yamlNode["stage"])
	{
		const std::pair<std::string_view, glslang_stage_t> stageNames[] = {
			{ "vertex", GLSLANG_STAGE_VERTEX },
			{ "fragment", GLSLANG_STAGE_FRAGMENT },
			{ "geometry", GLSLANG_STAGE_GEOMETRY },
			{ "compute", GLSLANG_STAGE_COMPUTE },
			{ "tess-control", GLSLANG_STAGE_TESSCONTROL },
			{ "tess-eval", GLSLANG_STAGE_TESSEVALUATION },
		};

		std::string stageNameStr = stageNode.as<std::string>();
		for (const std::pair<std::string_view, glslang_stage_t>& stageName : stageNames)
		{
			if (StringEqualCaseInsensitive(stageNameStr, stageName.first))
			{
				return stageName.second;
			}
		}

		std::ostringstream errorStream;
		errorStream << sourcePath << ": Invalid shader stage " << stageNameStr << ", should be ";
		for (size_t i = 0; i < std::size(stageNames); i++)
		{
			if (i == std::size(stageNames) - 1)
				errorStream << " or ";
			else if (i != 0)
				errorStream << ", ";
			errorStream << "'" << stageNames[i].first << "'";
		}

		Log(LogLevel::Error, "as", "{0}", errorStream.str());
	}
	else
	{
		const std::pair<std::string_view, glslang_stage_t> stageExtensions[] = {
			{ ".vs.glsl", GLSLANG_STAGE_VERTEX },        { ".vert", GLSLANG_STAGE_VERTEX },
			{ ".vert.glsl", GLSLANG_STAGE_VERTEX },      { ".fs.glsl", GLSLANG_STAGE_FRAGMENT },
			{ ".frag", GLSLANG_STAGE_FRAGMENT },         { ".frag.glsl", GLSLANG_STAGE_FRAGMENT },
			{ ".gs.glsl", GLSLANG_STAGE_GEOMETRY },      { ".geom", GLSLANG_STAGE_GEOMETRY },
			{ ".geom.glsl", GLSLANG_STAGE_GEOMETRY },    { ".cs.glsl", GLSLANG_STAGE_COMPUTE },
			{ ".comp", GLSLANG_STAGE_COMPUTE },          { ".comp.glsl", GLSLANG_STAGE_COMPUTE },
			{ ".tcs.glsl", GLSLANG_STAGE_TESSCONTROL },  { ".tesc", GLSLANG_STAGE_TESSCONTROL },
			{ ".tesc.glsl", GLSLANG_STAGE_TESSCONTROL }, { ".tes.glsl", GLSLANG_STAGE_TESSEVALUATION },
			{ ".tese", GLSLANG_STAGE_TESSEVALUATION },   { ".tese.glsl", GLSLANG_STAGE_TESSEVALUATION },
		};

		for (const std::pair<std::string_view, glslang_stage_t>& stageExtension : stageExtensions)
		{
			if (sourcePath.ends_with(stageExtension.first))
			{
				return stageExtension.second;
			}
		}

		Log(LogLevel::Error, "as", "{0}: Unable to deduce shader stage from file extension.", sourcePath);
	}
	return {};
}

namespace glslang
{
bool hasTriedToLoadGlslang = false;
bool successfullyLoadedGlslang = false;
eg::DynamicLibrary glslangLibrary;
eg::DynamicLibrary spirvLibrary;

decltype(&::glslang_initialize_process) initialize_process;
decltype(&::glslang_finalize_process) finalize_process;
decltype(&::glslang_shader_create) shader_create;
decltype(&::glslang_shader_delete) shader_delete;
decltype(&::glslang_shader_preprocess) shader_preprocess;
decltype(&::glslang_shader_parse) shader_parse;
decltype(&::glslang_shader_get_preprocessed_code) shader_get_preprocessed_code;
decltype(&::glslang_shader_get_info_log) shader_get_info_log;
decltype(&::glslang_shader_get_info_debug_log) shader_get_info_debug_log;
decltype(&::glslang_program_create) program_create;
decltype(&::glslang_program_delete) program_delete;
decltype(&::glslang_program_add_shader) program_add_shader;
decltype(&::glslang_program_link) program_link;
decltype(&::glslang_program_get_info_log) program_get_info_log;
decltype(&::glslang_program_get_info_debug_log) program_get_info_debug_log;

decltype(&::glslang_program_SPIRV_generate) program_SPIRV_generate;
decltype(&::glslang_program_SPIRV_generate_with_options) program_SPIRV_generate_with_options;
decltype(&::glslang_program_SPIRV_get_size) program_SPIRV_get_size;
decltype(&::glslang_program_SPIRV_get) program_SPIRV_get;
decltype(&::glslang_program_SPIRV_get_ptr) program_SPIRV_get_ptr;
decltype(&::glslang_program_SPIRV_get_messages) program_SPIRV_get_messages;

void LoadGlslangLibrary()
{
	hasTriedToLoadGlslang = true;

	std::string glslangLibraryName = eg::DynamicLibrary::PlatformFormat("glslang");
	if (!glslangLibrary.Open(glslangLibraryName.c_str()))
	{
		eg::Log(
			eg::LogLevel::Error, "as", "Failed to load glslang library for shader compilation: {0}",
			glslangLibrary.FailureReason());
		return;
	}

	std::string spirvLibraryName = eg::DynamicLibrary::PlatformFormat("SPIRV");
	if (!spirvLibrary.Open(spirvLibraryName.c_str()))
	{
		eg::Log(
			eg::LogLevel::Error, "as", "Failed to load spirv library for shader compilation: {0}",
			spirvLibrary.FailureReason());
		return;
	}

	successfullyLoadedGlslang = true;

	constexpr const char* entryPointNotFoundMessage =
		"Failed to initialize shader compiler: Entry point not found: glslang_{0}";

#define LOAD_GLSLANG_SYMBOL(library, name)                                                                             \
	if (!(name = reinterpret_cast<decltype(name)>(library.GetSymbol("glslang_" #name))))                               \
		eg::Log(eg::LogLevel::Error, "as", entryPointNotFoundMessage, #name);

	LOAD_GLSLANG_SYMBOL(glslangLibrary, initialize_process)
	LOAD_GLSLANG_SYMBOL(glslangLibrary, finalize_process)
	LOAD_GLSLANG_SYMBOL(glslangLibrary, shader_create)
	LOAD_GLSLANG_SYMBOL(glslangLibrary, shader_delete)
	LOAD_GLSLANG_SYMBOL(glslangLibrary, shader_preprocess)
	LOAD_GLSLANG_SYMBOL(glslangLibrary, shader_parse)
	LOAD_GLSLANG_SYMBOL(glslangLibrary, shader_get_preprocessed_code)
	LOAD_GLSLANG_SYMBOL(glslangLibrary, shader_get_info_log)
	LOAD_GLSLANG_SYMBOL(glslangLibrary, shader_get_info_debug_log)
	LOAD_GLSLANG_SYMBOL(glslangLibrary, program_create)
	LOAD_GLSLANG_SYMBOL(glslangLibrary, program_delete)
	LOAD_GLSLANG_SYMBOL(glslangLibrary, program_add_shader)
	LOAD_GLSLANG_SYMBOL(glslangLibrary, program_link)
	LOAD_GLSLANG_SYMBOL(glslangLibrary, program_get_info_log)
	LOAD_GLSLANG_SYMBOL(glslangLibrary, program_get_info_debug_log)
	LOAD_GLSLANG_SYMBOL(spirvLibrary, program_SPIRV_generate)
	LOAD_GLSLANG_SYMBOL(spirvLibrary, program_SPIRV_generate_with_options)
	LOAD_GLSLANG_SYMBOL(spirvLibrary, program_SPIRV_get_size)
	LOAD_GLSLANG_SYMBOL(spirvLibrary, program_SPIRV_get)
	LOAD_GLSLANG_SYMBOL(spirvLibrary, program_SPIRV_get_ptr)
	LOAD_GLSLANG_SYMBOL(spirvLibrary, program_SPIRV_get_messages)

	if (!initialize_process())
	{
		eg::Log(eg::LogLevel::Error, "as", "Failed to initialize glslang library for shader compilation");
		successfullyLoadedGlslang = false;
	}
}
} // namespace glslang

struct CustomIncludeResult : glsl_include_result_t
{
	CustomIncludeResult()
	{
		header_name = nullptr;
		header_data = nullptr;
		header_length = 0;
	}

	CustomIncludeResult(std::string_view name, std::vector<char> _data)
		: nameBuffer(name.size() + 1), data(std::move(_data))
	{
		std::copy(name.begin(), name.end(), nameBuffer.begin());
		nameBuffer.back() = '\0';
		header_name = nameBuffer.data();
		header_data = data.data();
		header_length = data.size();
	}

	std::vector<char> nameBuffer;
	std::vector<char> data;
};

inline static CustomIncludeResult* TryCreateIncludeResult(const std::string& path, std::string_view name)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream)
		return nullptr;
	std::vector<char> data = ReadStreamContents(stream);
	return new CustomIncludeResult(name, std::move(data));
}

glsl_include_result_t* includeCallbackSystem(
	void* ctx, const char* headerName, const char* includerName, size_t includeDepth)
{
	auto CheckSystemHeader = [&](std::string_view name,
	                             std::span<const unsigned char> headerData) -> CustomIncludeResult*
	{
		if (name != headerName)
			return nullptr;
		const char* headerDataChar = reinterpret_cast<const char*>(headerData.data());
		return new CustomIncludeResult(name, { headerDataChar, headerDataChar + headerData.size() });
	};

	if (auto result = CheckSystemHeader("EGame.glh", Inc_EGame_glh))
		return result;
	if (auto result = CheckSystemHeader("Deferred.glh", Inc_Deferred_glh))
		return result;
	return nullptr;
}

glsl_include_result_t* includeCallbackLocal(
	void* ctx, const char* headerName, const char* includerName, size_t includeDepth)
{
	AssetGenerateContext* generateContext = static_cast<AssetGenerateContext*>(ctx);

	std::string_view includer = includerName;
	if (includer.empty())
		includer = generateContext->AssetName();

	std::string path = Concat({ ParentPath(includer), headerName });
	if (auto iRes = TryCreateIncludeResult(generateContext->ResolveRelPath(path), path))
	{
		generateContext->FileDependency(std::move(path));
		return iRes;
	}
	return nullptr;
}

int includeCallbackFree(void* ctx, glsl_include_result_t* result)
{
	delete static_cast<CustomIncludeResult*>(result);
	return 0;
}

class ShaderGenerator : public AssetGenerator
{
public:
	ShaderGenerator() {}

	bool Generate(AssetGenerateContext& generateContext) override
	{
		if (!glslang::hasTriedToLoadGlslang)
			glslang::LoadGlslangLibrary();
		if (!glslang::successfullyLoadedGlslang)
			return false;

		std::string relSourcePath = generateContext.RelSourcePath();
		std::string sourcePath = generateContext.FileDependency(relSourcePath);
		std::ifstream sourceStream(sourcePath, std::ios::binary);
		if (!sourceStream)
		{
			Log(LogLevel::Error, "as", "Error opening asset file for reading: '{0}'", sourcePath);
			return false;
		}

		std::vector<char> source = ReadStreamContents(sourceStream);
		sourceStream.close();

		std::string_view sourceVersionDirective;
		std::string_view sourceAfterVersionDirective(source.data(), source.size());
		if (sourceAfterVersionDirective.starts_with("#version"))
		{
			size_t firstNewLine = sourceAfterVersionDirective.find('\n');
			if (firstNewLine != std::string_view::npos)
			{
				sourceVersionDirective = sourceAfterVersionDirective.substr(0, firstNewLine);
				sourceAfterVersionDirective = sourceAfterVersionDirective.substr(firstNewLine + 1);
			}
		}

		std::optional<glslang_stage_t> lang = DeduceShaderStage(sourcePath, generateContext.YAMLNode());
		if (!lang.has_value())
			return false;

		std::vector<std::string_view> variants;

		IterateStringParts(
			std::string_view(source.data(), source.size()), '\n',
			[&](std::string_view line)
			{
				std::string_view trimmedLine = TrimString(line);
				std::vector<std::string_view> words;
				SplitString(trimmedLine, ' ', words);
				if (words.size() >= 3 && words[0] == "#pragma" && words[1] == "variants")
				{
					for (size_t i = 2; i < words.size(); i++)
						variants.push_back(words[i]);
				}
			});

		if (variants.empty())
		{
			variants.push_back("_VDEFAULT");
		}
		else
		{
			std::sort(variants.begin(), variants.end());
			variants.erase(std::unique(variants.begin(), variants.end()), variants.end());
		}

		// Translates the shader stage and writes this to the output stream
		ShaderStage egStage;
		switch (*lang)
		{
		case GLSLANG_STAGE_VERTEX: egStage = ShaderStage::Vertex; break;
		case GLSLANG_STAGE_FRAGMENT: egStage = ShaderStage::Fragment; break;
		case GLSLANG_STAGE_GEOMETRY: egStage = ShaderStage::Geometry; break;
		case GLSLANG_STAGE_COMPUTE: egStage = ShaderStage::Compute; break;
		case GLSLANG_STAGE_TESSCONTROL: egStage = ShaderStage::TessControl; break;
		case GLSLANG_STAGE_TESSEVALUATION: egStage = ShaderStage::TessEvaluation; break;
		default: EG_UNREACHABLE break;
		}
		generateContext.writer.Write(static_cast<uint32_t>(egStage));

		generateContext.writer.Write(UnsignedNarrow<uint32_t>(variants.size()));

		glsl_include_callbacks_s includeCallbacks = {
			.include_system = includeCallbackSystem,
			.include_local = includeCallbackLocal,
			.free_include_result = includeCallbackFree,
		};

		// Compiles each shader variant
		for (std::string_view variant : variants)
		{
			std::ostringstream fullSourceStream;
			fullSourceStream << sourceVersionDirective
							 << "\n#extension GL_GOOGLE_include_directive:enable\n"
								"#extension GL_GOOGLE_cpp_style_line_directive:enable\n"
								"#define "
							 << variant << "\n#line 2\n"
							 << sourceAfterVersionDirective;
			std::string fullSourceCode = fullSourceStream.str();

			glslang_input_s shaderInput = {
				.language = GLSLANG_SOURCE_GLSL,
				.stage = *lang,
				.client = GLSLANG_CLIENT_VULKAN,
				.client_version = GLSLANG_TARGET_VULKAN_1_1,
				.target_language = GLSLANG_TARGET_SPV,
				.target_language_version = GLSLANG_TARGET_SPV_1_3,
				.code = fullSourceCode.c_str(),
				.default_version = 450,
				.default_profile = GLSLANG_NO_PROFILE,
				.force_default_version_and_profile = false,
				.forward_compatible = true,
				.messages = GLSLANG_MSG_DEFAULT_BIT,
				.resource = &DefaultTBuiltInResource,
				.callbacks = includeCallbacks,
				.callbacks_ctx = &generateContext,
			};

			glslang_shader_t* shader = glslang::shader_create(&shaderInput);

			std::unique_ptr<glslang_shader_t, decltype(glslang::shader_delete)> shaderUniquePtr(
				shader, glslang::shader_delete);

			if (!glslang::shader_preprocess(shader, &shaderInput))
			{
				Log(LogLevel::Error, "as", "Shader ({0}:{1}) failed to compile (preprocessing): {2}", sourcePath,
				    variant, glslang::shader_get_info_log(shader));
				return false;
			}

			if (!glslang::shader_parse(shader, &shaderInput))
			{
				Log(LogLevel::Error, "as", "Shader ({0}:{1}) failed to compile (parse): {2}", sourcePath, variant,
				    glslang::shader_get_info_log(shader));
				return false;
			}

			glslang_program_t* program = glslang::program_create();

			std::unique_ptr<glslang_program_t, decltype(glslang::program_delete)> programUniquePtr(
				program, glslang::program_delete);

			glslang::program_add_shader(program, shader);

			if (!glslang::program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT))
			{
				Log(LogLevel::Error, "as", "Shader ({0}:{1}) failed to compile (link): {2}", sourcePath, variant,
				    glslang::program_get_info_log(program));
				return false;
			}

			glslang::program_SPIRV_generate(program, *lang);

			const char* spirv_messages = glslang::program_SPIRV_get_messages(program);
			if (spirv_messages)
			{
				Log(LogLevel::Warning, "as", "Shader ({0}:{1}) produced spir-v messages:\n{2}", sourcePath, variant,
				    spirv_messages);
			}

			const uint64_t codeSize = glslang::program_SPIRV_get_size(program) * sizeof(uint32_t);
			std::span<char> spirvData(reinterpret_cast<char*>(glslang::program_SPIRV_get_ptr(program)), codeSize);

			generateContext.writer.WriteString(variant);
			generateContext.writer.Write<uint64_t>(codeSize);
			generateContext.writer.WriteBytes(spirvData);

			eg::Log(eg::LogLevel::Info, "sh", "Compiling shader: {0}", generateContext.AssetName());
		}

		return true;
	}
};

void RegisterShaderGenerator()
{
	RegisterAssetGenerator<ShaderGenerator>("Shader", ShaderModuleAsset::AssetFormat);
}
} // namespace eg::asset_gen
