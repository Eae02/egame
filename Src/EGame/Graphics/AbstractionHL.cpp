#include "AbstractionHL.hpp"
#include "../IOUtils.hpp"

#include <fstream>

namespace eg
{
	CommandContext DC;
	
	BlendState AlphaBlend { BlendFunc::Add, BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha };
	
	void ShaderProgram::AddStageFromFile(const std::string& path)
	{
		ShaderStage stage;
		if (StringEndsWith(path, ".fs.spv") || StringEndsWith(path, ".frag.spv"))
			stage = ShaderStage::Fragment;
		else if (StringEndsWith(path, ".vs.spv") || StringEndsWith(path, ".vert.spv"))
			stage = ShaderStage::Vertex;
		else
			EG_PANIC("Unrecognized shader stage file extension in '" << path << "'.");
		
		std::ifstream stream(path, std::ios::binary);
		if (!stream)
		{
			EG_PANIC("Error opening shader file for reading: " << path);
		}
		
		std::vector<char> code = ReadStreamContents(stream);
		AddStage(stage, std::move(code));
	}
}
