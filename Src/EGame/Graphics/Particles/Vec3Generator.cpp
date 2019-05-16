#include "Vec3Generator.hpp"

namespace eg
{
	static std::uniform_real_distribution<float> twoPiDist(0, TWO_PI);
	static std::uniform_real_distribution<float> negOneToOneDist(-1, 1);
	
	glm::vec3 SphereVec3Generator::operator()(std::mt19937& rand) const
	{
		float t = twoPiDist(rand);
		float u = negOneToOneDist(rand);
		float v = std::sqrt(1.0f - u * u);
		return sphere.position + glm::vec3(v * std::cos(t), v * std::sin(t), u) * sphere.radius;
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
