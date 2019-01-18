#include "Common.hpp"
#include "RenderPasses.hpp"
#include "VulkanBuffer.hpp"
#include "VulkanTexture.hpp"
#include "../../Alloc/ObjectPool.hpp"

#include <spirv_cross.hpp>

namespace eg::graphics_api::vk
{
	struct Pipeline;
	
	struct DescriptorSetLayout
	{
		VkDescriptorSetLayout layout;
		std::vector<VkDescriptorSetLayoutBinding> bindings;
	};
	
	static std::vector<DescriptorSetLayout> cachedSetLayouts;
	
	static VkDescriptorSetLayout GetCachedDescriptorSet(std::vector<VkDescriptorSetLayoutBinding> bindings)
	{
		std::sort(bindings.begin(), bindings.end(),
			[&] (const VkDescriptorSetLayoutBinding& a, const VkDescriptorSetLayoutBinding& b)
		{
			return a.binding < b.binding;
		});
		
		for (const DescriptorSetLayout& setLayout : cachedSetLayouts)
		{
			bool eq = std::equal(setLayout.bindings.begin(), setLayout.bindings.end(), bindings.begin(), bindings.end(),
				[&] (const VkDescriptorSetLayoutBinding& a, const VkDescriptorSetLayoutBinding& b)
			{
				return a.binding == b.binding && a.stageFlags == b.stageFlags && a.descriptorType == b.descriptorType &&
				       a.descriptorCount == b.descriptorCount;
			});
			
			if (eq)
				return setLayout.layout;
		}
		
		VkDescriptorSetLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		createInfo.bindingCount = bindings.size();
		createInfo.pBindings = bindings.data();
		createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
		
		VkDescriptorSetLayout setLayout;
		CheckRes(vkCreateDescriptorSetLayout(ctx.device, &createInfo, nullptr, &setLayout));
		cachedSetLayouts.push_back({ setLayout, std::move(bindings) });
		
		return setLayout;
	}
	
	void DestroyCachedDescriptorSets()
	{
		for (const DescriptorSetLayout& setLayout : cachedSetLayouts)
		{
			vkDestroyDescriptorSetLayout(ctx.device, setLayout.layout, nullptr);
		}
		cachedSetLayouts.clear();
	}
	
	struct ShaderProgram
	{
		VkShaderModule fragmentShader;
		VkShaderModule vertexShader;
		VkShaderStageFlags pushConstantStages;
		VkPipelineLayout pipelineLayout;
		VkPipeline basePipeline;
		std::atomic_int refCount;
		
		void UnRef();
	};
	
	struct Pipeline : Resource
	{
		VkPipeline pipeline;
		ShaderProgram* program;
		bool enableScissorTest;
		
		void Free() override;
	};
	
	inline ShaderProgram* UnwrapShaderProgram(ShaderProgramHandle handle)
	{
		return reinterpret_cast<ShaderProgram*>(handle);
	}
	
	inline Pipeline* UnwrapPipeline(PipelineHandle handle)
	{
		return reinterpret_cast<Pipeline*>(handle);
	}
	
	static ConcurrentObjectPool<ShaderProgram> shaderProgramsPool;
	static ConcurrentObjectPool<Pipeline> pipelinesPool;
	
	void ShaderProgram::UnRef()
	{
		if (--refCount <= 0)
		{
			if (basePipeline != VK_NULL_HANDLE)
				vkDestroyPipeline(ctx.device, basePipeline, nullptr);
			
			vkDestroyPipelineLayout(ctx.device, pipelineLayout, nullptr);
			if (vertexShader != VK_NULL_HANDLE)
				vkDestroyShaderModule(ctx.device, vertexShader, nullptr);
			if (fragmentShader != VK_NULL_HANDLE)
				vkDestroyShaderModule(ctx.device, fragmentShader, nullptr);
			shaderProgramsPool.Delete(this);
		}
	}
	
