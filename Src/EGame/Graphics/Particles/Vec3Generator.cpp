#include "Vec3Generator.hpp"
#include "../../IOUtils.hpp"
#include "../../Utils.hpp"

namespace eg
{
static std::uniform_real_distribution<float> twoPiDist(0, TWO_PI);
static std::uniform_real_distribution<float> zeroOneDist(0, 1);

glm::vec3 SphereVec3Generator::operator()(std::mt19937& rand) const
{
	float theta = twoPiDist(rand);
	float phi = std::acos(zeroOneDist(rand) * 2 - 1);
	float r = std::cbrt(zeroOneDist(rand));
	float sinTheta = std::sin(theta);
	float cosTheta = std::cos(theta);
	float sinPhi = std::sin(phi);
	float cosPhi = std::cos(phi);
	return sphere.position + (r * sphere.radius) * glm::vec3(sinPhi * cosTheta, sinPhi * sinTheta, cosPhi);
}

void SphereVec3Generator::Read(MemoryReader& reader)
{
	sphere.position.x = reader.Read<float>();
	sphere.position.y = reader.Read<float>();
	sphere.position.z = reader.Read<float>();
	sphere.radius = reader.Read<float>();
}

void SphereVec3Generator::Write(MemoryWriter& writer) const
{
	writer.Write(sphere.position.x);
	writer.Write(sphere.position.y);
	writer.Write(sphere.position.z);
	writer.Write(sphere.radius);
}
} // namespace eg
