#include "../../Inc/Common.hpp"
#include "../EGame/Assets/ShaderModule.hpp"
#include "../EGame/Assets/AssetGenerator.hpp"
#include "../EGame/Log.hpp"
#include "../EGame/IOUtils.hpp"
#include "../EGame/Platform/FileSystem.hpp"
#include "ShaderResource.hpp"

#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <fstream>

namespace eg::asset_gen
{
	static const char* extensionsStr = "#extension GL_GOOGLE_include_directive:enable\n"
	                                   "#extension GL_GOOGLE_cpp_style_line_directive:enable\n";
	
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
			
			//Detects the shader language (stage)
			EShLanguage lang;
			if (const YAML::Node& stageNode = generateContext.YAMLNode()["stage"])
			{
				std::string stageName = stageNode.as<std::string>();
				if (StringEqualCaseInsensitive(stageName, "vertex"))
				{
					lang = EShLangVertex;
				}
				else if (StringEqualCaseInsensitive(stageName, "fragment"))
				{
					lang = EShLangFragment;
				}
				else if (StringEqualCaseInsensitive(stageName, "geometry"))
				{
					lang = EShLangGeometry;
				}
				else if (StringEqualCaseInsensitive(stageName, "tess-control"))
				{
					lang = EShLangTessControl;
				}
				else if (StringEqualCaseInsensitive(stageName, "tess-eval"))
				{
					lang = EShLangTessEvaluation;
				}
				else
				{
					Log(LogLevel::Error, "as", "{0}: Invalid shader stage {1}, should be 'vertex', 'fragment', "
								"'geometry', 'tess-control' or 'tess-eval'.", sourcePath, stageName);
					return false;
				}
			}
			else
			{
				if (StringEndsWith(sourcePath, ".vs.glsl") ||
					StringEndsWith(sourcePath, ".vert") ||
					StringEndsWith(sourcePath, ".vert.glsl"))
				{
					lang = EShLangVertex;
				}
				else if (StringEndsWith(sourcePath, ".fs.glsl") ||
					StringEndsWith(sourcePath, ".frag") ||
					StringEndsWith(sourcePath, ".frag.glsl"))
				{
					lang = EShLangFragment;
				}
				else if (StringEndsWith(sourcePath, ".gs.glsl") ||
					StringEndsWith(sourcePath, ".geom") ||
					StringEndsWith(sourcePath, ".geom.glsl"))
				{
					lang = EShLangGeometry;
				}
				else if (StringEndsWith(sourcePath, ".tcs.glsl") ||
					StringEndsWith(sourcePath, ".tesc") ||
					StringEndsWith(sourcePath, ".tesc.glsl"))
				{
					lang = EShLangTessControl;
				}
				else if (StringEndsWith(sourcePath, ".tes.glsl") ||
					StringEndsWith(sourcePath, ".tese") ||
					StringEndsWith(sourcePath, ".tese.glsl"))
				{
					lang = EShLangTessEvaluation;
				}
				else
				{
					Log(LogLevel::Error, "as", "{0}: Unable to deduce shader stage from file extension.", sourcePath);
					return false;
				}
			}
			
			const EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
			
			//Sets up parameters for the shader
			const char* shaderStrings[] = { source.data() };
			const int shaderStringLengths[] = { static_cast<int>(source.size()) };
			const char* shaderStringNames[] = { relSourcePath.c_str() };
			
			glslang::TShader shader(lang);
			shader.setPreamble(extensionsStr);
			shader.setStringsWithLengthsAndNames(shaderStrings, shaderStringLengths, shaderStringNames, 1);
			shader.setEnvClient(glslang::EShClient::EShClientOpenGL, glslang::EShTargetVulkan_1_0);
			shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
			
			Includer includer(generateContext);
			if (!shader.parse(&DefaultTBuiltInResource, 450, ECoreProfile, true, false, messages, includer))
			{
				Log(LogLevel::Error, "as", "Shader failed to compile: {0}", shader.getInfoLog());
				return false;
			}
			
			glslang::TProgram program;
			program.addShader(&shader);
			if (!program.link(messages))
			{
				Log(LogLevel::Error, "as", "Shader failed to compile: {0}", program.getInfoLog());
				return false;
			}
			
			std::vector<uint32_t> spirvCode;
			glslang::GlslangToSpv(*program.getIntermediate(lang), spirvCode);
			
			ShaderStage egStage;
			switch (lang)
			{
			case EShLangVertex: egStage = ShaderStage::Vertex; break;
			case EShLangFragment: egStage = ShaderStage::Fragment; break;
			case EShLangGeometry: egStage = ShaderStage::Geometry; break;
			case EShLangTessControl: egStage = ShaderStage::TessControl; break;
			case EShLangTessEvaluation: egStage = ShaderStage::TessEvaluation; break;
			default: EG_UNREACHABLE
			}
			
			uint32_t codeSize = spirvCode.size() * sizeof(uint32_t);
			BinWrite(generateContext.outputStream, (uint32_t)egStage);
			BinWrite(generateContext.outputStream, codeSize);
			generateContext.outputStream.write((char*)spirvCode.data(), codeSize);
			
			return true;
		}
	};
	
	void RegisterShaderGenerator()
	{
		RegisterAssetGenerator<ShaderGenerator>("Shader", ShaderModuleAssetFormat);
	}
}
