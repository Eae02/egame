#pragma once

#include <yaml-cpp/yaml.h>

#include <cstdlib>

namespace eg
{
size_t HashYAMLNode(const YAML::Node& node);
}
