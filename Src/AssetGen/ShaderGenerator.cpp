#include "../../Inc/Common.hpp"
#include "../EGame/Assets/ShaderModule.hpp"
#include "../EGame/Assets/AssetGenerator.hpp"
#include "../EGame/Log.hpp"
#include "../EGame/IOUtils.hpp"
#include "../EGame/Platform/FileSystem.hpp"
#include "ShaderResource.hpp"

#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <spirv-tools/optimizer.hpp>
#include <fstream>

namespace eg::asset_gen
{
	static std::string_view EGameHeader = R"(
#ifndef EG_GLH
#define EG_GLH

layout(constant_id=500) const uint _api = 0;
#define EG_VULKAN (_api == 0)
#define EG_OPENGL (_api == 1)
#define EG_FLIPGL(x) (EG_OPENGL ? 1.0 - (x) : (x))

#endif
)";
	
	static std::string_view DeferredGLH = R"(
#ifndef DEFERRED_GLH
#define DEFERRED_GLH

#include <EGame.glh>

vec2 SMEncode(vec3 n)
{
	if (n.z < -0.999)
		return vec2(0.5, 1.0);
	float p = sqrt(n.z * 8.0 + 8.0);
	return vec2(n.xy / p + 0.5);
}

vec3 SMDecode(vec2 s)
{
	vec2 fenc = s * 4.0 - 2.0;
	float f = dot(fenc, fenc);
	float g = sqrt(max(1.0 - f / 4.0, 0.0));
	return normalize(vec3(fenc * g, 1.0 - f / 2.0));
}

vec3 WorldPosFromDepth(float depthH, vec2 screenCoord, mat4 inverseViewProj)
{
	vec4 h = vec4(screenCoord * 2.0 - vec2(1.0), EG_OPENGL ? (depthH * 2.0 - 1.0) : depthH, 1.0);
	if (!EG_OPENGL)
		h.y = -h.y;
	vec4 d = inverseViewProj * h;
	return d.xyz / d.w;
}

