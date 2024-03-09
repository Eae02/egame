#include "../../String.hpp"
#include "../SpirvCrossUtils.hpp"
#include "MetalCommandContext.hpp"
#include "MetalPipeline.hpp"
#include "MetalShaderModule.hpp"
#include "MetalTranslation.hpp"

#include <Metal/MTLLibrary.hpp>
#include <optional>
#include <spirv.hpp>
#include <spirv_cross.hpp>
#include <spirv_msl.hpp>

namespace eg::graphics_api::mtl
{
static const std::pair<MTL::PrimitiveTopologyClass, MTL::PrimitiveType> topologyTranslationTable[] = {
	[(int)Topology::TriangleList] = { MTL::PrimitiveTopologyClassTriangle, MTL::PrimitiveTypeTriangle },
	[(int)Topology::TriangleStrip] = { MTL::PrimitiveTopologyClassTriangle, MTL::PrimitiveTypeTriangleStrip },
	[(int)Topology::LineList] = { MTL::PrimitiveTopologyClassLine, MTL::PrimitiveTypeLine },
	[(int)Topology::LineStrip] = { MTL::PrimitiveTopologyClassLine, MTL::PrimitiveTypeLineStrip },
	[(int)Topology::Points] = { MTL::PrimitiveTopologyClassPoint, MTL::PrimitiveTypePoint },
};

PipelineHandle CreateGraphicsPipeline(const GraphicsPipelineCreateInfo& createInfo)
{
	MTL::RenderPipelineDescriptor* descriptor = MTL::RenderPipelineDescriptor::alloc()->init();

	auto PrepareShaderModule =
		[&](const ShaderStageInfo& stageInfo) -> std::pair<MTL::Function*, std::shared_ptr<StageBindingsTable>>
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

			if (specConstIt != module.specializationConstants.end() &&
			    specConstIt->constantID == specConstant.constantID)
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
	};

	auto [vsFunction, vsBindingTable] = PrepareShaderModule(createInfo.vertexShader);
	auto [fsFunction, fsBindingTable] = PrepareShaderModule(createInfo.fragmentShader);

	EG_ASSERT(vsFunction != nullptr);

	std::array<uint32_t, MAX_DESCRIPTOR_SETS> descriptorSetsMaxBindingPlusOne;
	for (uint32_t set = 0; set < MAX_DESCRIPTOR_SETS; set++)
	{
		size_t maxBindingPlusOne = vsBindingTable->bindingsMetalIndexTable[set].size();
		if (fsBindingTable != nullptr)
			maxBindingPlusOne = std::max(maxBindingPlusOne, fsBindingTable->bindingsMetalIndexTable[set].size());
		descriptorSetsMaxBindingPlusOne[set] = static_cast<uint32_t>(maxBindingPlusOne);
	}

	descriptor->setVertexFunction(vsFunction);
	descriptor->setFragmentFunction(fsFunction);

	descriptor->setAlphaToCoverageEnabled(createInfo.enableAlphaToCoverage);
	descriptor->setAlphaToOneEnabled(createInfo.enableAlphaToOne);
	descriptor->setRasterSampleCount(createInfo.sampleCount);

	EG_ASSERT(static_cast<int>(createInfo.topology) < std::size(topologyTranslationTable));
	auto [topologyClass, primitiveType] = topologyTranslationTable[static_cast<int>(createInfo.topology)];
	descriptor->setInputPrimitiveTopology(topologyClass);

	// Creates the vertex descriptor
	MTL::VertexDescriptor* vertexDescriptor = MTL::VertexDescriptor::alloc()->init();
	for (uint32_t i = 0; i < MAX_VERTEX_BINDINGS; i++)
	{
		if (createInfo.vertexBindings[i].IsEnabled())
		{
			auto layoutDescriptor = vertexDescriptor->layouts()->object(GetVertexBindingBufferIndex(i));
			layoutDescriptor->setStride(createInfo.vertexBindings[i].stride);
			if (createInfo.vertexBindings[i].inputRate == InputRate::Instance)
				layoutDescriptor->setStepFunction(MTL::VertexStepFunctionPerInstance);
		}
	}
	for (uint32_t i = 0; i < MAX_VERTEX_ATTRIBUTES; i++)
	{
		if (createInfo.vertexAttributes[i].IsEnabled())
		{
			auto attribDescriptor = vertexDescriptor->attributes()->object(i);
			attribDescriptor->setBufferIndex(GetVertexBindingBufferIndex(createInfo.vertexAttributes[i].binding));
			attribDescriptor->setOffset(createInfo.vertexAttributes[i].offset);
			attribDescriptor->setFormat(TranslateVertexFormat(createInfo.vertexAttributes[i].format));
		}
	}

	descriptor->setVertexDescriptor(vertexDescriptor);

	if (createInfo.depthAttachmentFormat != Format::Undefined)
	{
		descriptor->setDepthAttachmentPixelFormat(TranslatePixelFormat(createInfo.depthAttachmentFormat));
	}

