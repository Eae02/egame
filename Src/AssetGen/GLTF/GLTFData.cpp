#include "GLTFData.hpp"

#include <stdexcept>

namespace eg::asset_gen::gltf
{
	ElementType GLTFData::ParseElementType(std::string_view name)
	{
		if (name == "SCALAR")
			return ElementType::SCALAR;
		if (name == "VEC2")
			return ElementType::VEC2;
		if (name == "VEC3")
			return ElementType::VEC3;
		if (name == "VEC4")
			return ElementType::VEC4;
		if (name == "MAT4")
			return ElementType::MAT4;
		throw std::runtime_error("Invalid element type");
	}
}
