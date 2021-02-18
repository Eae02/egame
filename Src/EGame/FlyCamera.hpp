#pragma once

#include "API.hpp"
#include <glm/glm.hpp>

namespace eg
{
	class EG_API FlyCamera
	{
	public:
		void Update(float dt);
		
		const glm::vec3& Position() const
		{
			return m_position;
		}
		
		const glm::vec3& Velocity() const
		{
			return m_velocity;
		}
		
		const glm::vec3& Forward() const
		{
			return m_forward;
		}
		
		const glm::mat4& ViewMatrix() const
		{
			return m_viewMatrix;
		}
		
		const glm::mat4& InverseViewMatrix() const
		{
			return m_invViewMatrix;
		}
		
		void SetView(const glm::vec3& position, const glm::vec3& lookAt);
		
	private:
		float m_yaw = 0.0f;
		float m_pitch = 0.0f;
		
		glm::vec3 m_position;
		glm::vec3 m_velocity;
		glm::vec3 m_forward;
		
		glm::mat4 m_viewMatrix;
		glm::mat4 m_invViewMatrix;
	};
}
