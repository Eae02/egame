#pragma once

#include "../API.hpp"
#include "../Geometry/Ray.hpp"
#include "../Graphics/AbstractionHL.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace eg
{
class EG_API RotationGizmo
{
public:
	RotationGizmo() = default;

	void Update(
		glm::quat& rotation, const glm::vec3& position, const glm::vec3& cameraPos, const glm::mat4& viewProjMatrix,
		const Ray& viewRay);

	void Draw(const glm::mat4& viewProjMatrix) const;

	bool HasInputFocus() const { return m_currentAxis != -1; }

	bool IsHovered() const { return m_hoveredAxis != -1; }

	int CurrentAxis() const { return m_currentAxis; }

	float size = 0.1f;
	int onlyAxis = -1;
	float dragIncrementRadians = 0;

	static void Initialize();
	static void Destroy();

private:
	glm::vec3 m_lastPosition;

	float m_renderScale = 1;

	int m_currentAxis = -1;
	int m_hoveredAxis = -1;
	int m_onlyAxisToDraw = -1;
	glm::vec3 m_previousDragVector;
	glm::quat m_initialRotation;
	float m_rotationAmount = 0;

	bool m_keyboardSelectingAxis = false;
};
} // namespace eg
