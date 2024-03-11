#include "MetalPipeline.hpp"
#include "MetalCommandContext.hpp"
#include "MetalShaderModule.hpp"

namespace eg::graphics_api::mtl
{
ConcurrentObjectPool<Pipeline> pipelinePool;

void DestroyPipeline(PipelineHandle handle)
{
	Pipeline& mpipeline = UnwrapPipeline(handle);
	if (GraphicsPipeline* pipeline = std::get_if<GraphicsPipeline>(&mpipeline.variant))
		pipeline->pso->release();
	if (ComputePipeline* pipeline = std::get_if<ComputePipeline>(&mpipeline.variant))
		pipeline->pso->release();
	pipelinePool.Delete(&mpipeline);
}

void BindPipeline(CommandContextHandle ctx, PipelineHandle handle)
{
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(ctx);
	Pipeline& mpipeline = UnwrapPipeline(handle);

	std::visit([&](auto& pipeline) { pipeline.Bind(mcc); }, mpipeline.variant);
}

std::optional<uint32_t> StageBindingsTable::GetResourceMetalIndex(uint32_t set, uint32_t binding) const
{
	if (set >= bindingsMetalIndexTable.size() || binding >= bindingsMetalIndexTable[set].size())
		return std::nullopt;
	int index = bindingsMetalIndexTable[set][binding];
	if (index < 0)
		return std::nullopt;
	return static_cast<uint32_t>(index);
}

std::pair<MTL::Function*, std::shared_ptr<StageBindingsTable>> Pipeline::PrepareShaderModule(
	const ShaderStageInfo& stageInfo)
{
	if (stageInfo.shaderModule == nullptr)
		return { nullptr, nullptr };

	const ShaderModule& module = *reinterpret_cast<const ShaderModule*>(stageInfo.shaderModule);

	MTL::FunctionConstantValues* constantValues = MTL::FunctionConstantValues::alloc()->init();

	static const uint32_t METAL_API_CONSTANT = 2;
	constantValues->setConstantValue(&METAL_API_CONSTANT, MTL::DataTypeUInt, 500);

	for (const SpecializationConstantEntry& specConstant : stageInfo.specConstants)
	{
		auto specConstIt = std::lower_bound(
			module.specializationConstants.begin(), module.specializationConstants.end(), specConstant.constantID);

		if (specConstIt != module.specializationConstants.end() && specConstIt->constantID == specConstant.constantID)
		{
			const void* valuePtr =
				std::visit([](const auto& value) -> const void* { return &value; }, specConstant.value);
			constantValues->setConstantValue(valuePtr, specConstIt->dataType, specConstant.constantID);
		}
	}

	NS::Error* error = nullptr;
	MTL::Function* function =
		module.mtlLibrary->newFunction(NS::String::string("main0", NS::UTF8StringEncoding), constantValues, &error);

	if (function == nullptr)
	{
		EG_PANIC("Error creating shader function: " << error->localizedDescription()->utf8String());
	}

	constantValues->release();

	return { function, module.bindingsTable };
}
} // namespace eg::graphics_api::mtl
