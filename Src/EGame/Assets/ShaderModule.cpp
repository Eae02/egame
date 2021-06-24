#include "ShaderModule.hpp"
#include "AssetLoad.hpp"
#include "../Graphics/AbstractionHL.hpp"

namespace eg
{
	const eg::AssetFormat ShaderModuleAsset::AssetFormat { "EG::Shader", 1 };
	
	bool ShaderModuleAsset::AssetLoader(const AssetLoadContext& context)
	{
		ShaderModuleAsset& result = context.CreateResult<ShaderModuleAsset>();
		
		const char* data = context.Data().data();
		
		const ShaderStage stage = (ShaderStage)reinterpret_cast<const uint32_t*>(data)[0];
		const uint32_t numVariants = reinterpret_cast<const uint32_t*>(data)[1];
		data += sizeof(uint32_t) * 2;
		
		for (uint32_t i = 0; i < numVariants; i++)
		{
			uint32_t variantHash = reinterpret_cast<const uint32_t*>(data)[0];
			uint32_t codeSize    = reinterpret_cast<const uint32_t*>(data)[1];
			data += sizeof(uint32_t) * 2;
			
			result.m_variants.emplace_back(variantHash, stage, std::span<const char>(data, codeSize));
			
			data += codeSize;
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
