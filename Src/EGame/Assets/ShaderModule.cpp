#include "ShaderModule.hpp"
#include "AssetLoad.hpp"
#include "../Graphics/AbstractionHL.hpp"

namespace eg
{
	const eg::AssetFormat ShaderModuleAsset::AssetFormat { "EG::Shader", 1 };
	
	bool ShaderModuleAsset::AssetLoader(const AssetLoadContext& context)
	{
		size_t pos = 0;
		
		ShaderModuleAsset& result = context.CreateResult<ShaderModuleAsset>();
		
		const ShaderStage stage = (ShaderStage)context.Data().AtAs<const uint32_t>(pos + 0);
		const uint32_t numVariants = context.Data().AtAs<const uint32_t>(pos + 4);
		pos += 8;
		
		for (uint32_t i = 0; i < numVariants; i++)
		{
			uint32_t variantHash = context.Data().AtAs<const uint32_t>(pos + 0);
			uint32_t codeSize = context.Data().AtAs<const uint32_t>(pos + 4);
			Span<const char> code(context.Data().data() + pos + 8, codeSize);
			
			result.m_variants.emplace_back(variantHash, stage, code);
			
			pos += 8 + codeSize;
		}
		
		return true;
	}
	
	ShaderModuleHandle ShaderModuleAsset::GetVariant(std::string_view name) const
	{
		uint32_t hash = HashFNV1a32(name);
		for (const Variant& variant : m_variants)
		{
			if (variant.hash == hash)
			{
				return variant.shaderModule.Handle();
			}
		}
		EG_PANIC("Shader module variant not found: '" << name << "'");
	}
	
	ShaderModuleHandle ShaderModuleAsset::DefaultVariant() const
	{
		return GetVariant("_VDEFAULT");
	}
}
