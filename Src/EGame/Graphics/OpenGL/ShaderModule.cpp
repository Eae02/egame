#include "ShaderModule.hpp"
#include "../../Alloc/ObjectPool.hpp"

namespace eg::graphics_api::gl
{
	static ConcurrentObjectPool<ShaderModule> shaderModulePool;
	
	ShaderModuleHandle CreateShaderModule(ShaderStage stage, Span<const char> code)
	{
		ShaderModule* module = shaderModulePool.New(code);
		module->stage = stage;
		
		for (spirv_cross::SpecializationConstant& specConst : module->spvCompiler.get_specialization_constants())
		{
			spirv_cross::SPIRConstant& spirConst = module->spvCompiler.get_constant(specConst.id);
			if (specConst.constant_id == 500)
			{
				spirConst.m.c[0].r[0].u32 = 1;
			}
			module->initialSpecConstantValues.push_back(spirConst.m);
		}
		
		return reinterpret_cast<ShaderModuleHandle>(module);
	}
	
	void ShaderModule::ResetSpecializationConstants()
	{
		const auto& specConstants = spvCompiler.get_specialization_constants();
		for (size_t i = 0; i < specConstants.size(); i++)
		{
			spirv_cross::SPIRConstant& spirConst = spvCompiler.get_constant(specConstants[i].id);
			spirConst.m = initialSpecConstantValues[i];
		}
	}
	
	void DestroyShaderModule(ShaderModuleHandle handle)
	{
		shaderModulePool.Delete(UnwrapShaderModule(handle));
	}
}
