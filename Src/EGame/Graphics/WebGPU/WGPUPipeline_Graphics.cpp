#include "../Abstraction.hpp"
#include "../SpirvCrossUtils.hpp"
#include "EGame/String.hpp"
#include "WGPUCommandContext.hpp"
#include "WGPUDescriptorSet.hpp"
#include "WGPUPipeline.hpp"
#include "WGPUShaderModule.hpp"
#include "WGPUTranslation.hpp"

namespace eg::graphics_api::webgpu
{
static WGPUPrimitiveTopology TranslatePrimitiveTopology(Topology topology)
{
	switch (topology)
	{
	case Topology::TriangleList: return WGPUPrimitiveTopology_TriangleList;
	case Topology::TriangleStrip: return WGPUPrimitiveTopology_TriangleStrip;
	case Topology::LineList: return WGPUPrimitiveTopology_LineList;
	case Topology::LineStrip: return WGPUPrimitiveTopology_LineStrip;
	case Topology::Points: return WGPUPrimitiveTopology_PointList;
	case Topology::Patches: EG_PANIC("Unsupported topology: Patches");
	}
	EG_UNREACHABLE
}

static WGPUVertexStepMode TranslateInputRate(InputRate inputRate)
{
	switch (inputRate)
	{
	case InputRate::Vertex: return WGPUVertexStepMode_Vertex;
	case InputRate::Instance: return WGPUVertexStepMode_Instance;
	}
	EG_UNREACHABLE
}

static WGPUBlendFactor TranslateBlendFactor(BlendFactor factor)
{
	switch (factor)
	{
	case BlendFactor::Zero: return WGPUBlendFactor_Zero;
	case BlendFactor::One: return WGPUBlendFactor_One;
	case BlendFactor::SrcColor: return WGPUBlendFactor_Src;
	case BlendFactor::OneMinusSrcColor: return WGPUBlendFactor_OneMinusSrc;
	case BlendFactor::DstColor: return WGPUBlendFactor_Dst;
	case BlendFactor::OneMinusDstColor: return WGPUBlendFactor_OneMinusDst;
	case BlendFactor::SrcAlpha: return WGPUBlendFactor_SrcAlpha;
	case BlendFactor::OneMinusSrcAlpha: return WGPUBlendFactor_OneMinusSrcAlpha;
	case BlendFactor::DstAlpha: return WGPUBlendFactor_DstAlpha;
	case BlendFactor::OneMinusDstAlpha: return WGPUBlendFactor_OneMinusDstAlpha;
	case BlendFactor::ConstantColor: return WGPUBlendFactor_Constant;
	case BlendFactor::OneMinusConstantColor: return WGPUBlendFactor_OneMinusConstant;
	case BlendFactor::ConstantAlpha: return WGPUBlendFactor_Constant;
	case BlendFactor::OneMinusConstantAlpha: return WGPUBlendFactor_Constant;
	}
	EG_UNREACHABLE
}

static WGPUBlendOperation TranslateBlendFunc(BlendFunc blendFunc)
{
	switch (blendFunc)
	{
	case BlendFunc::Add: return WGPUBlendOperation_Add;
	case BlendFunc::Subtract: return WGPUBlendOperation_Subtract;
	case BlendFunc::ReverseSubtract: return WGPUBlendOperation_ReverseSubtract;
	case BlendFunc::Min: return WGPUBlendOperation_Min;
	case BlendFunc::Max: return WGPUBlendOperation_Max;
	}
	EG_UNREACHABLE
}

PipelineHandle CreateGraphicsPipeline(const GraphicsPipelineCreateInfo& createInfo)
{
	if (createInfo.dynamicDescriptorSetIndex.has_value())
	{
		std::string labelWithParen;
		if (createInfo.label)
			labelWithParen = Concat({ "(", createInfo.label, ") " });
		eg::Log(
			eg::LogLevel::Warning, "webgpu",
			"Pipeline{0} uses dynamic descriptor set, which is not supported in WebGPU", labelWithParen);
	}

	ShaderModule* vertexShader = reinterpret_cast<ShaderModule*>(createInfo.vertexShader.shaderModule);
	ShaderModule* fragmentShader = reinterpret_cast<ShaderModule*>(createInfo.fragmentShader.shaderModule);
	EG_ASSERT(vertexShader != nullptr);

	DescriptorSetBindings bindings = vertexShader->bindings;
	if (fragmentShader != nullptr)
		bindings.AppendFrom(fragmentShader->bindings);
	for (size_t set = 0; set < MAX_DESCRIPTOR_SETS; set++)
	{
		std::span<const eg::DescriptorSetBinding> forcedBindings = createInfo.descriptorSetBindings[set];
		if (!forcedBindings.empty())
			bindings.sets[set].assign(forcedBindings.begin(), forcedBindings.end());
	}
	bindings.SortByBinding();

	AbstractPipeline* pipeline = new AbstractPipeline(bindings, createInfo.label);

	// Processes vertex attributes
	std::array<std::vector<WGPUVertexAttribute>, MAX_VERTEX_BINDINGS> bindingsAttributes;
	for (uint32_t attrib = 0; attrib < MAX_VERTEX_ATTRIBUTES; attrib++)
	{
		if (createInfo.vertexAttributes[attrib].IsEnabled())
		{
			bindingsAttributes.at(createInfo.vertexAttributes[attrib].binding)
				.push_back(WGPUVertexAttribute{
					.format = TranslateVertexFormat(createInfo.vertexAttributes[attrib].format),
					.offset = createInfo.vertexAttributes[attrib].offset,
					.shaderLocation = attrib,
				});
		}
	}

	uint32_t maxEnabledBindingPlusOne = 0;
	for (uint32_t binding = 0; binding < MAX_VERTEX_BINDINGS; binding++)
	{
		if (createInfo.vertexBindings[binding].IsEnabled())
			maxEnabledBindingPlusOne = binding + 1;
	}

	// Processes vertex bindings
	std::vector<WGPUVertexBufferLayout> vertexBuffers(maxEnabledBindingPlusOne);
	for (uint32_t binding = 0; binding < maxEnabledBindingPlusOne; binding++)
	{
		if (createInfo.vertexBindings[binding].IsEnabled())
		{
			vertexBuffers[binding] = WGPUVertexBufferLayout{
				.arrayStride = createInfo.vertexBindings[binding].stride,
				.stepMode = TranslateInputRate(createInfo.vertexBindings[binding].inputRate),
				.attributeCount = bindingsAttributes[binding].size(),
				.attributes = bindingsAttributes[binding].data(),
			};
		}
		else
		{
			vertexBuffers[binding].stepMode = WGPUVertexStepMode_VertexBufferNotUsed;
		}
	}

	auto vertexShaderModule = vertexShader->GetSpecializedShaderModule(createInfo.vertexShader.specConstants);

	WGPURenderPipelineDescriptor pipelineDescriptor = {
		.label = createInfo.label,
		.layout = pipeline->pipelineLayout,
		.vertex =
			WGPUVertexState{
				.module = vertexShaderModule.get(),
				.entryPoint = "main",
				.bufferCount = vertexBuffers.size(),
				.buffers = vertexBuffers.data(),
			},
		.primitive =
			WGPUPrimitiveState{
				.topology = TranslatePrimitiveTopology(createInfo.topology),
				.frontFace = createInfo.frontFaceCCW ? WGPUFrontFace_CCW : WGPUFrontFace_CW,
				.cullMode = TranslateCullMode(createInfo.cullMode.value_or(CullMode::None)),
			},
		.multisample =
			WGPUMultisampleState{
				.count = createInfo.sampleCount,
				.mask = 0xFFFFFFFF,
				.alphaToCoverageEnabled = createInfo.enableAlphaToCoverage,
			},
	};

	if (createInfo.topology == Topology::TriangleStrip || createInfo.topology == Topology::LineStrip)
	{
		pipelineDescriptor.primitive.stripIndexFormat =
			createInfo.stripIndexType == IndexType::UInt32 ? WGPUIndexFormat_Uint32 : WGPUIndexFormat_Uint16;
	}

	std::unique_ptr<WGPUShaderModuleImpl, ShaderModuleOptDeleter> fragmentShaderModule;

	WGPUFragmentState fragmentState;
	WGPUColorTargetState colorTargetStates[MAX_COLOR_ATTACHMENTS];
	WGPUBlendState blendStates[MAX_COLOR_ATTACHMENTS];
	if (fragmentShader != nullptr)
	{
		for (uint32_t i = 0; i < createInfo.numColorAttachments; i++)
		{
			colorTargetStates[i] = WGPUColorTargetState{
				.format = TranslateTextureFormat(createInfo.colorAttachmentFormats[i]),
				.writeMask = static_cast<WGPUColorWriteMask>(createInfo.blendStates[i].colorWriteMask),
			};

			if (createInfo.blendStates[i].enabled)
			{
				blendStates[i] = WGPUBlendState{
					.color = {
						.operation = TranslateBlendFunc(createInfo.blendStates[i].colorFunc),
						.srcFactor = TranslateBlendFactor(createInfo.blendStates[i].srcColorFactor),
						.dstFactor = TranslateBlendFactor(createInfo.blendStates[i].dstColorFactor),
					},
					.alpha = {
						.operation = TranslateBlendFunc(createInfo.blendStates[i].alphaFunc),
						.srcFactor = TranslateBlendFactor(createInfo.blendStates[i].srcAlphaFactor),
						.dstFactor = TranslateBlendFactor(createInfo.blendStates[i].dstAlphaFactor),
					},
				};
				colorTargetStates[i].blend = &blendStates[i];
			}
		}

		fragmentShaderModule = fragmentShader->GetSpecializedShaderModule(createInfo.fragmentShader.specConstants);

		fragmentState = WGPUFragmentState{
			.module = fragmentShaderModule.get(),
			.entryPoint = "main",
			.targetCount = createInfo.numColorAttachments,
			.targets = colorTargetStates,
		};
		pipelineDescriptor.fragment = &fragmentState;
	}

	WGPUDepthStencilState depthStencilState;
	if (createInfo.depthAttachmentFormat != Format::Undefined &&
	    createInfo.depthAttachmentFormat != Format::DefaultDepthStencil)
	{
		static constexpr WGPUStencilFaceState STENCIL_DISABLED_STATE = {
			.compare = WGPUCompareFunction_Always,
			.failOp = WGPUStencilOperation_Keep,
			.depthFailOp = WGPUStencilOperation_Keep,
			.passOp = WGPUStencilOperation_Keep,
		};

		depthStencilState = WGPUDepthStencilState{
			.format = TranslateTextureFormat(createInfo.depthAttachmentFormat),
			.depthWriteEnabled = createInfo.enableDepthWrite,
			.depthCompare =
				createInfo.enableDepthTest ? TranslateCompareOp(createInfo.depthCompare) : WGPUCompareFunction_Always,
			.stencilFront = STENCIL_DISABLED_STATE,
			.stencilBack = STENCIL_DISABLED_STATE,
		};
		pipelineDescriptor.depthStencil = &depthStencilState;
	}

	GraphicsPipeline graphicsPipeline;
	graphicsPipeline.enableScissorTest = createInfo.enableScissorTest;

	if (createInfo.cullMode.has_value())
	{
		graphicsPipeline.pipeline = wgpuDeviceCreateRenderPipeline(wgpuctx.device, &pipelineDescriptor);
	}
	else
	{
		std::array<WGPURenderPipeline, 3> pipelines;
		for (uint32_t cullMode = 0; cullMode < 3; cullMode++)
		{
			pipelineDescriptor.primitive.cullMode = TranslateCullMode(static_cast<CullMode>(cullMode));
			pipelines[cullMode] = wgpuDeviceCreateRenderPipeline(wgpuctx.device, &pipelineDescriptor);
		}

		graphicsPipeline.pipeline = pipelines[0];
		graphicsPipeline.dynamicCullModePipelines = pipelines;
	}

	pipeline->pipeline = graphicsPipeline;
	return AbstractPipeline::Wrap(pipeline);
}

void GraphicsPipeline::Destroy()
{
	if (dynamicCullModePipelines.has_value())
	{
		for (WGPURenderPipeline p : *dynamicCullModePipelines)
			wgpuRenderPipelineRelease(p);
	}
	else
	{
		wgpuRenderPipelineRelease(pipeline);
	}
}

void GraphicsPipeline::Bind(CommandContext& cc)
{
	EG_ASSERT(cc.renderPassEncoder != nullptr);

	if (!enableScissorTest)
		cc.SetScissor(std::nullopt);

	if (HasDynamicCullMode())
	{
		cc.DynamicCullModeMarkDirty();
	}
	else
	{
		wgpuRenderPassEncoderSetPipeline(cc.renderPassEncoder, pipeline);
	}
}
} // namespace eg::graphics_api::webgpu
