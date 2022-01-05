#include "YAMLUtils.hpp"
#include "../Utils.hpp"

namespace eg
{
	size_t HashYAMLNode(const YAML::Node& node)
	{
#ifdef EG_HAS_YAML_CPP
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
#else
		return 0;
#endif
	}
}