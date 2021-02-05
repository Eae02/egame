#pragma once

#include "API.hpp"
#include "InputState.hpp"
#include "Ray.hpp"
#include "Graphics/AbstractionHL.hpp"

namespace eg
{
	class EG_API TranslationGizmo
	{
	public:
		TranslationGizmo()
			: m_axisDrawOrder{ 0, 1, 2 } { }
		
		void Update(glm::vec3& position, const glm::vec3& cameraPos, const glm::mat4& viewProjMatrix, const Ray& viewRay);
		
		void Draw(const glm::mat4& viewProjMatrix) const;
		
		bool HasInputFocus() const
		{
			return m_currentAxis != -1;
		}
		
		bool IsHovered() const
		{
			return m_hoveredAxis != -1;
		}
		
		int CurrentAxis() const
		{
			return m_currentAxis;
		}
		
		static void Initialize();
		static void Destroy();
		
		float size = 0.1f;
		
	private:
		glm::vec3 m_lastPosition;
		int m_axisDrawOrder[3];
		
		float m_renderScale = 1;
		
		int m_currentAxis = -1;
		int m_hoveredAxis = -1;
		Ray m_axisDragRay;
		float m_initialDragDist = 0;
		
		bool m_keyboardSelectingAxis = false;
	};
}
