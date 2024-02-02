#include "ShaderModule.hpp"
#include "../../Alloc/ObjectPool.hpp"

#include <spirv_parser.hpp>

namespace eg::graphics_api::gl
{
static ConcurrentObjectPool<ShaderModule> shaderModulePool;

ShaderModuleHandle CreateShaderModule(ShaderStage stage, std::span<const char> code)
{
	ShaderModule* module = shaderModulePool.New();
	module->stage = stage;

	spirv_cross::Parser parser(reinterpret_cast<const uint32_t*>(code.data()), code.size() / sizeof(uint32_t));
	parser.parse();
	module->parsedIR = parser.get_parsed_ir();

	return reinterpret_cast<ShaderModuleHandle>(module);
}

void DestroyShaderModule(ShaderModuleHandle handle)
{
	shaderModulePool.Delete(UnwrapShaderModule(handle));
}
} // namespace eg::graphics_api::gl
