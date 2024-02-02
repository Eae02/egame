#pragma once

#include "../AbstractionHL.hpp"

namespace eg
{
class EG_API SPFMapGenerator
{
public:
	SPFMapGenerator();

	void Generate(
		CommandContext& cc, const Texture& inputEnvMap, Texture& output, uint32_t arrayLayer = 0,
		float irradianceScale = 1.0f) const;

	static constexpr Format MAP_FORMAT = Format::R32G32B32A32_Float;

private:
	eg::Pipeline m_pipeline;
};
} // namespace eg