	void Pipeline::Free()
	{
		//If this pipeline is set as the base pipeline, it will be deleted with the shader program.
		if (program->basePipeline != pipeline)
			vkDestroyPipeline(ctx.device, pipeline, nullptr);
		
		program->UnRef();
		pipelinesPool.Delete(this);
	}
	
	ShaderProgramHandle CreateShaderProgram(Span<const ShaderStageDesc> stages)
	{
		ShaderProgram* program = shaderProgramsPool.New();
		program->vertexShader = VK_NULL_HANDLE;
		program->fragmentShader = VK_NULL_HANDLE;
		program->refCount = 1;
		program->pushConstantStages = 0;
		program->basePipeline = VK_NULL_HANDLE;
		
		std::vector<VkDescriptorSetLayoutBinding> bindings[MAX_DESCRIPTOR_SETS];
		uint32_t numPushConstantBytes = 0;
		
		//Creates shader modules and extracts pipeline layout information
		for (const ShaderStageDesc& stageDesc : stages)
		{
			//Creates the shader module
			VkShaderModule module;
			VkShaderModuleCreateInfo moduleCreateInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
			moduleCreateInfo.codeSize = stageDesc.codeBytes;
			moduleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(stageDesc.code);
			CheckRes(vkCreateShaderModule(ctx.device, &moduleCreateInfo, nullptr, &module));
			
			VkShaderStageFlags stageFlags;
			switch (stageDesc.stage)
			{
			case ShaderStage::Vertex:
				program->vertexShader = module;
				stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
				break;
			case ShaderStage::Fragment:
				program->fragmentShader = module;
				stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
				break;
			}
			
			spirv_cross::Compiler spvCrossCompiler(moduleCreateInfo.pCode, moduleCreateInfo.codeSize / 4);
			
			//Processes shader resources
			auto ProcessResources = [&] (const std::vector<spirv_cross::Resource>& resources, VkDescriptorType type)
			{
				for (const spirv_cross::Resource& resource : resources)
				{
					const uint32_t set = spvCrossCompiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
					const uint32_t binding = spvCrossCompiler.get_decoration(resource.id, spv::DecorationBinding);
					const uint32_t count = 1;
					
					auto it = std::find_if(bindings[set].begin(), bindings[set].end(),
						[&] (const VkDescriptorSetLayoutBinding& b) { return b.binding == binding; });
					
					if (it != bindings[set].end())
					{
						if (it->descriptorType != type)
							EG_PANIC("Descriptor type mismatch for binding " << binding << " in set " << set);
						if (it->descriptorCount != count)
							EG_PANIC("Descriptor count mismatch for binding " << binding << " in set " << set);
						it->stageFlags |= stageFlags;
					}
					else
					{
						VkDescriptorSetLayoutBinding& bindingRef = bindings[set].emplace_back();
						bindingRef.binding = binding;
						bindingRef.stageFlags = stageFlags;
						bindingRef.descriptorType = type;
						bindingRef.descriptorCount = count;
						bindingRef.pImmutableSamplers = nullptr;
					}
				}
			};
			
			const spirv_cross::ShaderResources& resources = spvCrossCompiler.get_shader_resources();
			ProcessResources(resources.uniform_buffers, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
			ProcessResources(resources.storage_buffers, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
			ProcessResources(resources.sampled_images, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
			ProcessResources(resources.separate_images, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
			ProcessResources(resources.separate_samplers, VK_DESCRIPTOR_TYPE_SAMPLER);
			
			for (const spirv_cross::Resource& pcBlock : resources.push_constant_buffers)
			{
				for (const spirv_cross::BufferRange& range : spvCrossCompiler.get_active_buffer_ranges(pcBlock.id))
				{
					numPushConstantBytes = std::max<uint32_t>(numPushConstantBytes, range.offset + range.range);
					program->pushConstantStages |= stageFlags;
				}
			}
		}
		
		//Gets descriptor set layouts for each descriptor set
		uint32_t numDescriptorSets = 0;
		VkDescriptorSetLayout setLayouts[MAX_DESCRIPTOR_SETS] = { };
		for (uint32_t i = 0; i < MAX_DESCRIPTOR_SETS; i++)
		{
			if (bindings[i].empty())
				continue;
			numDescriptorSets = i + 1;
			setLayouts[i] = GetCachedDescriptorSet(bindings[i]);
		}
		
		VkPipelineLayoutCreateInfo layoutCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		layoutCreateInfo.pSetLayouts = setLayouts;
		layoutCreateInfo.setLayoutCount = numDescriptorSets;
		
		VkPushConstantRange pushConstantRange;
		if (numPushConstantBytes > 0)
		{
			pushConstantRange = { program->pushConstantStages, 0, numPushConstantBytes };
			layoutCreateInfo.pushConstantRangeCount = 1;
			layoutCreateInfo.pPushConstantRanges = &pushConstantRange;
		}
		
		vkCreatePipelineLayout(ctx.device, &layoutCreateInfo, nullptr, &program->pipelineLayout);
		
		return reinterpret_cast<ShaderProgramHandle>(program);
	}
	
	void DestroyShaderProgram(ShaderProgramHandle handle)
	{
		ShaderProgram* program = UnwrapShaderProgram(handle);
		program->UnRef();
	}
	
	static VkCompareOp TranslateCompareOp(CompareOp op)
	{
		switch (op)
		{
		case CompareOp::Never: return VK_COMPARE_OP_NEVER;
		case CompareOp::Less: return VK_COMPARE_OP_LESS;
		case CompareOp::Equal: return VK_COMPARE_OP_EQUAL;
		case CompareOp::LessOrEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
		case CompareOp::Greater: return VK_COMPARE_OP_GREATER;
		case CompareOp::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
		case CompareOp::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
		case CompareOp::Always: return VK_COMPARE_OP_ALWAYS;
		}
		EG_UNREACHABLE
	}
	
	static VkBlendOp TranslateBlendFunc(BlendFunc blendFunc)
	{
		switch (blendFunc)
		{
		case BlendFunc::Add: return VK_BLEND_OP_ADD;
		case BlendFunc::Subtract: return VK_BLEND_OP_SUBTRACT;
		case BlendFunc::ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
		case BlendFunc::Min: return VK_BLEND_OP_MIN;
		case BlendFunc::Max: return VK_BLEND_OP_MAX;
		}
		EG_UNREACHABLE
	}
	
	static VkBlendFactor TranslateBlendFactor(BlendFactor blendFactor)
	{
		switch (blendFactor)
		{
		case BlendFactor::Zero: return VK_BLEND_FACTOR_ZERO;
		case BlendFactor::One: return VK_BLEND_FACTOR_ONE;
		case BlendFactor::SrcColor: return VK_BLEND_FACTOR_SRC_COLOR;
		case BlendFactor::OneMinusSrcColor: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		case BlendFactor::DstColor: return VK_BLEND_FACTOR_DST_COLOR;
		case BlendFactor::OneMinusDstColor: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		case BlendFactor::SrcAlpha: return VK_BLEND_FACTOR_SRC_ALPHA;
		case BlendFactor::OneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		case BlendFactor::DstAlpha: return VK_BLEND_FACTOR_DST_ALPHA;
		case BlendFactor::OneMinusDstAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		}
		EG_UNREACHABLE
	}
	
	static VkFormat GetAttribFormat(DataType dataType, uint32_t components)
	{
		switch (dataType)
		{
		case DataType::Float32:
			switch (components)
			{
			case 1: return VK_FORMAT_R32_SFLOAT;
			case 2: return VK_FORMAT_R32G32_SFLOAT;
			case 3: return VK_FORMAT_R32G32B32_SFLOAT;
			case 4: return VK_FORMAT_R32G32B32A32_SFLOAT;
			}
			break;
		case DataType::UInt8Norm:
			switch (components)
			{
			case 1: return VK_FORMAT_R8_UNORM;
			case 2: return VK_FORMAT_R8G8_UNORM;
			case 3: return VK_FORMAT_R8G8B8_UNORM;
			case 4: return VK_FORMAT_R8G8B8A8_UNORM;
			}
			break;
		case DataType::UInt16Norm:
			switch (components)
			{
			case 1: return VK_FORMAT_R16_UNORM;
			case 2: return VK_FORMAT_R16G16_UNORM;
			case 3: return VK_FORMAT_R16G16B16_UNORM;
			case 4: return VK_FORMAT_R16G16B16A16_UNORM;
			}
			break;
		case DataType::SInt8Norm:
			switch (components)
			{
			case 1: return VK_FORMAT_R8_SNORM;
			case 2: return VK_FORMAT_R8G8_SNORM;
			case 3: return VK_FORMAT_R8G8B8_SNORM;
			case 4: return VK_FORMAT_R8G8B8A8_SNORM;
			}
			break;
		case DataType::SInt16Norm:
			switch (components)
			{
			case 1: return VK_FORMAT_R16_SNORM;
			case 2: return VK_FORMAT_R16G16_SNORM;
			case 3: return VK_FORMAT_R16G16B16_SNORM;
			case 4: return VK_FORMAT_R16G16B16A16_SNORM;
			}
			break;
		case DataType::UInt8:
			switch (components)
			{
			case 1: return VK_FORMAT_R8_UINT;
			case 2: return VK_FORMAT_R8G8_UINT;
			case 3: return VK_FORMAT_R8G8B8_UINT;
			case 4: return VK_FORMAT_R8G8B8A8_UINT;
			}
			break;
		case DataType::UInt16:
			switch (components)
			{
			case 1: return VK_FORMAT_R16_UINT;
			case 2: return VK_FORMAT_R16G16_UINT;
			case 3: return VK_FORMAT_R16G16B16_UINT;
			case 4: return VK_FORMAT_R16G16B16A16_UINT;
			}
			break;
		case DataType::UInt32:
			switch (components)
			{
			case 1: return VK_FORMAT_R32_UINT;
			case 2: return VK_FORMAT_R32G32_UINT;
			case 3: return VK_FORMAT_R32G32B32_UINT;
			case 4: return VK_FORMAT_R32G32B32A32_UINT;
			}
			break;
		case DataType::SInt8:
			switch (components)
			{
			case 1: return VK_FORMAT_R8_SINT;
			case 2: return VK_FORMAT_R8G8_SINT;
			case 3: return VK_FORMAT_R8G8B8_SINT;
			case 4: return VK_FORMAT_R8G8B8A8_SINT;
			}
			break;
		case DataType::SInt16:
			switch (components)
			{
			case 1: return VK_FORMAT_R16_SINT;
			case 2: return VK_FORMAT_R16G16_SINT;
			case 3: return VK_FORMAT_R16G16B16_SINT;
			case 4: return VK_FORMAT_R16G16B16A16_SINT;
			}
			break;
		case DataType::SInt32:
			switch (components)
			{
			case 1: return VK_FORMAT_R32_SINT;
			case 2: return VK_FORMAT_R32G32_SINT;
			case 3: return VK_FORMAT_R32G32B32_SINT;
			case 4: return VK_FORMAT_R32G32B32A32_SINT;
			}
			break;
		}
		EG_PANIC("Invalid vertex attribute format");
	}
	
	static VkCullModeFlags TranslateCullMode(CullMode mode)
	{
		switch (mode)
		{
		case CullMode::None: return VK_CULL_MODE_NONE;
		case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
		case CullMode::Back: return VK_CULL_MODE_BACK_BIT;
		}
		EG_UNREACHABLE
	}
	
	PipelineHandle CreatePipeline(ShaderProgramHandle programHandle, const FixedFuncState& fixedFuncState)
	{
		ShaderProgram* program = UnwrapShaderProgram(programHandle);
		program->refCount++;
		
		Pipeline* pipeline = pipelinesPool.New();
		pipeline->refCount = 1;
		pipeline->program = UnwrapShaderProgram(programHandle);
		pipeline->enableScissorTest = fixedFuncState.enableScissorTest;
		
		VkPipelineShaderStageCreateInfo stageCreateInfos[2];
		uint32_t numStages = 0;
		
		auto MaybeAddStage = [&] (VkShaderModule shaderModule, VkShaderStageFlagBits stageFlags)
		{
			if (shaderModule)
			{
				stageCreateInfos[numStages].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				stageCreateInfos[numStages].pNext = nullptr;
				stageCreateInfos[numStages].flags = 0;
				stageCreateInfos[numStages].module = shaderModule;
				stageCreateInfos[numStages].pName = "main";
				stageCreateInfos[numStages].stage = stageFlags;
				stageCreateInfos[numStages].pSpecializationInfo = nullptr;
				numStages++;
			}
		};
		
		MaybeAddStage(program->vertexShader, VK_SHADER_STAGE_VERTEX_BIT);
		MaybeAddStage(program->fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT);
		
		VkPipelineInputAssemblyStateCreateInfo iaState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		switch (fixedFuncState.topology)
		{
		case Topology::TriangleList: iaState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
		case Topology::TriangleStrip: iaState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
		case Topology::TriangleFan: iaState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN; break;
		case Topology::LineList: iaState.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
		case Topology::LineStrip: iaState.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; break;
		case Topology::Points: iaState.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
		}
		
		const VkViewport dummyViewport = { 0, 0, 0, 1, 0, 1 };
		const VkRect2D dummyScissor = { { 0, 0 }, { 1, 1 } };
		VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
		viewportState.viewportCount = 1;
		viewportState.pViewports = &dummyViewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &dummyScissor;
		
		VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
		if (fixedFuncState.wireframe && ctx.deviceFeatures.fillModeNonSolid)
			polygonMode = VK_POLYGON_MODE_LINE;
		
		const VkPipelineRasterizationStateCreateInfo rasterizationState =
		{
			/* sType                   */ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			/* pNext                   */ nullptr,
			/* flags                   */ 0,
			/* depthClampEnable        */ static_cast<VkBool32>(fixedFuncState.enableDepthClamp),
			/* rasterizerDiscardEnable */ VK_FALSE,
			/* polygonMode             */ polygonMode,
			/* cullMode                */ TranslateCullMode(fixedFuncState.cullMode),
			/* frontFace               */ fixedFuncState.frontFaceCCW ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE,
			/* depthBiasEnable         */ VK_FALSE,
			/* depthBiasConstantFactor */ 0,
			/* depthBiasClamp          */ 0,
			/* depthBiasSlopeFactor    */ 0,
			/* lineWidth               */ 1.0f
		};
		
		const VkPipelineMultisampleStateCreateInfo multisampleState =
		{
			/* sType                 */ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			/* pNext                 */ nullptr,
			/* flags                 */ 0,
			/* rasterizationSamples  */ VK_SAMPLE_COUNT_1_BIT,
			/* sampleShadingEnable   */ VK_FALSE,
			/* minSampleShading      */ 0.0f,
			/* pSampleMask           */ nullptr,
			/* alphaToCoverageEnable */ VK_FALSE,
			/* alphaToOneEnable      */ VK_FALSE
		};
		
		VkPipelineDepthStencilStateCreateInfo depthStencilState =
		{
			/* sType                 */ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			/* pNext                 */ nullptr,
			/* flags                 */ 0,
			/* depthTestEnable       */ static_cast<VkBool32>(fixedFuncState.enableDepthTest),
			/* depthWriteEnable      */ static_cast<VkBool32>(fixedFuncState.enableDepthWrite),
			/* depthCompareOp        */ TranslateCompareOp(fixedFuncState.depthCompare),
			/* depthBoundsTestEnable */ VK_FALSE,
			/* stencilTestEnable     */ VK_FALSE,
			/* front                 */ { },
			/* back                  */ { },
			/* minDepthBounds        */ 0,
			/* maxDepthBounds        */ 0
		};
		
		VkPipelineColorBlendAttachmentState blendStates[8];
		
		VkPipelineColorBlendStateCreateInfo colorBlendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
		colorBlendState.attachmentCount = 0;
		colorBlendState.pAttachments = blendStates;
		
		RenderPassDescription renderPassDescription;
		renderPassDescription.depthAttachment.format = TranslateFormat(fixedFuncState.depthFormat);
		renderPassDescription.depthAttachment.samples = fixedFuncState.depthSamples;
		
		//Initializes attachment blend states and color attachment information for the render pass description.
		for (uint32_t i = 0; i < 8; i++)
		{
			if (fixedFuncState.attachments[i].format == Format::Undefined)
				continue;
			
			renderPassDescription.colorAttachments[i].format = TranslateFormat(fixedFuncState.attachments[i].format);
			renderPassDescription.colorAttachments[i].samples = fixedFuncState.attachments[i].samples;
			
			const BlendState& blendState = fixedFuncState.attachments[i].blend;
			
			colorBlendState.attachmentCount = i + 1;
			
			blendStates[i].blendEnable = static_cast<VkBool32>(blendState.enabled);
			blendStates[i].colorBlendOp = TranslateBlendFunc(blendState.colorFunc);
			blendStates[i].alphaBlendOp = TranslateBlendFunc(blendState.alphaFunc);
			blendStates[i].srcColorBlendFactor = TranslateBlendFactor(blendState.srcColorFactor);
			blendStates[i].dstColorBlendFactor = TranslateBlendFactor(blendState.dstColorFactor);
			blendStates[i].srcAlphaBlendFactor = TranslateBlendFactor(blendState.srcAlphaFactor);
			blendStates[i].dstAlphaBlendFactor = TranslateBlendFactor(blendState.dstAlphaFactor);
			blendStates[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
				VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		}
		
		const VkDynamicState dynamicState[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo =
		{
			/* sType             */ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			/* pNext             */ nullptr,
			/* flags             */ 0,
			/* dynamicStateCount */ ArrayLen(dynamicState),
			/* pDynamicStates    */ dynamicState
		};
		
		//Translates vertex bindings
		uint32_t numVertexBindings = 0;
		VkVertexInputBindingDescription vertexBindings[MAX_VERTEX_BINDINGS];
		for (uint32_t b = 0; b < MAX_VERTEX_BINDINGS; b++)
		{
			if (fixedFuncState.vertexBindings[b].stride == UINT32_MAX)
				continue;
			
			vertexBindings[numVertexBindings].binding = b;
			vertexBindings[numVertexBindings].stride = fixedFuncState.vertexBindings[b].stride;
			
			if (fixedFuncState.vertexBindings[b].inputRate == InputRate::Vertex)
				vertexBindings[numVertexBindings].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			else
				vertexBindings[numVertexBindings].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
			
			numVertexBindings++;
		}
		
		//Translates vertex attributes
		uint32_t numVertexAttribs = 0;
		VkVertexInputAttributeDescription vertexAttribs[MAX_VERTEX_ATTRIBUTES];
		for (uint32_t a = 0; a < MAX_VERTEX_ATTRIBUTES; a++)
		{
			const VertexAttribute& attribIn = fixedFuncState.vertexAttributes[a];
			if (attribIn.binding == UINT32_MAX)
				continue;
			
			vertexAttribs[numVertexAttribs].binding = attribIn.binding;
			vertexAttribs[numVertexAttribs].offset = attribIn.offset;
			vertexAttribs[numVertexAttribs].location = a;
			vertexAttribs[numVertexAttribs].format = GetAttribFormat(attribIn.type, attribIn.components);
			numVertexAttribs++;
		}
		
		const VkPipelineVertexInputStateCreateInfo vertexInputState =
		{
			/* sType                           */ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			/* pNext                           */ nullptr,
			/* flags                           */ 0,
			/* vertexBindingDescriptionCount   */ numVertexBindings,
			/* pVertexBindingDescriptions      */ vertexBindings,
			/* vertexAttributeDescriptionCount */ numVertexAttribs,
			/* pVertexAttributeDescriptions    */ vertexAttribs
		};
		
		VkGraphicsPipelineCreateInfo createInfo =
		{
			/* sType               */ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			/* pNext               */ nullptr,
			/* flags               */ 0,
			/* stageCount          */ numStages,
			/* pStages             */ stageCreateInfos,
			/* pVertexInputState   */ &vertexInputState,
			/* pInputAssemblyState */ &iaState,
			/* pTessellationState  */ nullptr,
			/* pViewportState      */ &viewportState,
			/* pRasterizationState */ &rasterizationState,
			/* pMultisampleState   */ &multisampleState,
			/* pDepthStencilState  */ &depthStencilState,
			/* pColorBlendState    */ &colorBlendState,
			/* pDynamicState       */ &dynamicStateCreateInfo,
			/* layout              */ program->pipelineLayout,
			/* renderPass          */ GetRenderPass(renderPassDescription, true),
			/* subpass             */ 0,
			/* basePipelineHandle  */ VK_NULL_HANDLE,
			/* basePipelineIndex   */ -1
		};
		
		if (program->basePipeline == VK_NULL_HANDLE)
		{
			createInfo.flags |= VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
		}
		else
		{
			createInfo.flags |= VK_PIPELINE_CREATE_DERIVATIVE_BIT;
			createInfo.basePipelineHandle = program->basePipeline;
		}
		
		CheckRes(vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline->pipeline));
		
		if (program->basePipeline == VK_NULL_HANDLE)
		{
			program->basePipeline = pipeline->pipeline;
		}
		
		return reinterpret_cast<PipelineHandle>(pipeline);
	}
	
	void DestroyPipeline(PipelineHandle handle)
	{
		UnwrapPipeline(handle)->UnRef();
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
			state.scissor.offset.x = x;
			state.scissor.offset.y = state.framebufferH - (y + h);
			state.scissor.extent.width = w;
			state.scissor.extent.height = h;
			state.scissorOutOfDate = true;
		}
	}
	
	void BindPipeline(CommandContextHandle cc, PipelineHandle handle)
	{
		Pipeline* pipeline = UnwrapPipeline(handle);
		RefResource(cc, *pipeline);
		
		CommandContextState& ctxState = GetCtxState(cc);
		ctxState.pipeline = pipeline;
		
		VkCommandBuffer cb = GetCB(cc);
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
		
		if (!pipeline->enableScissorTest)
		{
			SetScissor(cc, 0, 0, ctxState.framebufferW, ctxState.framebufferH);
		}
	}
	
	void BindUniformBuffer(CommandContextHandle cc, BufferHandle bufferHandle, uint32_t binding, uint64_t offset, uint64_t range)
	{
		Buffer* buffer = UnwrapBuffer(bufferHandle);
		RefResource(cc, *buffer);
		
		buffer->CheckUsageState(BufferUsage::UniformBuffer, "binding as a uniform buffer");
		
		Pipeline* pipeline = GetCtxState(cc).pipeline;
		
		VkDescriptorBufferInfo bufferInfo;
		bufferInfo.buffer = buffer->buffer;
		bufferInfo.offset = offset;
		bufferInfo.range = range;
		
		VkWriteDescriptorSet writeDS = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		writeDS.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDS.dstBinding = binding;
		writeDS.dstSet = 0;
		writeDS.descriptorCount = 1;
		writeDS.pBufferInfo = &bufferInfo;
		
		vkCmdPushDescriptorSetKHR(GetCB(cc), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->program->pipelineLayout,
			0, 1, &writeDS);
	}
	
	void BindTexture(CommandContextHandle cc, TextureHandle textureHandle, SamplerHandle samplerHandle, uint32_t binding)
	{
		Texture* texture = UnwrapTexture(textureHandle);
		RefResource(cc, *texture);
		
		if (texture->autoBarrier && texture->currentUsage != TextureUsage::ShaderSample)
		{
			EG_PANIC("Texture passed to BindTexture not in the correct usage state, did you forget to call UsageHint?");
		}
		
		VkSampler sampler = reinterpret_cast<VkSampler>(samplerHandle);
		if (sampler == VK_NULL_HANDLE)
		{
			if (texture->defaultSampler == VK_NULL_HANDLE)
			{
				EG_PANIC("Attempted to bind texture with no sampler specified.")
			}
			sampler = texture->defaultSampler;
		}
		
		Pipeline* pipeline = GetCtxState(cc).pipeline;
		
		VkDescriptorImageInfo imageInfo;
		imageInfo.imageView = texture->imageView;
		imageInfo.sampler = sampler;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		
		VkWriteDescriptorSet writeDS = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		writeDS.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeDS.dstBinding = binding;
		writeDS.dstSet = 0;
		writeDS.descriptorCount = 1;
		writeDS.pImageInfo = &imageInfo;
		
		vkCmdPushDescriptorSetKHR(GetCB(cc), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->program->pipelineLayout,
			0, 1, &writeDS);
	}
	
	void PushConstants(CommandContextHandle cc, uint32_t offset, uint32_t range, const void* data)
	{
		Pipeline* pipeline = GetCtxState(cc).pipeline;
		if (pipeline == nullptr)
		{
			Log(LogLevel::Error, "gfx", "No pipeline bound when updating push constants.");
			return;
		}
		
		vkCmdPushConstants(GetCB(cc), pipeline->program->pipelineLayout, pipeline->program->pushConstantStages,
			offset, range, data);
	}
	
	void BindVertexBuffer(CommandContextHandle cc, uint32_t binding, BufferHandle bufferHandle, uint32_t offset)
	{
		Buffer* buffer = UnwrapBuffer(bufferHandle);
		RefResource(cc, *buffer);
		
		buffer->CheckUsageState(BufferUsage::VertexBuffer, "binding as a vertex buffer");
		
		VkDeviceSize offsetDS = offset;
		vkCmdBindVertexBuffers(GetCB(cc), binding, 1, &buffer->buffer, &offsetDS);
	}
	
	void BindIndexBuffer(CommandContextHandle cc, IndexType type, BufferHandle bufferHandle, uint32_t offset)
	{
		Buffer* buffer = UnwrapBuffer(bufferHandle);
		RefResource(cc, *buffer);
		
		buffer->CheckUsageState(BufferUsage::IndexBuffer, "binding as an index buffer");
		
		const VkIndexType vkIndexType = type == IndexType::UInt32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
		vkCmdBindIndexBuffer(GetCB(cc), buffer->buffer, offset, vkIndexType);
	}
	
	void Draw(CommandContextHandle cc, uint32_t firstVertex, uint32_t numVertices, uint32_t numInstances)
	{
		CommitDynamicState(cc);
		vkCmdDraw(GetCB(cc), numVertices, numInstances, firstVertex, 0);
	}
	
	void DrawIndexed(CommandContextHandle cc, uint32_t firstIndex, uint32_t numIndices, uint32_t firstVertex, uint32_t numInstances)
	{
		CommitDynamicState(cc);
		vkCmdDrawIndexed(GetCB(cc), numIndices, numInstances, firstIndex, firstVertex, 0);
	}
}
