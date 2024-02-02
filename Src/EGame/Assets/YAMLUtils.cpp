#include "YAMLUtils.hpp"
#include "../Hash.hpp"

namespace eg
{
size_t HashYAMLNode(const YAML::Node& node)
{
	if (!node.IsDefined())
		return 0;
	size_t hash = std::hash<std::string>()(node.Tag());
	if (node.IsSequence() || node.IsMap())
	{
		for (YAML::const_iterator it = node.begin(); it != node.end(); ++it)
		{
			HashAppend(hash, HashYAMLNode(it->first));
			HashAppend(hash, HashYAMLNode(it->second));
		}
	}
	else
	{
		HashAppend(hash, node.as<std::string>());
	}
	return hash;
}
} // namespace eg
