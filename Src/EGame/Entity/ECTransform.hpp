#pragma once

#include "../API.hpp"
#include "Entity.hpp"

namespace eg
{
	struct ECPosition3D
	{
		glm::vec3 position { 0.0f };
	};
	
	struct ECScale3D
	{
		glm::vec3 scale { 1.0f };
	};
	
	struct ECRotation3D
	{
		glm::quat rotation;
	};
	
	EG_API glm::mat4 GetEntityTransform3D(const Entity& entity);
	
	inline glm::vec3 GetEntityPosition(const Entity& entity)
	{
		return glm::vec3(GetEntityTransform3D(entity)[3]);
	}
}
