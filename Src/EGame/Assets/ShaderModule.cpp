#include "ShaderModule.hpp"
#include "AssetLoad.hpp"

namespace eg
{
	const eg::AssetFormat ShaderModule::AssetFormat { "EG::Shader", 0 };
	
	bool ShaderModule::AssetLoader(const AssetLoadContext& context)
	{
		ShaderModule& result = context.CreateResult<ShaderModule>();
		
		result.stage = (ShaderStage)*reinterpret_cast<const uint32_t*>(context.Data().data());
		
		uint32_t codeSize = *reinterpret_cast<const uint32_t*>(context.Data().data() + 4);
		result.code.resize(codeSize);
		std::memcpy(result.code.data(), context.Data().data() + 8, codeSize);
		
		return true;
	}
}
