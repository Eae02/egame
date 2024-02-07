#include "ShaderModule.hpp"
#include "../../Alloc/ObjectPool.hpp"

#include <spirv_parser.hpp>

namespace eg::graphics_api::gl
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
	shaderModulePool.Delete(UnwrapShaderModule(handle));
}
} // namespace eg::graphics_api::gl
