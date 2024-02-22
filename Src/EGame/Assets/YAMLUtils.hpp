#pragma once

#include <cstdlib>

namespace YAML
{
class Node;
};

namespace eg
{
size_t HashYAMLNode(const YAML::Node& node);
}
