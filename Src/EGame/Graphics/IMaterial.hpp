#pragma once

#include "AbstractionHL.hpp"
#include "../API.hpp"

namespace eg
{
	class EG_API IMaterial
	{
	public:
		virtual size_t PipelineHash() const = 0;
		virtual bool BindPipeline(CommandContext& cmdCtx, void* drawArgs) const = 0;
		virtual bool BindMaterial(CommandContext& cmdCtx, void* drawArgs) const = 0;
	};
}
