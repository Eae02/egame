#pragma once

#ifdef EG_HAS_YAML_CPP
#include <yaml-cpp/yaml.h>
#else
namespace YAML { struct Node; }
#endif

#include <cstdlib>

namespace eg
{
	size_t HashYAMLNode(const YAML::Node& node);
}
