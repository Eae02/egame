#include "MetalShaderModule.hpp"
#include "../../Alloc/ObjectPool.hpp"

namespace eg::graphics_api::mtl
{
static ConcurrentObjectPool<ShaderModule> shaderModulePool;

ShaderModuleHandle CreateShaderModule(ShaderStage stage, const spirv_cross::ParsedIR& parsedIR)
{
	ShaderModule* module = shaderModulePool.New();
	module->stage = stage;
	module->parsedIR = &parsedIR;
	return reinterpret_cast<ShaderModuleHandle>(module);
}

void DestroyShaderModule(ShaderModuleHandle handle)
{
	shaderModulePool.Delete(ShaderModule::Unwrap(handle));
}
} // namespace eg::graphics_api::mtl
