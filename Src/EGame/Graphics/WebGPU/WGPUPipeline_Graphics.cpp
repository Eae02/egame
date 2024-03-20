#include "../Abstraction.hpp"
#include "../SpirvCrossUtils.hpp"
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
}

static WGPUVertexStepMode TranslateInputRate(InputRate inputRate)
{
	switch (inputRate)
	{
	case InputRate::Vertex: return WGPUVertexStepMode_Vertex;
	case InputRate::Instance: return WGPUVertexStepMode_Instance;
	}
}

PipelineHandle CreateGraphicsPipeline(const GraphicsPipelineCreateInfo& createInfo)
{
	GraphicsPipeline* pipeline = new GraphicsPipeline;

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

	// Gets bind group layouts
	std::array<WGPUBindGroupLayout, MAX_DESCRIPTOR_SETS> bindGroupLayouts;
	uint32_t maxUsedBindGroup = 0;
	for (uint32_t set = 0; set < MAX_DESCRIPTOR_SETS; set++)
	{
		if (!bindings.sets[set].empty())
		{
			const CachedBindGroupLayout& cachedLayout = GetBindGroupLayout(bindings.sets[set]);
			pipeline->bindGroupLayouts[set] = &cachedLayout;
			bindGroupLayouts[set] = cachedLayout.bindGroupLayout;
			maxUsedBindGroup = set;
		}
	}

	// Creates the pipeline layout
	const WGPUPipelineLayoutDescriptor pipelineLayoutDescriptor = {
		.label = createInfo.label,
		.bindGroupLayoutCount = maxUsedBindGroup + 1,
		.bindGroupLayouts = bindGroupLayouts.data(),
	};
	pipeline->pipelineLayout = wgpuDeviceCreatePipelineLayout(wgpuctx.device, &pipelineLayoutDescriptor);

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

	// Processes vertex bindings
	std::vector<WGPUVertexBufferLayout> vertexBuffers;
	for (uint32_t binding = 0; binding < MAX_VERTEX_BINDINGS; binding++)
	{
		if (createInfo.vertexBindings[binding].IsEnabled())
		{
			EG_ASSERT(binding == 0 || createInfo.vertexBindings[binding - 1].IsEnabled());

			vertexBuffers.push_back(WGPUVertexBufferLayout{
				.arrayStride = createInfo.vertexBindings[binding].stride,
				.stepMode = TranslateInputRate(createInfo.vertexBindings[binding].inputRate),
				.attributeCount = bindingsAttributes[binding].size(),
				.attributes = bindingsAttributes[binding].data(),
			});
		}
	}

	WGPURenderPipelineDescriptor pipelineDescriptor = {
		.label = createInfo.label,
		.layout = pipeline->pipelineLayout,
		.vertex =
			WGPUVertexState{
				.module = vertexShader->shaderModule,
				.entryPoint = "main",
				.bufferCount = vertexBuffers.size(),
				.buffers = vertexBuffers.data(),
			},
		.primitive =
			WGPUPrimitiveState{
				.topology = TranslatePrimitiveTopology(createInfo.topology),
				.frontFace = createInfo.frontFaceCCW ? WGPUFrontFace_CCW : WGPUFrontFace_CW,
				.stripIndexFormat = WGPUIndexFormat_Uint32, // TODO: Deal with this
				.cullMode = TranslateCullMode(createInfo.cullMode.value_or(CullMode::None)),
			},
		.multisample =
			WGPUMultisampleState{
				.count = createInfo.sampleCount,
				.mask = 0xFFFFFFFF,
				.alphaToCoverageEnabled = createInfo.enableAlphaToCoverage,
			},
	};

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

			// TODO: Blend
			// if (createInfo.blendStates[i].enabled)
			// {
			// 	blendStates[i] = WGPUBlendState { };
			// 	colorTargetStates[i].blend = &blendStates[i];
			// }
		}

		fragmentState = WGPUFragmentState{
			.module = fragmentShader->shaderModule,
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
		depthStencilState = WGPUDepthStencilState{
			.format = TranslateTextureFormat(createInfo.depthAttachmentFormat),
			.depthWriteEnabled = createInfo.enableDepthWrite,
			.depthCompare =
				createInfo.enableDepthTest ? TranslateCompareOp(createInfo.depthCompare) : WGPUCompareFunction_Always,
		};
		pipelineDescriptor.depthStencil = &depthStencilState;
	}

	if (createInfo.cullMode.has_value())
	{
		pipeline->pipeline = wgpuDeviceCreateRenderPipeline(wgpuctx.device, &pipelineDescriptor);
	}
	else
	{
		std::array<WGPURenderPipeline, 3> pipelines;
		for (uint32_t cullMode = 0; cullMode < 3; cullMode++)
		{
			pipelineDescriptor.primitive.cullMode = TranslateCullMode(static_cast<CullMode>(cullMode));
			pipelines[cullMode] = wgpuDeviceCreateRenderPipeline(wgpuctx.device, &pipelineDescriptor);
		}

		pipeline->pipeline = pipelines[0];
		pipeline->dynamicCullModePipelines = pipelines;
	}

	return AbstractPipeline::Wrap(pipeline);
}

void GraphicsPipeline::Bind(CommandContext& cc)
{
	if (!HasDynamicCullMode())
	{
		wgpuRenderPassEncoderSetPipeline(cc.renderPassEncoder, pipeline);
	}

	cc.currentPipeline = this;
}
} // namespace eg::graphics_api::webgpu
