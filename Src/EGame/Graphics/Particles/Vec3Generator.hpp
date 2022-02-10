#pragma once

#include "../../API.hpp"
#include "../../Geometry/Sphere.hpp"

#include <random>
#include <variant>

namespace eg
{
	struct EG_API SphereVec3Generator
	{
		Sphere sphere;
		
		static constexpr uint32_t TYPE = 0;
		
		glm::vec3 operator()(std::mt19937& rand) const;
		
		void Read(std::istream& stream);
		void Write(std::ostream& stream) const;
		
		SphereVec3Generator() = default;
		explicit SphereVec3Generator(const Sphere& _sphere)
			: sphere(_sphere) { }
	};
	
	using Vec3Generator = std::variant<SphereVec3Generator>;
}
