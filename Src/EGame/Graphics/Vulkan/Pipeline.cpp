#include "Pipeline.hpp"
#include "DSLCache.hpp"

namespace eg::graphics_api::vk
{
	void DestroyPipeline(PipelineHandle handle)
	{
		UnwrapPipeline(handle)->UnRef();
	}
	
	void BindPipeline(CommandContextHandle cc, PipelineHandle handle)
	{
		AbstractPipeline* pipeline = UnwrapPipeline(handle);
		
		RefResource(cc, *pipeline);
		CommandContextState& ctxState = GetCtxState(cc);
		ctxState.pipeline = pipeline;
		
		pipeline->Bind(cc);
	}
	
	void PushConstants(CommandContextHandle cc, uint32_t offset, uint32_t range, const void* data)
	{
		AbstractPipeline* pipeline = GetCtxState(cc).pipeline;
		if (pipeline == nullptr)
		{
			Log(LogLevel::Error, "gfx", "No pipeline bound when updating push constants.");
			return;
		}
		
		vkCmdPushConstants(GetCB(cc), pipeline->pipelineLayout, pipeline->pushConstantStages, offset, range, data);
	}
	
	void AbstractPipeline::InitPipelineLayout(const std::vector<VkDescriptorSetLayoutBinding>* bindings,
		const BindMode* setBindModes, uint32_t pushConstantBytes)
	{
		//Gets descriptor set layouts for each descriptor set
		uint32_t numDescriptorSets = 0;
		VkDescriptorSetLayout setLayouts[MAX_DESCRIPTOR_SETS] = { };
		for (uint32_t i = 0; i < MAX_DESCRIPTOR_SETS; i++)
		{
			if (bindings[i].empty())
				continue;
			numDescriptorSets = i + 1;
			const size_t cacheIdx = GetCachedDSLIndex(bindings[i], setBindModes[i]);
			setsLayoutIndices[i] = cacheIdx;
			setLayouts[i] = GetDSLFromCache(cacheIdx).layout;
		}
		
		//Creates the pipeline layout
		VkPipelineLayoutCreateInfo layoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		layoutCreateInfo.pSetLayouts = setLayouts;
		layoutCreateInfo.setLayoutCount = numDescriptorSets;
		VkPushConstantRange pushConstantRange;
		if (pushConstantBytes > 0)
		{
			pushConstantRange = { pushConstantStages, 0, pushConstantBytes };
			layoutCreateInfo.pushConstantRangeCount = 1;
			layoutCreateInfo.pPushConstantRanges = &pushConstantRange;
		}
		CheckRes(vkCreatePipelineLayout(ctx.device, &layoutCreateInfo, nullptr, &pipelineLayout));
	}
	
	void InitShaderStageCreateInfo(VkPipelineShaderStageCreateInfo& createInfo, VkShaderModule module,
		VkShaderStageFlagBits stage)
	{
		createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = 0;
		createInfo.module = module;
		createInfo.pName = "main";
		createInfo.stage = stage;
		createInfo.pSpecializationInfo = nullptr;
	}
}