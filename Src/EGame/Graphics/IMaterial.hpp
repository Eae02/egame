#pragma once

#include "../API.hpp"
#include "AbstractionHL.hpp"

namespace eg
{
class EG_API IMaterial
{
public:
	enum class OrderRequirement
	{
		None,
		OnlyUnordered,
		OnlyOrdered
	};

	virtual size_t PipelineHash() const = 0;
	virtual bool BindPipeline(CommandContext& cmdCtx, void* drawArgs) const = 0;
	virtual bool BindMaterial(CommandContext& cmdCtx, void* drawArgs) const = 0;
	virtual OrderRequirement GetOrderRequirement() const { return OrderRequirement::None; }
	virtual bool CheckInstanceDataType(const std::type_info* instanceDataType) const { return true; };
};
} // namespace eg
