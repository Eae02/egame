#include "GizmoCommon.hpp"
#include "../Geometry/Plane.hpp"
#include "../Geometry/Ray.hpp"
#include "../Utils.hpp"

#include "../../Shaders/Build/Gizmo.fs.h"
#include "../../Shaders/Build/Gizmo.vs.h"

namespace eg::detail
{
static constexpr float AXIS_LIGHTNESS = 0.25f;
static const std::array<glm::vec3, 3> AXIS_COLORS = {
	glm::vec3(1.0f, AXIS_LIGHTNESS, AXIS_LIGHTNESS),
	glm::vec3(AXIS_LIGHTNESS, 1.0f, AXIS_LIGHTNESS),
	glm::vec3(AXIS_LIGHTNESS, AXIS_LIGHTNESS, 1.0f),
};

static const glm::vec3 CURRENT_AXIS_COLOR(1.0f, 1.0f, 0.5f);

Pipeline gizmoPipeline;

void InitializeGizmoPipeline()
{
	return; // TODO: Implement format for the pipeline

	if (gizmoPipeline.handle)
		return;

	ShaderModule vs(ShaderStage::Vertex, { reinterpret_cast<const char*>(Gizmo_vs_glsl), sizeof(Gizmo_vs_glsl) });
	ShaderModule fs(ShaderStage::Fragment, { reinterpret_cast<const char*>(Gizmo_fs_glsl), sizeof(Gizmo_fs_glsl) });

	GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = vs.Handle();
	pipelineCI.fragmentShader = fs.Handle();
	pipelineCI.vertexBindings[0] = { sizeof(float) * 3, InputRate::Vertex };
	pipelineCI.vertexAttributes[0] = { 0, DataType::Float32, 3, 0 };
	gizmoPipeline = Pipeline::Create(pipelineCI);
}

std::optional<float> RayIntersectGizmoMesh(
	const glm::mat4& worldMatrix, const Ray& ray, std::span<const float> vertices, std::span<const uint16_t> indices)
{
	glm::vec3* verticesWorld = reinterpret_cast<glm::vec3*>(alloca(vertices.size_bytes()));
	for (size_t i = 0; i < vertices.size() / 3; i++)
	{
		verticesWorld[i] = worldMatrix * glm::vec4(vertices[i * 3 + 0], vertices[i * 3 + 1], vertices[i * 3 + 2], 1.0f);
	}

	std::optional<float> result;
	for (uint32_t i = 0; i < indices.size(); i += 3)
	{
		const glm::vec3& v0 = verticesWorld[indices[i + 0]];
		const glm::vec3& v1 = verticesWorld[indices[i + 1]];
		const glm::vec3& v2 = verticesWorld[indices[i + 2]];

		Plane plane(v0, v1, v2);

		float intersectDist;
		if (ray.Intersects(plane, intersectDist) && intersectDist > 0 &&
		    (!result.has_value() || intersectDist < *result))
		{
			glm::vec3 intersectPos = ray.GetPoint(intersectDist);
			if (TriangleContainsPoint(v0, v1, v2, intersectPos))
			{
				result = intersectDist;
			}
		}
	}

	return result;
}

void DrawGizmoAxis(int axis, int currentAxis, int hoveredAxis, uint32_t numIndices, const glm::mat4& transform)
{
	glm::vec3 color = AXIS_COLORS.at(axis);
	if (currentAxis == axis)
	{
		color = CURRENT_AXIS_COLOR;
	}
	else if (currentAxis == -1 && hoveredAxis == axis)
	{
		color *= 2.0f;
	}

	struct PC
	{
		glm::mat4 transform;
		glm::vec4 color;
	} pc;
	pc.transform = transform;
	pc.color = glm::vec4(color, 1.0f);

	DC.PushConstants(0, pc);

	DC.DrawIndexed(0, numIndices, 0, 0, 1);
}
} // namespace eg::detail
