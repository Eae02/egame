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

#include <iostream>

namespace eg::graphics_api::mtl
{
static const std::pair<MTL::PrimitiveTopologyClass, MTL::PrimitiveType> topologyTranslationTable[] = {
	[(int)Topology::TriangleList] = { MTL::PrimitiveTopologyClassTriangle, MTL::PrimitiveTypeTriangle },
	[(int)Topology::TriangleStrip] = { MTL::PrimitiveTopologyClassTriangle, MTL::PrimitiveTypeTriangleStrip },
	[(int)Topology::LineList] = { MTL::PrimitiveTopologyClassLine, MTL::PrimitiveTypeLine },
	[(int)Topology::LineStrip] = { MTL::PrimitiveTopologyClassLine, MTL::PrimitiveTypeLineStrip },
	[(int)Topology::Points] = { MTL::PrimitiveTopologyClassPoint, MTL::PrimitiveTypePoint },
};

/*
Fields not handled:
enableStencilTest
frontStencilState
backStencilState
dynamicStencilCompareMask
dynamicStencilWriteMask
dynamicStencilReference
enableSampleShading
minSampleShading
patchControlPoints
numClipDistances
lineWidth
blendConstants
*/

static std::optional<bool> shouldDumpMSL;

bool ShouldDumpMSL()
{
	if (!shouldDumpMSL.has_value())
	{
		const char* envValue = std::getenv("EG_DUMP_MSL");
		shouldDumpMSL = envValue != nullptr && std::strcmp(envValue, "1") == 0;
	}
	return *shouldDumpMSL;
}

PipelineHandle CreateGraphicsPipeline(const GraphicsPipelineCreateInfo& createInfo)
{
	MTL::RenderPipelineDescriptor* descriptor = MTL::RenderPipelineDescriptor::alloc()->init();

	struct ShaderStageData
	{
		const ShaderModule* module;
		const ShaderStageInfo* stageInfo;

		std::unique_ptr<spirv_cross::CompilerMSL> compiler;
		spirv_cross::ShaderResources shaderResources;
		uint32_t pushConstantBytes;

		StageBindingsTable bindingsTable;

		MTL::Library* library;
		MTL::Function* function;
	};

	DescriptorSetBindings bindings;

	auto MakeShaderStageData = [&](const ShaderStageInfo& stageInfo) -> std::optional<ShaderStageData>
	{
		if (stageInfo.shaderModule == nullptr)
			return std::nullopt;

		ShaderModule* module = reinterpret_cast<ShaderModule*>(stageInfo.shaderModule);

		ShaderStageData data = {
			.module = module,
			.stageInfo = &stageInfo,
			.compiler = std::make_unique<spirv_cross::CompilerMSL>(*module->parsedIR),
			.pushConstantBytes = 0,
		};

		data.compiler->set_msl_options(spirv_cross::CompilerMSL::Options{});

		data.shaderResources = data.compiler->get_shader_resources();

		data.pushConstantBytes = GetPushConstantBytes(*data.compiler, &data.shaderResources);
		bindings.AppendFromReflectionInfo(module->stage, *data.compiler, data.shaderResources);

		return data;
	};

	ShaderStageData vertexStageData = *MakeShaderStageData(createInfo.vertexShader);
	std::optional<ShaderStageData> fragmentStageData = MakeShaderStageData(createInfo.fragmentShader);

	// Constructs metal binding maps
	std::array<uint32_t, MAX_DESCRIPTOR_SETS> descriptorSetsMaxBindingPlusOne;
	for (uint32_t i = 0; i < MAX_DESCRIPTOR_SETS; i++)
	{
		descriptorSetsMaxBindingPlusOne[i] = DescriptorSetBinding::MaxBindingPlusOne(bindings.sets[i]);
	}

	uint32_t maxActiveVertexBindingPlusOne = 0;
	for (uint32_t i = 0; i < MAX_VERTEX_BINDINGS; i++)
	{
		if (createInfo.vertexBindings[i].IsEnabled())
			maxActiveVertexBindingPlusOne = i + 1;
	}

	auto CompileShaderStageToMSL = [&](ShaderStageData& stageData, const char* stageName, spv::ExecutionModel stage)
	{
		uint32_t nextBufferIndex = stage == spv::ExecutionModelVertex ? maxActiveVertexBindingPlusOne : 0;

		auto stageAccessFlag = static_cast<ShaderAccessFlags>(1 << static_cast<int>(stageData.module->stage));

		// Assigns a binding to the push constants buffer
		if (stageData.pushConstantBytes != 0)
		{
			uint32_t metalBinding = nextBufferIndex++;
			stageData.bindingsTable.pushConstantsBinding = metalBinding;
			stageData.compiler->add_msl_resource_binding(spirv_cross::MSLResourceBinding{
				.stage = stage,
				.desc_set = spirv_cross::kPushConstDescSet,
				.binding = spirv_cross::kPushConstBinding,
				.count = 1,
				.msl_buffer = metalBinding,
			});
		}

		uint32_t nextTextureIndex = 0;

		for (uint32_t i = 0; i < MAX_DESCRIPTOR_SETS; i++)
		{
			stageData.bindingsTable.bindingsMetalIndexTable[i].resize(descriptorSetsMaxBindingPlusOne[i], -1);

			// Assigns a binding to each resource in this descriptor set
			for (const DescriptorSetBinding& setBinding : bindings.sets[i])
			{
				if (!HasFlag(setBinding.shaderAccess, stageAccessFlag))
					continue;

				uint32_t metalIndex;

				switch (setBinding.type)
				{
				case BindingType::UniformBuffer:
				case BindingType::StorageBuffer: metalIndex = nextBufferIndex++; break;
				case BindingType::Texture:
				case BindingType::StorageImage: metalIndex = nextTextureIndex++; break;
				}

				stageData.bindingsTable.bindingsMetalIndexTable[i].at(setBinding.binding) = metalIndex;

				stageData.compiler->add_msl_resource_binding(spirv_cross::MSLResourceBinding{
					.stage = stage,
					.desc_set = i,
					.binding = setBinding.binding,
					.count = 1,
					.msl_buffer = metalIndex,
					.msl_texture = metalIndex,
					.msl_sampler = metalIndex,
				});
			}
		}

		std::string code = stageData.compiler->compile();

		if (ShouldDumpMSL())
		{
			std::cerr << "-- MSL Dump - " << (createInfo.label ? createInfo.label : "unlabeled") << " - " << stageName
					  << " --\n";
			IterateStringParts(code, '\n', [&](std::string_view line) { std::cerr << " |   " << line << "\n"; });
			std::cerr << "---------------\n\n";
		}

		NS::Error* error = nullptr;
		stageData.library =
			metalDevice->newLibrary(NS::String::string(code.c_str(), NS::UTF8StringEncoding), nullptr, &error);
		if (!stageData.library)
		{
			EG_PANIC("Error creating shader library: " << error->localizedDescription()->utf8String());
		}

		MTL::FunctionConstantValues* constantValues = MTL::FunctionConstantValues::alloc()->init();
		for (spirv_cross::SpecializationConstant& specConst : stageData.compiler->get_specialization_constants())
		{
			spirv_cross::SPIRConstant& spirConst = stageData.compiler->get_constant(specConst.id);
			const void* dataPointer = nullptr;
			MTL::DataType dataType;

			if (specConst.constant_id == 500)
			{
				static const uint32_t METAL_API_CONSTANT = 2;
				dataPointer = &METAL_API_CONSTANT;
				dataType = MTL::DataTypeUInt;
			}
			else
			{
				for (const SpecializationConstantEntry& entry : stageData.stageInfo->specConstants)
				{
					if (specConst.constant_id == entry.constantID)
					{
						dataPointer = static_cast<const char*>(stageData.stageInfo->specConstantsData) + entry.offset;
						break;
					}
				}

				const auto& constant =
					stageData.compiler->get_type(stageData.compiler->get_constant(specConst.id).constant_type);
				switch (constant.basetype)
				{
				case spirv_cross::SPIRType::Boolean: dataType = MTL::DataTypeBool; break;
				case spirv_cross::SPIRType::Int: dataType = MTL::DataTypeInt; break;
				case spirv_cross::SPIRType::UInt: dataType = MTL::DataTypeUInt; break;
				case spirv_cross::SPIRType::Float: dataType = MTL::DataTypeFloat; break;
				default:
					eg::Log(
						eg::LogLevel::Error, "mtl", "Unrecognized specialization constant type: {0}",
						constant.basetype);
					dataPointer = nullptr;
					break;
				}
			}

			if (dataPointer != nullptr)
			{
				constantValues->setConstantValue(dataPointer, dataType, specConst.constant_id);
			}
		}

		stageData.function =
			stageData.library->newFunction(NS::String::string("main0", NS::UTF8StringEncoding), constantValues, &error);

		if (!stageData.function)
		{
			EG_PANIC("Error creating shader function: " << error->localizedDescription()->utf8String());
		}

		constantValues->release();
	};

	CompileShaderStageToMSL(vertexStageData, "vs", spv::ExecutionModelVertex);
	descriptor->setVertexFunction(vertexStageData.function);

	if (fragmentStageData.has_value())
	{
		CompileShaderStageToMSL(*fragmentStageData, "fs", spv::ExecutionModelFragment);
		descriptor->setFragmentFunction(fragmentStageData->function);
	}

	descriptor->setAlphaToCoverageEnabled(createInfo.enableAlphaToCoverage);
	descriptor->setAlphaToOneEnabled(createInfo.enableAlphaToOne);

	EG_ASSERT(static_cast<int>(createInfo.topology) < std::size(topologyTranslationTable));
	auto [topologyClass, primitiveType] = topologyTranslationTable[static_cast<int>(createInfo.topology)];
	descriptor->setInputPrimitiveTopology(topologyClass);

	// Creates the vertex descriptor
	MTL::VertexDescriptor* vertexDescriptor = MTL::VertexDescriptor::alloc()->init();
	for (uint32_t i = 0; i < MAX_VERTEX_BINDINGS; i++)
	{
		if (createInfo.vertexBindings[i].IsEnabled())
		{
			auto layoutDescriptor = vertexDescriptor->layouts()->object(i);
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
			attribDescriptor->setBufferIndex(createInfo.vertexAttributes[i].binding);
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
		.boundState =
			BoundGraphicsPipelineState{
				.primitiveType = primitiveType,
				.vertexShaderPushConstantBytes = vertexStageData.pushConstantBytes,
				.fragmentShaderPushConstantBytes = fragmentStageData ? fragmentStageData->pushConstantBytes : 0,
				.enableScissorTest = createInfo.enableScissorTest,
				.bindingsTableVertexShader = std::move(vertexStageData.bindingsTable),
				.bindingsTableFragmentShader =
					fragmentStageData ? std::move(fragmentStageData->bindingsTable) : StageBindingsTable(),
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

	mcc.boundGraphicsPipelineState = &boundState;
}
} // namespace eg::graphics_api::mtl
