#include "../EGame/Assets/ShaderModule.hpp"
#include "../EGame/Assets/AssetGenerator.hpp"
#include "../EGame/Log.hpp"
#include "../EGame/IOUtils.hpp"
#include "../EGame/Platform/FileSystem.hpp"
#include "../Shaders/Build/Inc/EGame.glh.h"
#include "../Shaders/Build/Inc/Deferred.glh.h"
#include "ShaderResource.hpp"

#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <spirv-tools/optimizer.hpp>

#include <fstream>
#include <span>
#include <algorithm>

namespace eg::asset_gen
{
	class Includer : public glslang::TShader::Includer
	{
	public:
		explicit Includer(AssetGenerateContext& generateContext)
			: m_generateContext(&generateContext) { }
		
		struct CustomIncludeResult : IncludeResult
		{
			CustomIncludeResult(const std::string& name, std::span<const char> _data)
				: IncludeResult(name, _data.data(), _data.size(), nullptr), data(_data) { }
			
			std::span<const char> data;
			std::vector<char> dataOwned;
		};
		
		inline static CustomIncludeResult* TryCreateIncludeResult(const std::string& path, const std::string& name)
		{
			std::ifstream stream(path, std::ios::binary);
			if (!stream)
				return nullptr;
			std::vector<char> data = ReadStreamContents(stream);
			CustomIncludeResult* result = new CustomIncludeResult(name, data);
			result->dataOwned = std::move(data);
			return result;
		}
		
		IncludeResult* includeSystem(const char* headerName, const char* includerName, size_t size) override
		{
			auto CheckSystemHeader = [&] (const char* name, std::span<const unsigned char> headerData) -> CustomIncludeResult*
			{
				if (std::strcmp(name, headerName))
					return nullptr;
				const char* headerDataChar = reinterpret_cast<const char*>(headerData.data());
				return new CustomIncludeResult(headerName, { headerDataChar, headerDataChar + headerData.size() });
			};
			
			if (auto result = CheckSystemHeader("EGame.glh", Inc_EGame_glh))
				return result;
			if (auto result = CheckSystemHeader("Deferred.glh", Inc_Deferred_glh))
				return result;
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
			for (const std::pair<std::string_view, EShLanguage>& stageName : stageNames)
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
			
			for (const std::pair<std::string_view, EShLanguage>& stageExtension : stageExtensions)
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
			
			const EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
			
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
			BinWrite(generateContext.outputStream, static_cast<uint32_t>(egStage));
			
			//Sets up parameters for the shader
			const char* shaderStrings[] = { source.data() };
			const int shaderStringLengths[] = { static_cast<int>(source.size()) };
			const char* shaderStringNames[] = { relSourcePath.c_str() };
			
			eg::BinWrite(generateContext.outputStream, UnsignedNarrow<uint32_t>(variants.size()));
			
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
				
				uint32_t codeSize = UnsignedNarrow<uint32_t>(spirvCode.size() * sizeof(uint32_t));
				BinWrite(generateContext.outputStream, eg::HashFNV1a32(variant));
				BinWrite(generateContext.outputStream, codeSize);
				generateContext.outputStream.write(reinterpret_cast<char*>(spirvCode.data()), codeSize);
			}
			
			return true;
		}
	};
	
	void RegisterShaderGenerator()
	{
		RegisterAssetGenerator<ShaderGenerator>("Shader", ShaderModuleAsset::AssetFormat);
	}
}
