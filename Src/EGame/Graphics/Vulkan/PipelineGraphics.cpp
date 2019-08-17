#ifndef EG_NO_VULKAN
#include "Common.hpp"
#include "RenderPasses.hpp"
#include "ShaderModule.hpp"
#include "Framebuffer.hpp"
#include "DSLCache.hpp"
#include "Translation.hpp"
#include "Pipeline.hpp"
#include "../../Alloc/ObjectPool.hpp"

#include <spirv_cross.hpp>
#include <iomanip>

namespace eg::graphics_api::vk
{
	struct FramebufferPipeline
	{
		size_t framebufferHash;
		VkPipeline pipeline;
	};
	
	struct GraphicsPipeline : AbstractPipeline
	{
		ShaderModule* shaderModules[5];
		GraphicsPipeline* basePipeline;
		std::vector<FramebufferPipeline> pipelines;
		bool enableScissorTest;
		
		bool enableAlphaToCoverage;
		bool enableAlphaToOne;
		bool enableSampleShading;
		float minSampleShading;
		
		bool dynamicStencilCompareMask;
		bool dynamicStencilWriteMask;
		bool dynamicStencilReference;
		
		uint32_t numStages;
		VkPipelineShaderStageCreateInfo shaderStageCI[5];
		VkVertexInputBindingDescription vertexBindings[MAX_VERTEX_BINDINGS];
		VkVertexInputAttributeDescription vertexAttribs[MAX_VERTEX_ATTRIBUTES];
		VkPipelineVertexInputStateCreateInfo vertexInputStateCI;
		VkPrimitiveTopology topology;
		uint32_t patchControlPoints;
		VkPipelineRasterizationStateCreateInfo rasterizationStateCI;
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCI;
		VkPipelineColorBlendAttachmentState blendStates[8];
		VkPipelineColorBlendStateCreateInfo colorBlendStateCI;
		
		std::string label;
		
		void Free() override;
		
		void Bind(CommandContextHandle cc);
	};
	
	static ConcurrentObjectPool<GraphicsPipeline> gfxPipelinesPool;
	
