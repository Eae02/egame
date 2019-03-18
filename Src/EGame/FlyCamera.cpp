#include "FlyCamera.hpp"
#include "InputState.hpp"

namespace eg
{
	static constexpr float ACCEL_AMOUNT = 20.0f;
	static constexpr float DRAG_PER_SEC = 5.0f;
	static constexpr float MOUSE_SENSITIVITY = 0.01f;
	static constexpr float MAX_ROLL = 0.4f;
	
	void FlyCamera::Update(float dt)
	{
		const float rotateX = -CursorDeltaX() * MOUSE_SENSITIVITY;
		const float rotateY = -CursorDeltaY() * MOUSE_SENSITIVITY;
		m_yaw += rotateX;
		m_pitch = glm::clamp(m_pitch + rotateY, HALF_PI, -HALF_PI);
		
		glm::mat4 invRotation = glm::rotate(glm::mat4(), m_yaw, glm::vec3(0, 1, 0)) *
	       glm::rotate(glm::mat4(), m_pitch, glm::vec3(1, 0, 0));;
		
		int forceX = 0;
		int forceZ = 0;
		if (IsButtonDown(Button::W))
			forceZ--;
		if (IsButtonDown(Button::S))
			forceZ++;
		if (IsButtonDown(Button::A))
			forceX--;
		if (IsButtonDown(Button::D))
			forceX++;
		
		const int forceLenSq = forceX * forceX + forceZ * forceZ;
		if (forceLenSq > 0)
		{
			float forceScale = dt * ACCEL_AMOUNT / std::sqrt((float)forceLenSq);
			m_velocity += glm::vec3(invRotation[2]) * (forceZ * forceScale);
			m_velocity += glm::vec3(invRotation[0]) * (forceX * forceScale);
		}
		
		m_velocity -= m_velocity * std::min(dt * DRAG_PER_SEC, 1.0f);
		
		m_position += m_velocity * dt;
		
		m_viewMatrix = invRotation * glm::translate(glm::mat4(1.0f), -m_position);
		m_invViewMatrix = glm::translate(glm::mat4(1.0f), m_position) * glm::transpose(invRotation);
	}
}
