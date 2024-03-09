#include "MetalShaderModule.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "../../String.hpp"
#include "../SpirvCrossUtils.hpp"
#include <spirv_msl.hpp>

namespace eg::graphics_api::mtl
{
static ConcurrentObjectPool<ShaderModule> shaderModulePool;

static inline spv::ExecutionModel StageToSpvExecutionModel(ShaderStage stage)
{
	switch (stage)
	{
	case ShaderStage::Vertex: return spv::ExecutionModelVertex;
	case ShaderStage::Fragment: return spv::ExecutionModelFragment;
	case ShaderStage::Compute: return spv::ExecutionModelGLCompute;
	default: EG_PANIC("Unsupported stage: " << static_cast<int>(stage));
	}
}

static std::optional<bool> shouldDumpMSL;

static inline bool ShouldDumpMSL()
{
	if (!shouldDumpMSL.has_value())
	{
		const char* envValue = std::getenv("EG_DUMP_MSL");
		shouldDumpMSL = envValue != nullptr && std::strcmp(envValue, "1") == 0;
	}
	return *shouldDumpMSL;
}

ShaderModuleHandle CreateShaderModule(ShaderStage stage, const spirv_cross::ParsedIR& parsedIR)
{
	ShaderModule* module = shaderModulePool.New();
	module->stage = stage;

	spirv_cross::CompilerMSL compiler(parsedIR);
	spirv_cross::ShaderResources shaderResources = compiler.get_shader_resources();

	DescriptorSetBindings bindings;
	bindings.AppendFromReflectionInfo(module->stage, compiler, shaderResources);

	module->bindingsTable = std::make_shared<StageBindingsTable>();
	StageBindingsTable& bindingsTable = *module->bindingsTable;

	bindingsTable.pushConstantBytes = GetPushConstantBytes(compiler, &shaderResources);

	spv::ExecutionModel executionModel = StageToSpvExecutionModel(stage);

	auto stageAccessFlag = static_cast<ShaderAccessFlags>(1 << static_cast<int>(stage));

	uint32_t nextBufferIndex = 0;
	uint32_t nextTextureIndex = 0;

	// Assigns a binding to the push constants buffer
	if (bindingsTable.pushConstantBytes > 0)
	{
		uint32_t metalBinding = nextBufferIndex++;
		bindingsTable.pushConstantsBinding = metalBinding;
		compiler.add_msl_resource_binding(spirv_cross::MSLResourceBinding{
			.stage = executionModel,
			.desc_set = spirv_cross::kPushConstDescSet,
			.binding = spirv_cross::kPushConstBinding,
			.count = 1,
			.msl_buffer = metalBinding,
		});
	}

	for (uint32_t i = 0; i < MAX_DESCRIPTOR_SETS; i++)
	{
		bindingsTable.bindingsMetalIndexTable[i].resize(DescriptorSetBinding::MaxBindingPlusOne(bindings.sets[i]), -1);

		// Assigns a binding to each resource in this descriptor set
		for (const DescriptorSetBinding& setBinding : bindings.sets[i])
		{
			if (!HasFlag(setBinding.shaderAccess, stageAccessFlag))
				continue;

			spirv_cross::MSLResourceBinding resourceBinding = {
				.stage = executionModel,
				.desc_set = i,
				.binding = setBinding.binding,
				.count = 1,
			};

			uint32_t metalIndex;

			switch (setBinding.type)
			{
			case BindingType::UniformBuffer:
			case BindingType::StorageBuffer:
			case BindingType::UniformBufferDynamicOffset:
			case BindingType::StorageBufferDynamicOffset:
				metalIndex = nextBufferIndex++;
				resourceBinding.msl_buffer = metalIndex;
				break;
			case BindingType::Texture:
				metalIndex = nextTextureIndex++;
				resourceBinding.msl_texture = metalIndex;
				resourceBinding.msl_sampler = metalIndex;
				break;
			case BindingType::StorageImage:
				metalIndex = nextTextureIndex++;
				resourceBinding.msl_texture = metalIndex;
				resourceBinding.msl_buffer = nextBufferIndex++;
				break;
			}

			bindingsTable.bindingsMetalIndexTable[i].at(setBinding.binding) = metalIndex;

			compiler.add_msl_resource_binding(resourceBinding);
		}
	}

	// Processes specialization constants
	for (spirv_cross::SpecializationConstant& specConst : compiler.get_specialization_constants())
	{
		if (specConst.constant_id == 500)
			continue;

		const spirv_cross::SPIRType& type = compiler.get_type(compiler.get_constant(specConst.id).constant_type);
		std::optional<MTL::DataType> metalDataType;
		switch (type.basetype)
		{
		case spirv_cross::SPIRType::Boolean: metalDataType = MTL::DataTypeBool; break;
		case spirv_cross::SPIRType::Int: metalDataType = MTL::DataTypeInt; break;
		case spirv_cross::SPIRType::UInt: metalDataType = MTL::DataTypeUInt; break;
		case spirv_cross::SPIRType::Float: metalDataType = MTL::DataTypeFloat; break;
		default:
			eg::Log(eg::LogLevel::Error, "mtl", "Unrecognized specialization constant type: {0}", type.basetype);
			break;
		}

		if (metalDataType.has_value())
		{
			module->specializationConstants.push_back(SpecializationConstant{
				.constantID = specConst.constant_id,
				.dataType = *metalDataType,
			});
		}
	}
	sort(module->specializationConstants.begin(), module->specializationConstants.end());

	module->usedBufferLocations = nextBufferIndex;

	std::string code = compiler.compile();

	if (ShouldDumpMSL())
	{
		std::cerr << "-- MSL Dump --\n";
		IterateStringParts(code, '\n', [&](std::string_view line) { std::cerr << " |   " << line << "\n"; });
		std::cerr << "---------------\n\n";
	}

	NS::Error* error = nullptr;
	module->mtlLibrary =
		metalDevice->newLibrary(NS::String::string(code.c_str(), NS::UTF8StringEncoding), nullptr, &error);
	if (module->mtlLibrary == nullptr)
	{
		EG_PANIC("Error creating shader library: " << error->localizedDescription()->utf8String());
	}

	return reinterpret_cast<ShaderModuleHandle>(module);
}

void DestroyShaderModule(ShaderModuleHandle handle)
{
	shaderModulePool.Delete(ShaderModule::Unwrap(handle));
}
} // namespace eg::graphics_api::mtl
