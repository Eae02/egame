#include "ShaderModule.hpp"
#include "AssetLoad.hpp"
#include "../Graphics/AbstractionHL.hpp"

namespace eg
{
	const eg::AssetFormat ShaderModuleAssetFormat { "EG::Shader", 0 };
	
	bool ShaderModuleLoader(const AssetLoadContext& context)
	{
		ShaderStage stage = (ShaderStage)*reinterpret_cast<const uint32_t*>(context.Data().data());
		
		uint32_t codeSize = *reinterpret_cast<const uint32_t*>(context.Data().data() + 4);
		Span<const char> code(context.Data().data() + 8, codeSize);
		
		context.CreateResult<ShaderModule>(stage, code);
		
		return true;
	}
}
