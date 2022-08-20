#include "TranslationGizmo.hpp"
#include "GizmoCommon.hpp"
#include "../Geometry/Plane.hpp"
#include "../InputState.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <span>
#include <algorithm>

namespace eg
{
	static Buffer s_arrowVB;
	static Buffer s_arrowIB;
	
	static const glm::vec3 ARROW_OFFSET = glm::vec3(0.2f, 0.0f, 0.0f);
	static const glm::vec3 ARROW_SCALE = glm::vec3(0.8f, 0.6f, 0.6f);
	
	void TranslationGizmo::Initialize()
	{
		for (uint16_t& i : detail::ARROW_INDICES)
			i--;
		s_arrowVB = eg::Buffer(BufferFlags::VertexBuffer, sizeof(detail::ARROW_VERTICES), detail::ARROW_VERTICES);
		s_arrowIB = eg::Buffer(BufferFlags::IndexBuffer, sizeof(detail::ARROW_INDICES), detail::ARROW_INDICES);
		
		s_arrowVB.UsageHint(BufferUsage::VertexBuffer);
		s_arrowIB.UsageHint(BufferUsage::IndexBuffer);
		
		detail::InitializeGizmoPipeline();
	}
	
	void TranslationGizmo::Destroy()
	{
		s_arrowVB.Destroy();
		s_arrowIB.Destroy();
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
			if (detail::RayIntersectGizmoMesh(worldMatrix, viewRay, detail::ARROW_VERTICES, detail::ARROW_INDICES))
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
	
	void TranslationGizmo::Draw(const glm::mat4& viewProjMatrix) const
	{
		DC.BindPipeline(detail::gizmoPipeline);
		DC.BindVertexBuffer(0, s_arrowVB, 0);
		DC.BindIndexBuffer(IndexType::UInt16, s_arrowIB, 0);
		
		for (int axis : m_axisDrawOrder)
		{
			detail::DrawGizmoAxis(axis, m_currentAxis, m_hoveredAxis, std::size(detail::ARROW_INDICES),
			                      viewProjMatrix * GetAxisTransform(m_lastPosition, m_renderScale, axis));
		}
	}
}
