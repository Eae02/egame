#include "TranslationGizmo.hpp"
#include "Geometry/Plane.hpp"
#include "../Shaders/Build/Gizmo.vs.h"
#include "../Shaders/Build/Gizmo.fs.h"

#include <glm/gtc/matrix_transform.hpp>
#include <span>

namespace eg
{
	extern float ARROW_VERTICES[75];
	extern uint16_t ARROW_INDICES[138];
	
	static Buffer s_arrowVB;
	static Buffer s_arrowIB;
	Pipeline gizmoPipeline;
	
	static const glm::vec3 ARROW_OFFSET = glm::vec3(0.2f, 0.0f, 0.0f);
	static const glm::vec3 ARROW_SCALE = glm::vec3(0.8f, 0.6f, 0.6f);
	
	static constexpr float AXIS_LIGHTNESS = 0.25f;
	static const glm::vec3 AXIS_COLORS[] = 
	{
		glm::vec3(1.0f, AXIS_LIGHTNESS, AXIS_LIGHTNESS),
		glm::vec3(AXIS_LIGHTNESS, 1.0f, AXIS_LIGHTNESS),
		glm::vec3(AXIS_LIGHTNESS, AXIS_LIGHTNESS, 1.0f),
	};
	
	static const glm::vec3 CURRENT_AXIS_COLOR(1.0f, 1.0f, 0.5f);
	
	void TranslationGizmo::Initialize()
	{
		for (uint16_t& i : ARROW_INDICES)
			i--;
		s_arrowVB = eg::Buffer(BufferFlags::VertexBuffer, sizeof(ARROW_VERTICES), ARROW_VERTICES);
		s_arrowIB = eg::Buffer(BufferFlags::IndexBuffer, sizeof(ARROW_INDICES), ARROW_INDICES);
		
		s_arrowVB.UsageHint(BufferUsage::VertexBuffer);
		s_arrowIB.UsageHint(BufferUsage::IndexBuffer);
		
		ShaderModule vs(ShaderStage::Vertex, { reinterpret_cast<const char*>(Gizmo_vs_glsl), sizeof(Gizmo_vs_glsl) });
		ShaderModule fs(ShaderStage::Fragment, { reinterpret_cast<const char*>(Gizmo_fs_glsl), sizeof(Gizmo_fs_glsl) });
		
		GraphicsPipelineCreateInfo pipelineCI;
		pipelineCI.vertexShader = vs.Handle();
		pipelineCI.fragmentShader = fs.Handle();
		pipelineCI.vertexBindings[0] = { sizeof(float) * 3, InputRate::Vertex };
		pipelineCI.vertexAttributes[0] = { 0, DataType::Float32, 3, 0 };
		gizmoPipeline = Pipeline::Create(pipelineCI);
	}
	
	void TranslationGizmo::Destroy()
	{
		s_arrowVB.Destroy();
		s_arrowIB.Destroy();
		gizmoPipeline.Destroy();
	}
	
	std::optional<float> RayIntersectGizmoMesh(const glm::mat4& worldMatrix, const Ray& ray,
		std::span<const float> vertices, std::span<const uint16_t> indices)
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
	
	static inline glm::mat4 GetAxisTransform(const glm::vec3& position, float scale, int axis)
	{
		glm::mat4 rotation(0.0f);
		for (int i = 0; i < 3; i++)
			rotation[i][(axis + i) % 3] = 1;
		rotation[3][3] = 1;
		
		return glm::translate(glm::mat4(), position) *
			rotation *
			glm::translate(glm::mat4(), ARROW_OFFSET * scale) *
			glm::scale(glm::mat4(), ARROW_SCALE * scale);
	}
	
	void TranslationGizmo::Update(glm::vec3& position, const glm::vec3& cameraPos, const glm::mat4& viewProjMatrix, const Ray& viewRay)
	{
		m_renderScale = glm::distance(cameraPos, position) * size;
		
		//Calculates the depth of the end of each arrow
		float arrowDepths[3];
		for (int i = 0; i < 3; i++)
		{
			glm::vec3 endPos = position;
			endPos[i] += ARROW_SCALE.x * m_renderScale;
			
			glm::vec4 endPosPPS = viewProjMatrix * glm::vec4(endPos, 1.0f);
			arrowDepths[i] = endPosPPS.z / endPosPPS.w;
		}
		
		std::sort(std::begin(m_axisDrawOrder), std::end(m_axisDrawOrder), [&] (int a, int b)
		{
			return arrowDepths[a] > arrowDepths[b];
		});
		
		auto BeginDragAxis = [&] (int index)
		{
			m_currentAxis = index;
			
			glm::vec3 dragDirection;
			dragDirection[index] = 1;
			m_axisDragRay = { position, dragDirection };
			
			m_initialDragDist = m_axisDragRay.GetClosestPoint(viewRay);
			m_keyboardSelectingAxis = false;
		};
		
		//Handles keyboard input
		if (!m_keyboardSelectingAxis)
		{
			if (IsButtonDown(Button::G) && !WasButtonDown(Button::G))
				m_keyboardSelectingAxis = true;
		}
		else
		{
			if (IsButtonDown(Button::X) && !WasButtonDown(Button::X))
				BeginDragAxis(0);
			if (IsButtonDown(Button::Y) && !WasButtonDown(Button::Y))
				BeginDragAxis(1);
			if (IsButtonDown(Button::Z) && !WasButtonDown(Button::Z))
				BeginDragAxis(2);
			if (IsButtonDown(Button::Escape) && !WasButtonDown(Button::Escape))
				m_keyboardSelectingAxis = false;
		}
		
		bool select = m_currentAxis == -1 && IsButtonDown(Button::MouseLeft) && !WasButtonDown(Button::MouseLeft);
		
		//Resets the current axis if the mouse button was released.
		if (WasButtonDown(Button::MouseLeft) && !IsButtonDown(Button::MouseLeft))
		{
			m_currentAxis = -1;
		}
		
		//Drags the gizmo
		if (m_currentAxis != -1)
		{
			float dragDist = m_axisDragRay.GetClosestPoint(viewRay);
			if (!std::isnan(dragDist))
			{
				position = m_axisDragRay.GetPoint(dragDist - m_initialDragDist);
			}
		}
		
		m_hoveredAxis = -1;
		for (int axis : m_axisDrawOrder)
		{
			const glm::mat4 worldMatrix = GetAxisTransform(position, m_renderScale, axis);
			if (RayIntersectGizmoMesh(worldMatrix, viewRay, ARROW_VERTICES, ARROW_INDICES))
			{
				m_hoveredAxis = axis;
				if (select)
					m_currentAxis = axis;
			}
		}
		
		if (select && m_currentAxis != -1)
		{
			BeginDragAxis(m_currentAxis);
			select = false;
		}
		
		m_lastPosition = position;
	}
	
	void DrawGizmoAxis(int axis, int currentAxis, int hoveredAxis, uint32_t numIndices, const glm::mat4& transform)
	{
		glm::vec3 color = AXIS_COLORS[axis];
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
	
	void TranslationGizmo::Draw(const glm::mat4& viewProjMatrix) const
	{
		DC.BindPipeline(gizmoPipeline);
		DC.BindVertexBuffer(0, s_arrowVB, 0);
		DC.BindIndexBuffer(IndexType::UInt16, s_arrowIB, 0);
		
		for (int axis : m_axisDrawOrder)
		{
			DrawGizmoAxis(axis, m_currentAxis, m_hoveredAxis, std::size(ARROW_INDICES),
			              viewProjMatrix * GetAxisTransform(m_lastPosition, m_renderScale, axis));
		}
	}
}
