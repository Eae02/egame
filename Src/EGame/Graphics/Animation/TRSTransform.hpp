#pragma once

#include <glm/gtc/quaternion.hpp>

namespace eg
{
	struct TRSTransform
	{
		glm::vec3 translation = glm::vec3(0);
		glm::vec3 scale = glm::vec3(1);
		glm::quat rotation = glm::quat(1, 0, 0, 0);
		
		TRSTransform Interpolate(const TRSTransform& other, float t) const;
		
		glm::mat4 GetMatrix() const;
	};
}
