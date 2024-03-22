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

FramebufferLazyPipeline gizmoPipeline;

ShaderModule gizmoVertexShader;
ShaderModule gizmoFragmentShader;

static const eg::DescriptorSetBinding GIZMO_DS_BINDINGS[] = { {
	.binding = 0,
	.type = eg::BindingTypeUniformBuffer{ .dynamicOffset = true },
	.shaderAccess = ShaderAccessFlags::Vertex | ShaderAccessFlags::Fragment,
} };

void DestroyGizmoPipelines()
{
	gizmoVertexShader.Destroy();
	gizmoFragmentShader.Destroy();
	gizmoPipeline.DestroyPipelines();
}

void InitGizmoPipeline()
{
	gizmoVertexShader =
		ShaderModule(ShaderStage::Vertex, { reinterpret_cast<const char*>(Gizmo_vs_glsl), sizeof(Gizmo_vs_glsl) });

	gizmoFragmentShader =
		ShaderModule(ShaderStage::Fragment, { reinterpret_cast<const char*>(Gizmo_fs_glsl), sizeof(Gizmo_fs_glsl) });

	GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = { .shaderModule = gizmoVertexShader.Handle() };
	pipelineCI.fragmentShader = { .shaderModule = gizmoFragmentShader.Handle() };
	pipelineCI.descriptorSetBindings[0] = GIZMO_DS_BINDINGS;
	pipelineCI.vertexBindings[0] = { sizeof(float) * 3, InputRate::Vertex };
	pipelineCI.vertexAttributes[0] = { 0, DataType::Float32, 3, 0 };

	gizmoPipeline = FramebufferLazyPipeline(pipelineCI);
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

struct GizmoParameters
{
	glm::mat4 transform;
	glm::vec4 color;
};

GizmoDrawParametersBuffer::GizmoDrawParametersBuffer()
{
	m_parametersStride =
		RoundToNextMultiple<uint32_t>(sizeof(GizmoParameters), GetGraphicsDeviceInfo().uniformBufferOffsetAlignment);

	m_buffer = Buffer(BufferFlags::CopyDst | BufferFlags::UniformBuffer, m_parametersStride * 3, nullptr);
	m_descriptorSet = DescriptorSet(GIZMO_DS_BINDINGS);
	m_descriptorSet.BindUniformBuffer(m_buffer, 0, BIND_BUFFER_OFFSET_DYNAMIC, sizeof(GizmoParameters));
}

void GizmoDrawParametersBuffer::SetParameters(
	std::span<const glm::mat4> axisTransforms, int currentAxis, int hoveredAxis)
{
	EG_ASSERT(axisTransforms.size() == 3);

	eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBuffer(m_parametersStride * 3);
	char* uploadBufferMemory = static_cast<char*>(uploadBuffer.Map());

	for (int axis = 0; axis < 3; axis++)
	{
		glm::vec3 color = AXIS_COLORS.at(axis);
		if (currentAxis == axis)
			color = CURRENT_AXIS_COLOR;
		else if (currentAxis == -1 && hoveredAxis == axis)
			color *= 2.0f;

		*reinterpret_cast<GizmoParameters*>(uploadBufferMemory + axis * m_parametersStride) = {
			.transform = axisTransforms[axis],
			.color = glm::vec4(color, 1.0f),
		};
	}

	uploadBuffer.Flush();

	eg::DC.CopyBuffer(uploadBuffer.buffer, m_buffer, uploadBuffer.offset, 0, uploadBuffer.range);
	m_buffer.UsageHint(BufferUsage::UniformBuffer, ShaderAccessFlags::Vertex | ShaderAccessFlags::Fragment);
}

void GizmoDrawParametersBuffer::Bind(uint32_t axis) const
{
	uint32_t offset = axis * m_parametersStride;
	eg::DC.BindDescriptorSet(m_descriptorSet, 0, { &offset, 1 });
}
} // namespace eg::detail
