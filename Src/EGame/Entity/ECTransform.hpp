#pragma once

#include "../API.hpp"
#include "Entity.hpp"

namespace eg
{
	struct ECPosition3D
	{
		glm::vec3 position { 0.0f };
		
		ECPosition3D() = default;
		ECPosition3D(const glm::vec3& _position) : position(_position) { }
		ECPosition3D(float x, float y, float z) : position(x, y, z) { }
	};
	
	struct ECScale3D
	{
		glm::vec3 scale { 1.0f };
		
		ECScale3D() = default;
		ECScale3D(const glm::vec3& _scale) : scale(_scale) { }
		ECScale3D(float x, float y, float z) : scale(x, y, z) { }
	};
	
	struct ECRotation3D
	{
		glm::quat rotation;
		
		ECRotation3D() = default;
		ECRotation3D(const glm::quat& _rotation) : rotation(_rotation) { }
	};
	
	EG_API glm::mat4 GetEntityTransform3D(const Entity& entity);
	
	inline glm::vec3 GetEntityPosition(const Entity& entity)
	{
		return glm::vec3(GetEntityTransform3D(entity)[3]);
	}
}
