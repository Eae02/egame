#include "FlyCamera.hpp"
#include "InputState.hpp"
#include "Utils.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace eg
{
	static constexpr float ACCEL_AMOUNT = 20.0f;
	static constexpr float DRAG_PER_SEC = 5.0f;
	static constexpr float MOUSE_SENSITIVITY = 0.01f;
	static constexpr float MAX_ROLL = 0.4f;
	
	void FlyCamera::Update(float dt)
	{
		const float rotateX = static_cast<float>(CursorDeltaX()) * MOUSE_SENSITIVITY;
		const float rotateY = static_cast<float>(CursorDeltaY()) * MOUSE_SENSITIVITY;
		m_yaw += rotateX;
		m_pitch = glm::clamp(m_pitch + rotateY, -HALF_PI, HALF_PI);
		
		glm::mat4 invRotation =
			glm::rotate(glm::mat4(), m_pitch, glm::vec3(1, 0, 0)) *
			glm::rotate(glm::mat4(), m_yaw, glm::vec3(0, 1, 0));
		glm::mat4 rotation = glm::transpose(invRotation);
		
		m_forward = -glm::vec3(rotation[2]);
		
		int forceX = 0;
		int forceZ = 0;
		if (IsButtonDown(Button::W))
			forceZ++;
		if (IsButtonDown(Button::S))
			forceZ--;
		if (IsButtonDown(Button::A))
			forceX--;
		if (IsButtonDown(Button::D))
			forceX++;
		
		const int forceLenSq = forceX * forceX + forceZ * forceZ;
		if (forceLenSq > 0)
		{
			float forceScale = dt * ACCEL_AMOUNT / std::sqrt(static_cast<float>(forceLenSq));
			m_velocity += m_forward * (static_cast<float>(forceZ) * forceScale);
			m_velocity += glm::vec3(rotation[0]) * (static_cast<float>(forceX) * forceScale);
		}
		
		m_velocity -= m_velocity * std::min(dt * DRAG_PER_SEC, 1.0f);
		
		m_position += m_velocity * dt * speed;
		
		m_viewMatrix = invRotation * glm::translate(glm::mat4(1.0f), -m_position);
		m_invViewMatrix = glm::translate(glm::mat4(1.0f), m_position) * rotation;
	}
	
	void FlyCamera::SetView(const glm::vec3& position, const glm::vec3& lookAt)
	{
		m_position = position;
		m_velocity = glm::vec3(0.0f);
		
		glm::vec3 look = glm::normalize(lookAt - position);
		m_pitch = -std::asin(look.y);
		m_yaw = std::atan2(look.z, look.x) + HALF_PI;
	}
}
