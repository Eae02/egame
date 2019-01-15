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
	
	class Includer : public glslang::TShader::Includer
	{
	public:
		explicit Includer(AssetGenerateContext& generateContext)
			: m_generateContext(&generateContext) { }
		
		struct CustomIncludeResult : IncludeResult
		{
			CustomIncludeResult(const std::string& path, std::vector<char> _data)
				: IncludeResult(path, _data.data(), _data.size(), nullptr), data(std::move(_data)) { }
			
			std::vector<char> data;
		};
		
		inline static CustomIncludeResult* TryCreateIncludeResult(const std::string& path)
		{
			std::ifstream stream(path, std::ios::binary);
			if (!stream)
				return nullptr;
			std::vector<char> data = ReadStreamContents(stream);
			return new CustomIncludeResult(path, std::move(data));
		}
		
		IncludeResult* includeLocal(const char* headerName, const char* includerName, size_t size) override
		{
			std::string path = Concat({ ParentPath(includerName), headerName });
			if (IncludeResult* iRes = TryCreateIncludeResult(m_generateContext->ResolveRelPath(path)))
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
					lang = EShLangVertex;
				else if (StringEqualCaseInsensitive(stageName, "fragment"))
					lang = EShLangFragment;
				else
				{
					Log(LogLevel::Error, "as", "{0}: Invalid shader stage {1}, should be 'vertex' or 'fragment'.", sourcePath, stageName);
					return false;
				}
			}
			else
			{
				if (StringEndsWith(sourcePath, ".vs.glsl"))
					lang = EShLangVertex;
				else if (StringEndsWith(sourcePath, ".fs.glsl"))
					lang = EShLangFragment;
				else
				{
					Log(LogLevel::Error, "as", "{0}: Unable to deduce shader stage from file extension.", sourcePath);
					return false;
				}
			}
			
			const EShMessages messages = EShMsgSpvRules;
			
			//Sets up parameters for the shader
			const char* shaderStrings[] = { source.data() };
			const int shaderStringLengths[] = { static_cast<int>(source.size()) };
			const char* shaderStringNames[] = { relSourcePath.c_str() };
			
			glslang::TShader shader(lang);
			shader.setPreamble(extensionsStr);
			shader.setStringsWithLengthsAndNames(shaderStrings, shaderStringLengths, shaderStringNames, 1);
			shader.setEnvClient(glslang::EShClient::EShClientOpenGL, 450);
			//shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
			
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
			default: std::abort();
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
		RegisterAssetGenerator<ShaderGenerator>("Shader", ShaderModule::AssetFormat);
	}
}