	void GraphicsPipeline::Free()
	{
		if (basePipeline != nullptr)
		{
			basePipeline->UnRef();
		}
		else
		{
			vkDestroyPipelineLayout(ctx.device, pipelineLayout, nullptr);
		}
		
		for (ShaderModule* module : shaderModules)
		{
			if (module != nullptr)
				module->UnRef();
		}
		
		for (const FramebufferPipeline& pipeline : pipelines)
		{
			vkDestroyPipeline(ctx.device, pipeline.pipeline, nullptr);
		}
		
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
	
	PipelineHandle CreateGraphicsPipeline(const GraphicsPipelineCreateInfo& createInfo)
	{
		GraphicsPipeline* pipeline = gfxPipelinesPool.New();
		pipeline->refCount = 1;
		pipeline->enableScissorTest = createInfo.enableScissorTest;
		pipeline->bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		std::fill_n(pipeline->shaderModules, 5, nullptr);
		
		std::vector<VkDescriptorSetLayoutBinding> bindings[MAX_DESCRIPTOR_SETS];
		uint32_t numPushConstantBytes = 0;
		pipeline->pushConstantStages = 0;
		
		auto MaybeAddStage = [&] (const ShaderStageInfo& stageInfo, VkShaderStageFlagBits stageFlags)
		{
			if (stageInfo.shaderModule == nullptr)
				return;
			
			ShaderModule* module = UnwrapShaderModule(stageInfo.shaderModule);
			module->ref++;
			
			InitShaderStageCreateInfo(pipeline->shaderStageCI[pipeline->numStages], pipeline->linearAllocator,
				stageInfo, stageFlags);
			pipeline->shaderModules[pipeline->numStages++] = module;
			
			//Adds bindings from this stage
			for (uint32_t set = 0; set < MAX_DESCRIPTOR_SETS; set++)
			{
				for (const VkDescriptorSetLayoutBinding& binding : module->bindings[set])
				{
					auto it = std::find_if(bindings[set].begin(), bindings[set].end(),
						[&] (const VkDescriptorSetLayoutBinding& b) { return b.binding == binding.binding; });
					
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
		
		switch (createInfo.topology)
		{
		case Topology::TriangleList: pipeline->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
		case Topology::TriangleStrip: pipeline->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
		case Topology::TriangleFan: pipeline->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN; break;
		case Topology::LineList: pipeline->topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
		case Topology::LineStrip: pipeline->topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; break;
		case Topology::Points: pipeline->topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
		case Topology::Patches: pipeline->topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; break;
		default: EG_UNREACHABLE break;
		}
		
		VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
		if (createInfo.wireframe && ctx.deviceFeatures.fillModeNonSolid)
			polygonMode = VK_POLYGON_MODE_LINE;
		
		pipeline->rasterizationStateCI =
		{
			/* sType                   */ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			/* pNext                   */ nullptr,
			/* flags                   */ 0,
			/* depthClampEnable        */ static_cast<VkBool32>(createInfo.enableDepthClamp),
			/* rasterizerDiscardEnable */ VK_FALSE,
			/* polygonMode             */ polygonMode,
			/* cullMode                */ TranslateCullMode(createInfo.cullMode),
			/* frontFace               */ createInfo.frontFaceCCW ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE,
			/* depthBiasEnable         */ VK_FALSE,
			/* depthBiasConstantFactor */ 0,
			/* depthBiasClamp          */ 0,
			/* depthBiasSlopeFactor    */ 0,
			/* lineWidth               */ 1.0f
		};
		
		pipeline->depthStencilStateCI =
		{
			/* sType                 */ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			/* pNext                 */ nullptr,
			/* flags                 */ 0,
			/* depthTestEnable       */ static_cast<VkBool32>(createInfo.enableDepthTest),
			/* depthWriteEnable      */ static_cast<VkBool32>(createInfo.enableDepthWrite),
			/* depthCompareOp        */ TranslateCompareOp(createInfo.depthCompare),
			/* depthBoundsTestEnable */ VK_FALSE,
			/* stencilTestEnable     */ static_cast<VkBool32>(createInfo.enableStencilTest),
			/* front                 */ { },
			/* back                  */ { },
			/* minDepthBounds        */ 0,
			/* maxDepthBounds        */ 0
		};
		
		if (createInfo.enableStencilTest)
		{
			TranslateStencilState(createInfo.backStencilState, pipeline->depthStencilStateCI.back);
			TranslateStencilState(createInfo.frontStencilState, pipeline->depthStencilStateCI.front);
		}
		
		pipeline->enableAlphaToCoverage = createInfo.enableAlphaToCoverage;
		pipeline->enableAlphaToOne = createInfo.enableAlphaToOne;
		pipeline->enableSampleShading = createInfo.enableSampleShading;
		pipeline->minSampleShading = createInfo.minSampleShading;
		pipeline->dynamicStencilCompareMask = createInfo.dynamicStencilCompareMask;
		pipeline->dynamicStencilWriteMask = createInfo.dynamicStencilWriteMask;
		pipeline->dynamicStencilReference = createInfo.dynamicStencilReference;
		
		pipeline->colorBlendStateCI = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
		std::copy_n(createInfo.blendConstants, 4, pipeline->colorBlendStateCI.blendConstants);
		pipeline->colorBlendStateCI.pAttachments = pipeline->blendStates;
		
		//Initializes attachment blend states and color attachment information for the render pass description.
		for (uint32_t i = 0; i < createInfo.numColorAttachments; i++)
		{
			const BlendState& blendState = createInfo.blendStates[i];
			
			pipeline->colorBlendStateCI.attachmentCount = i + 1;
			
			pipeline->blendStates[i].blendEnable = static_cast<VkBool32>(blendState.enabled);
			pipeline->blendStates[i].colorBlendOp = TranslateBlendFunc(blendState.colorFunc);
			pipeline->blendStates[i].alphaBlendOp = TranslateBlendFunc(blendState.alphaFunc);
			pipeline->blendStates[i].srcColorBlendFactor = TranslateBlendFactor(blendState.srcColorFactor);
			pipeline->blendStates[i].dstColorBlendFactor = TranslateBlendFactor(blendState.dstColorFactor);
			pipeline->blendStates[i].srcAlphaBlendFactor = TranslateBlendFactor(blendState.srcAlphaFactor);
			pipeline->blendStates[i].dstAlphaBlendFactor = TranslateBlendFactor(blendState.dstAlphaFactor);
			pipeline->blendStates[i].colorWriteMask = (VkColorComponentFlags)blendState.colorWriteMask;
		}
		
		//Translates vertex bindings
		uint32_t numVertexBindings = 0;
		for (uint32_t b = 0; b < MAX_VERTEX_BINDINGS; b++)
		{
			if (createInfo.vertexBindings[b].stride == UINT32_MAX)
				continue;
			
			pipeline->vertexBindings[numVertexBindings].binding = b;
			pipeline->vertexBindings[numVertexBindings].stride = createInfo.vertexBindings[b].stride;
			
			if (createInfo.vertexBindings[b].inputRate == InputRate::Vertex)
				pipeline->vertexBindings[numVertexBindings].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			else
				pipeline->vertexBindings[numVertexBindings].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
			
			numVertexBindings++;
		}
		
		//Translates vertex attributes
		uint32_t numVertexAttribs = 0;
		for (uint32_t a = 0; a < MAX_VERTEX_ATTRIBUTES; a++)
		{
			const VertexAttribute& attribIn = createInfo.vertexAttributes[a];
			if (attribIn.binding == UINT32_MAX)
				continue;
			
			pipeline->vertexAttribs[numVertexAttribs].binding = attribIn.binding;
			pipeline->vertexAttribs[numVertexAttribs].offset = attribIn.offset;
			pipeline->vertexAttribs[numVertexAttribs].location = a;
			pipeline->vertexAttribs[numVertexAttribs].format = GetAttribFormat(attribIn.type, attribIn.components);
			numVertexAttribs++;
		}
		
		pipeline->vertexInputStateCI =
		{
			/* sType                           */ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			/* pNext                           */ nullptr,
			/* flags                           */ 0,
			/* vertexBindingDescriptionCount   */ numVertexBindings,
			/* pVertexBindingDescriptions      */ pipeline->vertexBindings,
			/* vertexAttributeDescriptionCount */ numVertexAttribs,
			/* pVertexAttributeDescriptions    */ pipeline->vertexAttribs
		};
		
		pipeline->patchControlPoints = createInfo.patchControlPoints;
		
		if (createInfo.label != nullptr)
		{
			SetObjectName(reinterpret_cast<uint64_t>(pipeline->pipelineLayout),
				VK_OBJECT_TYPE_PIPELINE_LAYOUT, createInfo.label);
			pipeline->label = createInfo.label;
		}
		
		return WrapPipeline(pipeline);
	}
	
	static const VkViewport g_dummyViewport = { 0, 0, 0, 1, 0, 1 };
	static const VkRect2D g_dummyScissor = { { 0, 0 }, { 1, 1 } };
	static const VkPipelineViewportStateCreateInfo g_viewportStateCI = 
	{
		/* sType         */ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		/* pNext         */ nullptr,
		/* flags         */ 0,
		/* viewportCount */ 1,
		/* pViewports    */ &g_dummyViewport,
		/* scissorCount  */ 1,
		/* pScissors     */ &g_dummyScissor
	};
	
	VkPipeline MaybeCreatePipelineFramebufferVariant(const FramebufferFormat& format, GraphicsPipeline& pipeline, bool warn)
	{
		auto it = std::lower_bound(pipeline.pipelines.begin(), pipeline.pipelines.end(), format.hash,
			[&] (const FramebufferPipeline& a, size_t b)
		{
			return a.framebufferHash < b;
		});
		
		if (it != pipeline.pipelines.end() && it->framebufferHash == format.hash)
			return it->pipeline;
		
		int64_t beginTime = NanoTime();
		
		RenderPassDescription renderPassDescription;
		renderPassDescription.numResolveColorAttachments = 0;
		renderPassDescription.numColorAttachments = 0;
		renderPassDescription.depthAttachment.format = format.depthStencilFormat;
		renderPassDescription.depthAttachment.samples = format.sampleCount;
		for (uint32_t i = 0; i < 8 && format.colorFormats[i] != VK_FORMAT_UNDEFINED; i++)
		{
			renderPassDescription.colorAttachments[i].format = format.colorFormats[i];
			renderPassDescription.colorAttachments[i].samples = format.sampleCount;
			renderPassDescription.numColorAttachments++;
		}
		
		VkPipelineInputAssemblyStateCreateInfo iaState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		iaState.topology = pipeline.topology;
		
		const VkPipelineMultisampleStateCreateInfo multisampleState =
		{
			/* sType                 */ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			/* pNext                 */ nullptr,
			/* flags                 */ 0,
			/* rasterizationSamples  */ (VkSampleCountFlagBits)format.sampleCount,
			/* sampleShadingEnable   */ pipeline.enableSampleShading,
			/* minSampleShading      */ pipeline.minSampleShading,
			/* pSampleMask           */ nullptr,
			/* alphaToCoverageEnable */ pipeline.enableAlphaToCoverage,
			/* alphaToOneEnable      */ pipeline.enableAlphaToOne
		};
		
		const VkPipelineTessellationStateCreateInfo tessState =
		{
			/* sType              */ VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
			/* pNext              */ nullptr,
			/* flags              */ 0,
			/* patchControlPoints */ pipeline.patchControlPoints
		};
		
		VkDynamicState dynamicState[5] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		uint32_t dynamicStateCount = 2;
		if (pipeline.dynamicStencilCompareMask)
			dynamicState[dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK;
		if (pipeline.dynamicStencilWriteMask)
			dynamicState[dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_WRITE_MASK;
		if (pipeline.dynamicStencilReference)
			dynamicState[dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;
		
		const VkPipelineDynamicStateCreateInfo dynamicStateCI =
		{
			/* sType             */ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			/* pNext             */ nullptr,
			/* flags             */ 0,
			/* dynamicStateCount */ dynamicStateCount,
			/* pDynamicStates    */ dynamicState
		};
		
		VkGraphicsPipelineCreateInfo vkCreateInfo =
		{
			/* sType               */ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			/* pNext               */ nullptr,
			/* flags               */ VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT,
			/* stageCount          */ pipeline.numStages,
			/* pStages             */ pipeline.shaderStageCI,
			/* pVertexInputState   */ &pipeline.vertexInputStateCI,
			/* pInputAssemblyState */ &iaState,
			/* pTessellationState  */ pipeline.patchControlPoints ? &tessState : nullptr,
			/* pViewportState      */ &g_viewportStateCI,
			/* pRasterizationState */ &pipeline.rasterizationStateCI,
			/* pMultisampleState   */ &multisampleState,
			/* pDepthStencilState  */ &pipeline.depthStencilStateCI,
			/* pColorBlendState    */ &pipeline.colorBlendStateCI,
			/* pDynamicState       */ &dynamicStateCI,
			/* layout              */ pipeline.pipelineLayout,
			/* renderPass          */ GetRenderPass(renderPassDescription, true),
			/* subpass             */ 0,
			/* basePipelineHandle  */ VK_NULL_HANDLE,
			/* basePipelineIndex   */ -1
		};
		
		if (!pipeline.pipelines.empty())
		{
			vkCreateInfo.flags |= VK_PIPELINE_CREATE_DERIVATIVE_BIT;
			vkCreateInfo.basePipelineHandle = pipeline.pipelines.front().pipeline;
		}
		
		FramebufferPipeline& fbPipeline = *pipeline.pipelines.emplace(it);
		fbPipeline.framebufferHash = format.hash;
		CheckRes(vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &vkCreateInfo, nullptr, &fbPipeline.pipeline));
		
		int64_t elapsed = NanoTime() - beginTime;
		
		if (warn)
		{
			std::ostringstream msgStream;
			msgStream << "Creating pipeline on demand stalled CPU for " <<
				std::setprecision(2) << (elapsed * 1E-6) << "ms.";
			
			if (!pipeline.label.empty())
				msgStream << " Label of affected pipeline: '" << pipeline.label << "'.";
			
			Log(LogLevel::Warning, "vk", "{0}", msgStream.str());
		}
		
		if (!pipeline.label.empty())
		{
			SetObjectName(reinterpret_cast<uint64_t>(fbPipeline.pipeline),
				VK_OBJECT_TYPE_PIPELINE, pipeline.label.c_str());
		}
		
		return fbPipeline.pipeline;
	}
	
	void PipelineFramebufferFormatHint(PipelineHandle handle, const FramebufferFormatHint& hint)
	{
		MaybeCreatePipelineFramebufferVariant(FramebufferFormat::FromHint(hint), 
			*static_cast<GraphicsPipeline*>(UnwrapPipeline(handle)), false);
	}
	
	inline void CommitDynamicState(CommandContextHandle cc)
	{
		CommandContextState& state = GetCtxState(cc);
		VkCommandBuffer cb = GetCB(cc);
		
		if (state.viewportOutOfDate)
		{
			const VkViewport viewport = { state.viewportX, state.viewportY + state.viewportH, state.viewportW, -state.viewportH, 0.0f, 1.0f };
			vkCmdSetViewport(cb, 0, 1, &viewport);
			state.viewportOutOfDate = false;
		}
		
		if (state.scissorOutOfDate)
		{
			vkCmdSetScissor(cb, 0, 1, &state.scissor);
			state.scissorOutOfDate = false;
		}
	}
	
	void SetViewport(CommandContextHandle cc, float x, float y, float w, float h)
	{
		CommandContextState& state = GetCtxState(cc);
		if (!FEqual(state.viewportX, x) || !FEqual(state.viewportY, y) ||
		    !FEqual(state.viewportW, w) || !FEqual(state.viewportH, h))
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
		    (int)state.scissor.extent.width != w || (int)state.scissor.extent.height != h)
		{
			state.scissor.offset.x = std::max<int>(x, 0);
			state.scissor.offset.y = std::max<int>(state.framebufferH - (y + h), 0);
			state.scissor.extent.width = glm::clamp(w, 0, (int)state.framebufferW - x);
			state.scissor.extent.height = glm::clamp(h, 0, (int)state.framebufferH - state.scissor.offset.y);
			state.scissorOutOfDate = true;
		}
	}
	
	void SetStencilValue(CommandContextHandle cc, StencilValue kind, uint32_t val)
	{
		VkStencilFaceFlags faceFlags = 0;
		if ((int)kind & 0b0100)
			faceFlags |= VK_STENCIL_FACE_FRONT_BIT;
		if ((int)kind & 0b1000)
			faceFlags |= VK_STENCIL_FACE_BACK_BIT;
		
		int type = (int)kind & 0b11;
		if (type == 0)
			vkCmdSetStencilCompareMask(GetCB(cc), faceFlags, val);
		else if (type == 1)
			vkCmdSetStencilWriteMask(GetCB(cc), faceFlags, val);
		else if (type == 2)
			vkCmdSetStencilReference(GetCB(cc), faceFlags, val);
	}
	
	void GraphicsPipeline::Bind(CommandContextHandle cc)
	{
		VkCommandBuffer cb = GetCB(cc);
		VkPipeline vkPipeline = MaybeCreatePipelineFramebufferVariant(currentFBFormat, *this, true);
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);
		
		if (!enableScissorTest)
		{
			const CommandContextState& ctxState = GetCtxState(cc);
			SetScissor(cc, 0, 0, ctxState.framebufferW, ctxState.framebufferH);
		}
	}
	
	void Draw(CommandContextHandle cc, uint32_t firstVertex, uint32_t numVertices,
		uint32_t firstInstance, uint32_t numInstances)
	{
		CommitDynamicState(cc);
		vkCmdDraw(GetCB(cc), numVertices, numInstances, firstVertex, firstInstance);
	}
	
	void DrawIndexed(CommandContextHandle cc, uint32_t firstIndex, uint32_t numIndices, uint32_t firstVertex, 
		uint32_t firstInstance, uint32_t numInstances)
	{
		CommitDynamicState(cc);
		vkCmdDrawIndexed(GetCB(cc), numIndices, numInstances, firstIndex, firstVertex, firstInstance);
	}
}

#endif
