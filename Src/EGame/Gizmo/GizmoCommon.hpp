#pragma once

#include <optional>
#include <span>
#include <glm/glm.hpp>

#include "../Graphics/AbstractionHL.hpp"

namespace eg { class Ray; }

namespace eg::detail
{
	extern float ARROW_VERTICES[75];
	extern uint16_t ARROW_INDICES[138];
	
	extern float TORUS_VERTICES[360];
	extern uint16_t TORUS_INDICES[720];
	
	extern Pipeline gizmoPipeline;
	
	void InitializeGizmoPipeline();
	
	std::optional<float> RayIntersectGizmoMesh(
		const glm::mat4& worldMatrix, const Ray& ray,
		std::span<const float> vertices, std::span<const uint16_t> indices);
	
	void DrawGizmoAxis(int axis, int currentAxis, int hoveredAxis, uint32_t numIndices, const glm::mat4& transform);
}
