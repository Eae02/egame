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
		}
		
		return reinterpret_cast<ShaderModuleHandle>(module);
	}
	
	void DestroyShaderModule(ShaderModuleHandle handle)
	{
		shaderModulePool.Delete(UnwrapShaderModule(handle));
	}
}
