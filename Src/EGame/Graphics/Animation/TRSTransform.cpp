#include "TRSTransform.hpp"

namespace eg
{
glm::mat4 TRSTransform::GetMatrix() const
{
	return glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
}

TRSTransform TRSTransform::Interpolate(const TRSTransform& other, float t) const
{
	TRSTransform result;
	result.translation = glm::mix(translation, other.translation, t);
	result.scale = glm::mix(scale, other.scale, t);
	result.rotation = glm::slerp(rotation, other.rotation, t);
	return result;
}
} // namespace eg