#endif
)";
	
	class Includer : public glslang::TShader::Includer
	{
	public:
		explicit Includer(AssetGenerateContext& generateContext)
			: m_generateContext(&generateContext) { }
		
		struct CustomIncludeResult : IncludeResult
		{
			CustomIncludeResult(const std::string& name, std::vector<char> _data)
				: IncludeResult(name, _data.data(), _data.size(), nullptr), data(std::move(_data)) { }
			
			std::vector<char> data;
		};
		
		inline static CustomIncludeResult* TryCreateIncludeResult(const std::string& path, const std::string& name)
		{
			std::ifstream stream(path, std::ios::binary);
			if (!stream)
				return nullptr;
			std::vector<char> data = ReadStreamContents(stream);
			return new CustomIncludeResult(name, std::move(data));
		}
		
		IncludeResult* includeSystem(const char* headerName, const char* includerName, size_t size) override
		{
			if (std::strcmp(headerName, "EGame.glh") == 0)
			{
				return new CustomIncludeResult("EGame.glh", { EGameHeader.begin(), EGameHeader.end() });
			}
			else if (std::strcmp(headerName, "Deferred.glh") == 0)
			{
				return new CustomIncludeResult("EGame.glh", { DeferredGLH.begin(), DeferredGLH.end() });
			}
			
			return nullptr;
		}
		
		IncludeResult* includeLocal(const char* headerName, const char* includerName, size_t size) override
		{
			std::string path = Concat({ ParentPath(includerName), headerName });
			if (IncludeResult* iRes = TryCreateIncludeResult(m_generateContext->ResolveRelPath(path), path))
			{
				m_generateContext->FileDependency(std::move(path));
				return iRes;
			}
			return nullptr;
		}
		
		void releaseInclude(IncludeResult* result) override
		{
			delete static_cast<CustomIncludeResult*>(result);
		}
		
	private:
		AssetGenerateContext* m_generateContext;
	};
	
	static std::optional<EShLanguage> DeduceShaderLanguage(std::string_view sourcePath, const YAML::Node& yamlNode)
	{
		if (const YAML::Node& stageNode = yamlNode["stage"])
		{
			const std::pair<std::string_view, EShLanguage> stageNames[] = 
			{
				{ "vertex", EShLangVertex },
				{ "fragment", EShLangFragment },
				{ "geometry", EShLangGeometry },
				{ "compute", EShLangCompute },
				{ "tess-control", EShLangTessControl },
				{ "tess-eval", EShLangTessEvaluation }
			};
			
			std::string stageNameStr = stageNode.as<std::string>();
			for (const std::pair<std::string_view, EShLanguage> stageName : stageNames)
			{
				if (StringEqualCaseInsensitive(stageNameStr, stageName.first))
				{
					return stageName.second;
				}
			}
			
			std::ostringstream errorStream;
			errorStream << sourcePath << ": Invalid shader stage " << stageNameStr << ", should be ";
			for (size_t i = 0; i < ArrayLen(stageNames); i++)
			{
				if (i == ArrayLen(stageNames) - 1)
					errorStream << " or ";
				else if (i != 0)
					errorStream << ", ";
				errorStream << "'" << stageNames[i].first << "'";
			}
			
			Log(LogLevel::Error, "as", "{0}", errorStream.str());
		}
		else
		{
			const std::pair<std::string_view, EShLanguage> stageExtensions[] = 
			{
				{ ".vs.glsl", EShLangVertex },
				{ ".vert", EShLangVertex },
				{ ".vert.glsl", EShLangVertex },
				{ ".fs.glsl", EShLangFragment },
				{ ".frag", EShLangFragment },
				{ ".frag.glsl", EShLangFragment },
				{ ".gs.glsl", EShLangGeometry },
				{ ".geom", EShLangGeometry },
				{ ".geom.glsl", EShLangGeometry },
				{ ".cs.glsl", EShLangCompute },
				{ ".comp", EShLangCompute },
				{ ".comp.glsl", EShLangCompute },
				{ ".tcs.glsl", EShLangTessControl },
				{ ".tesc", EShLangTessControl },
				{ ".tesc.glsl", EShLangTessControl },
				{ ".tes.glsl", EShLangTessEvaluation },
				{ ".tese", EShLangTessEvaluation },
				{ ".tese.glsl", EShLangTessEvaluation }
			};
			
			for (const std::pair<std::string_view, EShLanguage> stageExtension : stageExtensions)
			{
				if (StringEndsWith(sourcePath, stageExtension.first))
				{
					return stageExtension.second;
				}
			}
			
			Log(LogLevel::Error, "as", "{0}: Unable to deduce shader stage from file extension.", sourcePath);
		}
		return {};
	}
	
	class ShaderGenerator : public AssetGenerator
	{
	public:
		ShaderGenerator()
		{
			glslang::InitializeProcess();
		}
		
		~ShaderGenerator() override
		{
			glslang::FinalizeProcess();
		}
		
		bool Generate(AssetGenerateContext& generateContext) override
		{
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
			
			std::optional<EShLanguage> lang = DeduceShaderLanguage(sourcePath, generateContext.YAMLNode());
			if (!lang.has_value())
				return false;
			
			std::vector<std::string_view> variants;
			
			IterateStringParts(std::string_view(source.data(), source.size()), '\n', [&] (std::string_view line)
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
			
			const EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
			
			//Translates the shader stage and writes this to the output stream
			ShaderStage egStage;
			switch (*lang)
			{
			case EShLangVertex: egStage = ShaderStage::Vertex; break;
			case EShLangFragment: egStage = ShaderStage::Fragment; break;
			case EShLangGeometry: egStage = ShaderStage::Geometry; break;
			case EShLangCompute: egStage = ShaderStage::Compute; break;
			case EShLangTessControl: egStage = ShaderStage::TessControl; break;
			case EShLangTessEvaluation: egStage = ShaderStage::TessEvaluation; break;
			default: EG_UNREACHABLE break;
			}
			BinWrite(generateContext.outputStream, (uint32_t)egStage);
			
			//Sets up parameters for the shader
			const char* shaderStrings[] = { source.data() };
			const int shaderStringLengths[] = { static_cast<int>(source.size()) };
			const char* shaderStringNames[] = { relSourcePath.c_str() };
			
			eg::BinWrite<uint32_t>(generateContext.outputStream, variants.size());
			
			//Compiles each shader variant
			for (std::string_view variant : variants)
			{
				std::ostringstream preambleStream;
				preambleStream << "#extension GL_GOOGLE_include_directive:enable\n"
				                  "#extension GL_GOOGLE_cpp_style_line_directive:enable\n"
				                  "#define " << variant << "\n";
				std::string preamble = preambleStream.str();
				
				glslang::TShader shader(*lang);
				shader.setPreamble(preamble.c_str());
				shader.setStringsWithLengthsAndNames(shaderStrings, shaderStringLengths, shaderStringNames, 1);
				shader.setEnvClient(glslang::EShClient::EShClientOpenGL, glslang::EShTargetVulkan_1_0);
				shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
				
				Includer includer(generateContext);
				if (!shader.parse(&DefaultTBuiltInResource, 450, ECoreProfile, true, false, messages, includer))
				{
					Log(LogLevel::Error, "as", "Shader ({0}:{1}) failed to compile: {2}",
						sourcePath, variant, shader.getInfoLog());
					return false;
				}
				
				glslang::TProgram program;
				program.addShader(&shader);
				if (!program.link(messages))
				{
					Log(LogLevel::Error, "as", "Shader ({0}:{1}) failed to compile: {2}",
						sourcePath, variant, program.getInfoLog());
					return false;
				}
				
				//Generates SPIRV
				std::vector<uint32_t> spirvCode;
				glslang::GlslangToSpv(*program.getIntermediate(*lang), spirvCode);
				
				uint32_t codeSize = spirvCode.size() * sizeof(uint32_t);
				BinWrite(generateContext.outputStream, eg::HashFNV1a32(variant));
				BinWrite(generateContext.outputStream, codeSize);
				generateContext.outputStream.write((char*)spirvCode.data(), codeSize);
			}
			
			return true;
		}
	};
	
	void RegisterShaderGenerator()
	{
		RegisterAssetGenerator<ShaderGenerator>("Shader", ShaderModuleAsset::AssetFormat);
	}
}
