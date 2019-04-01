#include "ECTransform.hpp"
#include "Entity.hpp"

namespace eg
{
	glm::mat4 GetEntityTransform3D(const Entity& entity)
	{
		glm::mat4 transform(1.0f);
		if (const ECPosition3D* pos3D = entity.GetComponent<ECPosition3D>())
			transform = glm::translate(transform, pos3D->position);
		if (const ECRotation3D* rot3D = entity.GetComponent<ECRotation3D>())
			transform *= glm::mat4_cast(rot3D->rotation);
		if (const ECScale3D* scale3D = entity.GetComponent<ECScale3D>())
			transform = glm::translate(transform, scale3D->scale);
		
		if (const Entity* parent = entity.Parent())
			return GetEntityTransform3D(*parent) * transform;
		else
			return transform;
	}
}
