#ifndef EG_NO_VULKAN
#include "../../Alloc/ObjectPool.hpp"
#include "../../Assert.hpp"
#include "Common.hpp"
#include "Pipeline.hpp"
#include "RenderPasses.hpp"
#include "ShaderModule.hpp"
#include "Translation.hpp"

#include <algorithm>
#include <spirv_cross.hpp>

namespace eg::graphics_api::vk
{
struct GraphicsPipeline : AbstractPipeline
{
	VkPipeline pipeline;

	bool enableScissorTest = false;
	bool enableDynamicCullMode = false;
	bool enableDynamicPolygonMode = false;

	VkCullModeFlags staticCullMode{};

	void Free() override;

	void Bind(CommandContextHandle cc) override;
};

static ConcurrentObjectPool<GraphicsPipeline> gfxPipelinesPool;

void GraphicsPipeline::Free()
{
	vkDestroyPipeline(ctx.device, pipeline, nullptr);
	gfxPipelinesPool.Delete(this);
}

inline void TranslateStencilState(const StencilState& in, VkStencilOpState& out)
{
	out.failOp = TranslateStencilOp(in.failOp);
	out.passOp = TranslateStencilOp(in.passOp);
	out.depthFailOp = TranslateStencilOp(in.depthFailOp);
	out.compareOp = TranslateCompareOp(in.compareOp);
	out.compareMask = in.compareMask;
	out.writeMask = in.writeMask;
	out.reference = in.reference;
}

static const VkViewport g_dummyViewport = { 0, 0, 0, 1, 0, 1 };
static const VkRect2D g_dummyScissor = { { 0, 0 }, { 1, 1 } };
static const VkPipelineViewportStateCreateInfo g_viewportStateCI = {
	/* sType         */ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
	/* pNext         */ nullptr,
	/* flags         */ 0,
	/* viewportCount */ 1,
	/* pViewports    */ &g_dummyViewport,
	/* scissorCount  */ 1,
	/* pScissors     */ &g_dummyScissor
};

PipelineHandle CreateGraphicsPipeline(const GraphicsPipelineCreateInfo& createInfo)
{
	GraphicsPipeline* pipeline = gfxPipelinesPool.New();
	pipeline->refCount = 1;
	pipeline->bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	uint32_t numStages = 0;
	std::array<ShaderModule*, 5> shaderModules{};
	std::array<VkPipelineShaderStageCreateInfo, 5> shaderStageCI;

	VkVertexInputBindingDescription vertexBindings[MAX_VERTEX_BINDINGS];
	VkVertexInputAttributeDescription vertexAttribs[MAX_VERTEX_ATTRIBUTES];
	VkPipelineColorBlendAttachmentState blendStates[MAX_COLOR_ATTACHMENTS];

	std::vector<VkDescriptorSetLayoutBinding> bindings[MAX_DESCRIPTOR_SETS];
	uint32_t numPushConstantBytes = 0;
	pipeline->pushConstantStages = 0;

	auto MaybeAddStage = [&](const ShaderStageInfo& stageInfo, VkShaderStageFlagBits stageFlags)
	{
		if (stageInfo.shaderModule == nullptr)
			return;

		ShaderModule* module = UnwrapShaderModule(stageInfo.shaderModule);
		module->ref++;

		InitShaderStageCreateInfo(shaderStageCI[numStages], pipeline->linearAllocator, stageInfo, stageFlags);
		shaderModules[numStages++] = module;

		// Adds bindings from this stage
		for (uint32_t set = 0; set < MAX_DESCRIPTOR_SETS; set++)
		{
			for (const VkDescriptorSetLayoutBinding& binding : module->bindings[set])
			{
				auto it = std::find_if(
					bindings[set].begin(), bindings[set].end(),
					[&](const VkDescriptorSetLayoutBinding& b) { return b.binding == binding.binding; });

				if (it != bindings[set].end())
				{
					if (it->descriptorType != binding.descriptorType)
						EG_PANIC("Descriptor type mismatch for binding " << binding.binding << " in set " << set);
					if (it->descriptorCount != binding.descriptorCount)
						EG_PANIC("Descriptor count mismatch for binding " << binding.binding << " in set " << set);
					it->stageFlags |= stageFlags;
				}
				else
				{
					bindings[set].push_back(binding);
				}
			}
		}

		if (module->pushConstantBytes > 0)
		{
			numPushConstantBytes = std::max(numPushConstantBytes, module->pushConstantBytes);
			pipeline->pushConstantStages |= stageFlags;
		}
	};

	MaybeAddStage(createInfo.vertexShader, VK_SHADER_STAGE_VERTEX_BIT);
	MaybeAddStage(createInfo.fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT);
	MaybeAddStage(createInfo.geometryShader, VK_SHADER_STAGE_GEOMETRY_BIT);
	MaybeAddStage(createInfo.tessControlShader, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
	MaybeAddStage(createInfo.tessEvaluationShader, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

	pipeline->InitPipelineLayout(bindings, createInfo.setBindModes, numPushConstantBytes);

	VkCullModeFlags staticCullMode = TranslateCullMode(createInfo.cullMode.value_or(eg::CullMode::None));
	pipeline->staticCullMode = staticCullMode;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCI = {
		/* sType                   */ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		/* pNext                   */ nullptr,
		/* flags                   */ 0,
		/* depthClampEnable        */ static_cast<VkBool32>(createInfo.enableDepthClamp),
		/* rasterizerDiscardEnable */ VK_FALSE,
		/* polygonMode             */ VK_POLYGON_MODE_FILL,
		/* cullMode                */ staticCullMode,
		/* frontFace               */ createInfo.frontFaceCCW ? VK_FRONT_FACE_COUNTER_CLOCKWISE
															  : VK_FRONT_FACE_CLOCKWISE,
		/* depthBiasEnable         */ VK_FALSE,
		/* depthBiasConstantFactor */ 0,
		/* depthBiasClamp          */ 0,
		/* depthBiasSlopeFactor    */ 0,
		/* lineWidth               */ 1.0f
	};

	if (ctx.deviceFeatures.wideLines)
	{
		rasterizationStateCI.lineWidth =
			glm::clamp(createInfo.lineWidth, ctx.deviceLimits.lineWidthRange[0], ctx.deviceLimits.lineWidthRange[1]);
	}

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = {
		/* sType                 */ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		/* pNext                 */ nullptr,
		/* flags                 */ 0,
		/* depthTestEnable       */ static_cast<VkBool32>(createInfo.enableDepthTest),
		/* depthWriteEnable      */ static_cast<VkBool32>(createInfo.enableDepthWrite),
		/* depthCompareOp        */ TranslateCompareOp(createInfo.depthCompare),
		/* depthBoundsTestEnable */ VK_FALSE,
		/* stencilTestEnable     */ static_cast<VkBool32>(createInfo.enableStencilTest),
		/* front                 */ {},
		/* back                  */ {},
		/* minDepthBounds        */ 0,
		/* maxDepthBounds        */ 0
	};

	if (createInfo.enableStencilTest)
	{
		TranslateStencilState(createInfo.backStencilState, depthStencilStateCI.back);
		TranslateStencilState(createInfo.frontStencilState, depthStencilStateCI.front);
	}

	VkPipelineColorBlendStateCreateInfo colorBlendStateCI = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pAttachments = blendStates,
	};
	std::copy_n(createInfo.blendConstants, 4, colorBlendStateCI.blendConstants);

	// Initializes attachment blend states and color attachment information for the render pass description.
	for (uint32_t i = 0; i < createInfo.numColorAttachments; i++)
	{
		const BlendState& blendState = createInfo.blendStates[i];

		colorBlendStateCI.attachmentCount = i + 1;

		blendStates[i].blendEnable = static_cast<VkBool32>(blendState.enabled);
		blendStates[i].colorBlendOp = TranslateBlendFunc(blendState.colorFunc);
		blendStates[i].alphaBlendOp = TranslateBlendFunc(blendState.alphaFunc);
		blendStates[i].srcColorBlendFactor = TranslateBlendFactor(blendState.srcColorFactor);
		blendStates[i].dstColorBlendFactor = TranslateBlendFactor(blendState.dstColorFactor);
		blendStates[i].srcAlphaBlendFactor = TranslateBlendFactor(blendState.srcAlphaFactor);
		blendStates[i].dstAlphaBlendFactor = TranslateBlendFactor(blendState.dstAlphaFactor);
		blendStates[i].colorWriteMask = static_cast<VkColorComponentFlags>(blendState.colorWriteMask);
	}

	// Translates vertex bindings
	uint32_t numVertexBindings = 0;
	for (uint32_t b = 0; b < MAX_VERTEX_BINDINGS; b++)
	{
		if (createInfo.vertexBindings[b].stride == UINT32_MAX)
			continue;

		vertexBindings[numVertexBindings].binding = b;
		vertexBindings[numVertexBindings].stride = createInfo.vertexBindings[b].stride;

		if (createInfo.vertexBindings[b].inputRate == InputRate::Vertex)
			vertexBindings[numVertexBindings].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		else
			vertexBindings[numVertexBindings].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

		numVertexBindings++;
	}

	// Translates vertex attributes
	uint32_t numVertexAttribs = 0;
	for (uint32_t a = 0; a < MAX_VERTEX_ATTRIBUTES; a++)
	{
		const VertexAttribute& attribIn = createInfo.vertexAttributes[a];
		if (attribIn.binding == UINT32_MAX)
			continue;

		vertexAttribs[numVertexAttribs].binding = attribIn.binding;
		vertexAttribs[numVertexAttribs].offset = attribIn.offset;
		vertexAttribs[numVertexAttribs].location = a;
		vertexAttribs[numVertexAttribs].format = TranslateFormat(attribIn.format);
		numVertexAttribs++;
	}

	VkPipelineVertexInputStateCreateInfo vertexInputStateCI = {
		/* sType                           */ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		/* pNext                           */ nullptr,
		/* flags                           */ 0,
		/* vertexBindingDescriptionCount   */ numVertexBindings,
		/* pVertexBindingDescriptions      */ vertexBindings,
		/* vertexAttributeDescriptionCount */ numVertexAttribs,
		/* pVertexAttributeDescriptions    */ vertexAttribs
	};

	if (createInfo.label != nullptr)
	{
		SetObjectName(
			reinterpret_cast<uint64_t>(pipeline->pipelineLayout), VK_OBJECT_TYPE_PIPELINE_LAYOUT, createInfo.label);
	}

	RenderPassDescription renderPassDescription;
	renderPassDescription.numResolveColorAttachments = 0;
	renderPassDescription.numColorAttachments = 0;
	renderPassDescription.depthAttachment.format = TranslateFormat(createInfo.depthAttachmentFormat);
	renderPassDescription.depthAttachment.samples = createInfo.sampleCount;
	renderPassDescription.depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	for (uint32_t i = 0; i < createInfo.numColorAttachments; i++)
	{
		EG_ASSERT(createInfo.colorAttachmentFormats[i] != eg::Format::Undefined);
		renderPassDescription.colorAttachments[i].format = TranslateFormat(createInfo.colorAttachmentFormats[i]);
		renderPassDescription.colorAttachments[i].samples = createInfo.sampleCount;
		renderPassDescription.colorAttachments[i].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		renderPassDescription.numColorAttachments++;
	}

	VkPipelineInputAssemblyStateCreateInfo iaState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };

	switch (createInfo.topology)
	{
	case Topology::TriangleList: iaState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
	case Topology::TriangleStrip: iaState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
	case Topology::LineList: iaState.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
	case Topology::LineStrip: iaState.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; break;
	case Topology::Points: iaState.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
	case Topology::Patches: iaState.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; break;
	default: EG_UNREACHABLE break;
	}

	const VkPipelineMultisampleStateCreateInfo multisampleState = {
		/* sType                 */ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		/* pNext                 */ nullptr,
		/* flags                 */ 0,
		/* rasterizationSamples  */ static_cast<VkSampleCountFlagBits>(createInfo.sampleCount),
		/* sampleShadingEnable   */ createInfo.enableSampleShading,
		/* minSampleShading      */ createInfo.minSampleShading,
		/* pSampleMask           */ nullptr,
		/* alphaToCoverageEnable */ createInfo.enableAlphaToCoverage,
		/* alphaToOneEnable      */ createInfo.enableAlphaToOne
	};

	const VkPipelineTessellationStateCreateInfo tessState = {
		/* sType              */ VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
		/* pNext              */ nullptr,
		/* flags              */ 0,
		/* patchControlPoints */ createInfo.patchControlPoints
	};

	VkDynamicState dynamicState[10] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	uint32_t dynamicStateCount = 2;
	if (createInfo.dynamicStencilCompareMask)
		dynamicState[dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK;
	if (createInfo.dynamicStencilWriteMask)
		dynamicState[dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_WRITE_MASK;
	if (createInfo.dynamicStencilReference)
		dynamicState[dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;

	if (!createInfo.cullMode.has_value())
	{
		dynamicState[dynamicStateCount++] = VK_DYNAMIC_STATE_CULL_MODE;
		pipeline->enableDynamicCullMode = true;
	}

	if (createInfo.enableWireframeRasterization && ctx.hasDynamicStatePolygonMode)
	{
		dynamicState[dynamicStateCount++] = VK_DYNAMIC_STATE_POLYGON_MODE_EXT;
		pipeline->enableDynamicPolygonMode = true;
	}

	const VkPipelineDynamicStateCreateInfo dynamicStateCI = {
		/* sType             */ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		/* pNext             */ nullptr,
		/* flags             */ 0,
		/* dynamicStateCount */ dynamicStateCount,
		/* pDynamicStates    */ dynamicState
	};

	VkGraphicsPipelineCreateInfo vkCreateInfo = {
		/* sType               */ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		/* pNext               */ nullptr,
		/* flags               */ VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT,
		/* stageCount          */ numStages,
		/* pStages             */ shaderStageCI.data(),
		/* pVertexInputState   */ &vertexInputStateCI,
		/* pInputAssemblyState */ &iaState,
		/* pTessellationState  */ createInfo.patchControlPoints != 0 ? &tessState : nullptr,
		/* pViewportState      */ &g_viewportStateCI,
		/* pRasterizationState */ &rasterizationStateCI,
		/* pMultisampleState   */ &multisampleState,
		/* pDepthStencilState  */ &depthStencilStateCI,
		/* pColorBlendState    */ &colorBlendStateCI,
		/* pDynamicState       */ &dynamicStateCI,
		/* layout              */ pipeline->pipelineLayout,
		/* renderPass          */ GetRenderPass(renderPassDescription, true),
		/* subpass             */ 0,
		/* basePipelineHandle  */ VK_NULL_HANDLE,
		/* basePipelineIndex   */ -1
	};

	CheckRes(vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &vkCreateInfo, nullptr, &pipeline->pipeline));

	if (createInfo.label != nullptr)
	{
		SetObjectName(reinterpret_cast<uint64_t>(pipeline->pipeline), VK_OBJECT_TYPE_PIPELINE, createInfo.label);
	}

	return WrapPipeline(pipeline);
}

inline void CommitDynamicState(CommandContextHandle cc)
{
	CommandContextState& state = GetCtxState(cc);
	VkCommandBuffer cb = GetCB(cc);

	if (state.viewportOutOfDate)
	{
		const VkViewport viewport = {
			state.viewportX, state.viewportY + state.viewportH, state.viewportW, -state.viewportH, 0.0f, 1.0f
		};
		vkCmdSetViewport(cb, 0, 1, &viewport);
		state.viewportOutOfDate = false;
	}

	if (state.scissorOutOfDate)
	{
		vkCmdSetScissor(cb, 0, 1, &state.scissor);
		state.scissorOutOfDate = false;
	}

	if (state.cullModeOutOfDate && state.enableDynamicCullMode)
	{
		vkCmdSetCullModeEXT(cb, state.cullMode);
		state.cullModeOutOfDate = false;
	}

	if (state.polygonModeOutOfDate && state.enableDynamicPolygonMode)
	{
		vkCmdSetPolygonModeEXT(cb, state.polygonMode);
		state.polygonModeOutOfDate = false;
	}
}

void SetViewport(CommandContextHandle cc, float x, float y, float w, float h)
{
	CommandContextState& state = GetCtxState(cc);
	if (!FEqual(state.viewportX, x) || !FEqual(state.viewportY, y) || !FEqual(state.viewportW, w) ||
	    !FEqual(state.viewportH, h))
	{
		state.viewportX = x;
		state.viewportY = y;
		state.viewportW = w;
		state.viewportH = h;
		state.viewportOutOfDate = true;
	}
}

void SetScissor(CommandContextHandle cc, int x, int y, int w, int h)
{
	CommandContextState& state = GetCtxState(cc);
	if (state.scissor.offset.x != x || state.scissor.offset.y != y ||
	    static_cast<int>(state.scissor.extent.width) != w || static_cast<int>(state.scissor.extent.height) != h)
	{
		state.scissor.offset.x = std::max<int>(x, 0);
		state.scissor.offset.y = std::max<int>(state.framebufferH - (y + h), 0);
		state.scissor.extent.width = glm::clamp(w, 0, ToInt(state.framebufferW) - x);
		state.scissor.extent.height = glm::clamp(h, 0, ToInt(state.framebufferH) - state.scissor.offset.y);
		state.scissorOutOfDate = true;
	}
}

void SetStencilValue(CommandContextHandle cc, StencilValue kind, uint32_t val)
{
	VkStencilFaceFlags faceFlags = 0;
	if (static_cast<int>(kind) & 0b0100)
		faceFlags |= VK_STENCIL_FACE_FRONT_BIT;
	if (static_cast<int>(kind) & 0b1000)
		faceFlags |= VK_STENCIL_FACE_BACK_BIT;

	int type = static_cast<int>(kind) & 0b11;
	if (type == 0)
		vkCmdSetStencilCompareMask(GetCB(cc), faceFlags, val);
	else if (type == 1)
		vkCmdSetStencilWriteMask(GetCB(cc), faceFlags, val);
	else if (type == 2)
		vkCmdSetStencilReference(GetCB(cc), faceFlags, val);
}

void SetWireframe(CommandContextHandle cc, bool wireframe)
{
	CommandContextState& ctxState = GetCtxState(cc);
	VkPolygonMode polygonMode = wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
	if (polygonMode != ctxState.polygonMode)
	{
		ctxState.polygonModeOutOfDate = true;
		ctxState.polygonMode = polygonMode;
	}
}

void SetCullMode(CommandContextHandle cc, CullMode cullMode)
{
	CommandContextState& ctxState = GetCtxState(cc);
	VkCullModeFlags vkCullMode = TranslateCullMode(cullMode);
	if (vkCullMode != ctxState.cullMode)
	{
		ctxState.cullModeOutOfDate = true;
		ctxState.cullMode = ctxState.cullMode;
	}
}

void GraphicsPipeline::Bind(CommandContextHandle cc)
{
	VkCommandBuffer cb = GetCB(cc);
	vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	CommandContextState& ctxState = GetCtxState(cc);

	if (!enableScissorTest)
	{
		SetScissor(cc, 0, 0, ctxState.framebufferW, ctxState.framebufferH);
	}

	ctxState.enableDynamicCullMode = enableDynamicCullMode;
	ctxState.enableDynamicPolygonMode = enableDynamicPolygonMode;

	if (!enableDynamicPolygonMode && ctxState.polygonMode != VK_POLYGON_MODE_FILL)
	{
		ctxState.polygonMode = VK_POLYGON_MODE_FILL;
		ctxState.polygonModeOutOfDate = true;
	}

	if (!enableDynamicCullMode && ctxState.cullMode != staticCullMode)
	{
		ctxState.cullMode = staticCullMode;
		ctxState.cullModeOutOfDate = true;
	}
}

void Draw(
	CommandContextHandle cc, uint32_t firstVertex, uint32_t numVertices, uint32_t firstInstance, uint32_t numInstances)
{
	CommitDynamicState(cc);
	vkCmdDraw(GetCB(cc), numVertices, numInstances, firstVertex, firstInstance);
}

void DrawIndexed(
	CommandContextHandle cc, uint32_t firstIndex, uint32_t numIndices, uint32_t firstVertex, uint32_t firstInstance,
	uint32_t numInstances)
{
	CommitDynamicState(cc);
	vkCmdDrawIndexed(GetCB(cc), numIndices, numInstances, firstIndex, firstVertex, firstInstance);
}
} // namespace eg::graphics_api::vk

#endif
