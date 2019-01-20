#include "PerspectiveProjection.hpp"
#include "AbstractionHL.hpp"

namespace eg
{
	void PerspectiveProjection::Update()
	{
		if (GraphicsCaps().depthRange == DepthRange::ZeroToOne)
		{
			m_matrix = glm::perspectiveFovZO(m_fieldOfViewRad, m_aspectRatio, 1.0f, m_zNear, m_zFar);
		}
		else
		{
			m_matrix = glm::perspectiveFovNO(m_fieldOfViewRad, m_aspectRatio, 1.0f, m_zNear, m_zFar);
		}
		m_inverseMatrix = glm::inverse(m_matrix);
	}
}
