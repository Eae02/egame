#include "RotationGizmo.hpp"
#include "Plane.hpp"
#include "Ray.hpp"

#include <span>
#include <optional>

namespace eg
{
	extern float TORUS_VERTICES[360];
	extern uint16_t TORUS_INDICES[720];
	
	static Buffer s_torusVB;
	static Buffer s_torusIB;
	
	extern Pipeline gizmoPipeline;
	
	void RotationGizmo::Initialize()
	{
		s_torusVB = eg::Buffer(BufferFlags::VertexBuffer, sizeof(TORUS_VERTICES), TORUS_VERTICES);
		s_torusIB = eg::Buffer(BufferFlags::IndexBuffer, sizeof(TORUS_INDICES), TORUS_INDICES);
		
		s_torusVB.UsageHint(BufferUsage::VertexBuffer);
		s_torusIB.UsageHint(BufferUsage::IndexBuffer);
	}
	
	void RotationGizmo::Destroy()
	{
		s_torusVB.Destroy();
		s_torusIB.Destroy();
	}
	
	std::optional<float> RayIntersectGizmoMesh(const glm::mat4& worldMatrix, const Ray& ray,
		std::span<const float> vertices, std::span<const uint16_t> indices);
	
	void DrawGizmoAxis(int axis, int currentAxis, int hoveredAxis, uint32_t numIndices, const glm::mat4& transform);
	
	static const float TORUS_SCALE = 0.6f;
	
	static inline glm::mat4 GetAxisTransform(const glm::vec3& position, float scale, int axis)
	{
		glm::mat4 rotationAndScale(0.0f);
		rotationAndScale[0][(axis + 1) % 3] = scale;
		rotationAndScale[1][axis]           = scale;
		rotationAndScale[2][(axis + 2) % 3] = scale;
		rotationAndScale[3][3] = 1;
		
		return glm::translate(glm::mat4(), position) * rotationAndScale;
	}
	
	void RotationGizmo::Update(glm::quat& rotation, const glm::vec3& position, const glm::vec3& cameraPos,
		const glm::mat4& viewProjMatrix, const Ray& viewRay)
	{
		m_renderScale = glm::distance(cameraPos, position) * size * TORUS_SCALE;
		
		auto GetPlaneIntersectPos = [&] (int axis) -> std::optional<glm::vec3>
		{
			glm::vec3 axisUpDir(0);
			axisUpDir[axis] = 1;
			
			Plane plane(axisUpDir, position);
			float intersectDistance;
			if (viewRay.Intersects(plane, intersectDistance))
			{
				return viewRay.GetPoint(intersectDistance);
			}
			return {};
		};
		
		auto BeginDragAxis = [&] (int axis)
		{
			if (std::optional<glm::vec3> intersectPos = GetPlaneIntersectPos(axis))
			{
				m_currentAxis = axis;
				m_initialRotation = rotation;
				m_rotationAmount = 0;
				m_keyboardSelectingAxis = false;
				m_previousDragVector = glm::normalize(*intersectPos - position);
			}
		};
		
		bool select = m_currentAxis == -1 && IsButtonDown(Button::MouseLeft) && !WasButtonDown(Button::MouseLeft);
		
		//Resets the current axis if the mouse button was released.
		if (WasButtonDown(Button::MouseLeft) && !IsButtonDown(Button::MouseLeft))
		{
			m_currentAxis = -1;
		}
		
		//Drags the gizmo
		if (m_currentAxis != -1)
		{
			if (std::optional<glm::vec3> intersectPos = GetPlaneIntersectPos(m_currentAxis))
			{
				glm::vec3 toNewPos = glm::normalize(*intersectPos - position);
				float cosAngle = glm::dot(toNewPos, m_previousDragVector);
				if (cosAngle < 0.999f)
				{
					float angle = std::acos(cosAngle);
					glm::vec3 crossProd = glm::cross(toNewPos, m_previousDragVector);
					if (crossProd[m_currentAxis] > 0)
						angle = -angle;
					m_rotationAmount += angle;
					
					float roundedRotationAmount;
					if (dragIncrementRadians > 0)
						roundedRotationAmount = std::round(m_rotationAmount / dragIncrementRadians) * dragIncrementRadians;
					else
						roundedRotationAmount = m_rotationAmount;
					
					glm::vec3 rotationAxis(0);
					rotationAxis[m_currentAxis] = 1;
					rotation = glm::angleAxis(roundedRotationAmount, rotationAxis) * m_initialRotation;
					
					m_previousDragVector = toNewPos;
				}
			}
		}
		
		m_hoveredAxis = -1;
		float minIntersectDist = INFINITY;
		for (int axis = 0; axis < 3; axis++)
		{
			if (onlyAxis != -1 && axis != onlyAxis)
				continue;
			
			const glm::mat4 worldTransform = GetAxisTransform(position, m_renderScale, axis);
			std::optional<float> intersect = RayIntersectGizmoMesh(worldTransform, viewRay, TORUS_VERTICES, TORUS_INDICES);
			if (intersect.has_value() && *intersect < minIntersectDist)
			{
				minIntersectDist = *intersect;
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
		
		m_onlyAxisToDraw = onlyAxis;
		if (m_currentAxis != -1)
			m_onlyAxisToDraw = m_currentAxis;
		
		m_lastPosition = position;
	}
	
	void RotationGizmo::Draw(const glm::mat4& viewProjMatrix) const
	{
		DC.BindPipeline(gizmoPipeline);
		DC.BindVertexBuffer(0, s_torusVB, 0);
		DC.BindIndexBuffer(IndexType::UInt16, s_torusIB, 0);
		
		for (int axis = 0; axis < 3; axis++)
		{
			if (m_onlyAxisToDraw != -1 && axis != m_onlyAxisToDraw)
				continue;
			
			DrawGizmoAxis(axis, m_currentAxis, m_hoveredAxis, std::size(TORUS_INDICES),
			              viewProjMatrix * GetAxisTransform(m_lastPosition, m_renderScale, axis));
		}
	}
}
