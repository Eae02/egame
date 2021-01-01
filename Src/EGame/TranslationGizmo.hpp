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
		TranslationGizmo();
		
		void Update(glm::vec3& position, const glm::vec3& cameraPos, const glm::mat4& viewProjMatrix, const Ray& viewRay);
		
		void Draw(const glm::mat4& viewProjMatrix) const;
		
		inline bool HasInputFocus() const
		{
			return m_currentAxis != -1;
		}
		
		float SizeScale() const
		{
			return m_sizeScale;
		}
		
		void SetSizeScale(float sizeScale)
		{
			m_sizeScale = sizeScale;
		}
		
		int CurrentAxis() const
		{
			return m_currentAxis;
		}
		
		static void InitStatic();
		static void DestroyStatic();
		
	private:
		glm::vec3 m_lastPosition;
		int m_axisDrawOrder[3];
		
		float m_sizeScale = 0.1f;
		float m_renderScale;
		
		int m_currentAxis = -1;
		int m_hoveredAxis = -1;
		Ray m_axisDragRay;
		float m_initialDragDist = 0;
		
		bool m_keyboardSelectingAxis = false;
	};
}
