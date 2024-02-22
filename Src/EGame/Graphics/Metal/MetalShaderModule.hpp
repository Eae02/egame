#pragma once

#include "../Abstraction.hpp"
#include "../SpirvCrossFwd.hpp"
#include "MetalMain.hpp"
#include "MetalPipeline.hpp"

namespace eg::graphics_api::mtl
{
struct SpecializationConstant
{
	uint32_t constantID;
	MTL::DataType dataType;

	bool operator<(const SpecializationConstant& other) const { return constantID < other.constantID; }
	bool operator<(uint32_t other) const { return constantID < other; }
};

struct ShaderModule
{
	ShaderStage stage;

	uint32_t usedBufferLocations;

	// Specialization constants sorted by constantID for faster lookup
	std::vector<SpecializationConstant> specializationConstants;

	std::shared_ptr<StageBindingsTable> bindingsTable;

	MTL::Library* mtlLibrary;

	static ShaderModule* Unwrap(ShaderModuleHandle handle) { return reinterpret_cast<ShaderModule*>(handle); }
};
} // namespace eg::graphics_api::mtl
