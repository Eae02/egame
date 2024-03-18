#pragma once

#include <glm/glm.hpp>
#include <optional>
#include <span>

#include "../Graphics/AbstractionHL.hpp"
#include "../Graphics/FramebufferLazyPipeline.hpp"

namespace eg
{
class Ray;
}

namespace eg::detail
{
extern float ARROW_VERTICES[75];
extern uint16_t ARROW_INDICES[138];

extern float TORUS_VERTICES[360];
extern uint16_t TORUS_INDICES[720];

extern FramebufferLazyPipeline gizmoPipeline;

void DestroyGizmoPipelines();

void InitGizmoPipeline();

class GizmoDrawParametersBuffer
{
public:
	explicit GizmoDrawParametersBuffer();

	void SetParameters(std::span<const glm::mat4> axisTransforms, int currentAxis, int hoveredAxis);

	void Bind(uint32_t axis) const;

private:
	uint32_t m_parametersStride;
	Buffer m_buffer;
	DescriptorSet m_descriptorSet;
};

std::optional<float> RayIntersectGizmoMesh(
	const glm::mat4& worldMatrix, const Ray& ray, std::span<const float> vertices, std::span<const uint16_t> indices);
} // namespace eg::detail
