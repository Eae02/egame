#include "Vec3Generator.hpp"
#include "../../Utils.hpp"

#include <ostream>
#include <istream>

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
	
	void SphereVec3Generator::Read(std::istream& stream)
	{
		float data[4];
		stream.read(reinterpret_cast<char*>(data), sizeof(data));
		sphere = Sphere({ data[0], data[1], data[2] }, data[3]);
	}
	
	void SphereVec3Generator::Write(std::ostream& stream) const
	{
		float data[4] = { sphere.position.x, sphere.position.y, sphere.position.z, sphere.radius };
		stream.write(reinterpret_cast<const char*>(data), sizeof(data));
	}
}