	for (uint32_t i = 0; i < createInfo.numColorAttachments; i++)
	{
		EG_ASSERT(createInfo.colorAttachmentFormats[i] != eg::Format::Undefined);

		MTL::RenderPipelineColorAttachmentDescriptor* attachmentDescriptor = descriptor->colorAttachments()->object(i);
		attachmentDescriptor->setPixelFormat(TranslatePixelFormat(createInfo.colorAttachmentFormats[i]));

		// Reverse and set the write mask
		uint32_t writeMask = static_cast<uint32_t>(createInfo.blendStates[i].colorWriteMask);
		uint32_t revWriteMask =
			((writeMask & 1) << 3) | ((writeMask & 2) << 1) | ((writeMask & 4) >> 1) | ((writeMask & 8) >> 3);
		attachmentDescriptor->setWriteMask(static_cast<MTL::ColorWriteMask>(revWriteMask));

		if (createInfo.blendStates[i].enabled)
		{
			const auto& blendState = createInfo.blendStates[i];
			attachmentDescriptor->setBlendingEnabled(true);
			attachmentDescriptor->setRgbBlendOperation(TranslateBlendFunc(blendState.colorFunc));
			attachmentDescriptor->setAlphaBlendOperation(TranslateBlendFunc(blendState.alphaFunc));
			attachmentDescriptor->setSourceRGBBlendFactor(TranslateBlendFactor(blendState.srcColorFactor));
			attachmentDescriptor->setSourceAlphaBlendFactor(TranslateBlendFactor(blendState.srcAlphaFactor));
			attachmentDescriptor->setDestinationRGBBlendFactor(TranslateBlendFactor(blendState.dstColorFactor));
			attachmentDescriptor->setDestinationAlphaBlendFactor(TranslateBlendFactor(blendState.dstAlphaFactor));
		}
	}

	NS::Error* error = nullptr;
	MTL::RenderPipelineState* renderPipelineState = metalDevice->newRenderPipelineState(descriptor, &error);
	if (renderPipelineState == nullptr)
	{
		EG_PANIC("Error creating graphics pipeline: " << error->localizedDescription()->utf8String());
	}

	descriptor->release();

	if (createInfo.label != nullptr)
		renderPipelineState->label()->init(createInfo.label, NS::UTF8StringEncoding);

	// Creates the depth stencil state
	MTL::DepthStencilState* depthStencilState = nullptr;
	if (createInfo.depthAttachmentFormat != eg::Format::Undefined)
	{
		MTL::DepthStencilDescriptor* depthStencilDescriptor = MTL::DepthStencilDescriptor::alloc()->init();
		if (createInfo.enableDepthTest)
		{
			depthStencilDescriptor->setDepthCompareFunction(TranslateCompareOp(createInfo.depthCompare));
		}
		depthStencilDescriptor->setDepthWriteEnabled(createInfo.enableDepthWrite);

		depthStencilState = metalDevice->newDepthStencilState(depthStencilDescriptor);
		depthStencilDescriptor->release();

		if (createInfo.label != nullptr)
			depthStencilState->label()->init(createInfo.label, NS::UTF8StringEncoding);
	}

	Pipeline* pipeline = pipelinePool.New();

	pipeline->descriptorSetsMaxBindingPlusOne = descriptorSetsMaxBindingPlusOne;
	pipeline->variant = GraphicsPipeline{
		.pso = renderPipelineState,
		.cullMode = createInfo.cullMode.transform([](auto cullMode) { return TranslateCullMode(cullMode); }),
		.enableWireframeRasterization = createInfo.enableWireframeRasterization,
		.enableDepthClamp = createInfo.enableDepthClamp,
		.frontFaceCCW = createInfo.frontFaceCCW,
		.depthStencilState = depthStencilState,
		.blendColor = createInfo.blendConstants,
		.boundState =
			BoundGraphicsPipelineState{
				.primitiveType = primitiveType,
				.enableScissorTest = createInfo.enableScissorTest,
				.bindingsTableVS = std::move(vsBindingTable),
				.bindingsTableFS = std::move(fsBindingTable),
			},
	};

	return reinterpret_cast<PipelineHandle>(pipeline);
}

void GraphicsPipeline::Bind(MetalCommandContext& mcc) const
{
	mcc.RenderCmdEncoder().setRenderPipelineState(pso);

	if (depthStencilState != nullptr)
	{
		mcc.RenderCmdEncoder().setDepthStencilState(depthStencilState);
	}

	if (cullMode.has_value())
		mcc.SetCullMode(*cullMode);

	if (!enableWireframeRasterization)
		mcc.SetTriangleFillMode(MTL::TriangleFillModeFill);

	if (!boundState.enableScissorTest)
	{
		mcc.SetScissor(MTL::ScissorRect{
			.width = mcc.framebufferWidth,
			.height = mcc.framebufferHeight,
		});
	}

	mcc.SetFrontFaceCCW(frontFaceCCW);
	mcc.SetEnableDepthClamp(enableDepthClamp);
	mcc.SetBlendColor(blendColor);

	mcc.boundGraphicsPipelineState = &boundState;
}
} // namespace eg::graphics_api::mtl
