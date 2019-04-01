#pragma once

#include <string_view>

#include "../API.hpp"
#include "Entity.hpp"

namespace eg
{
	class EG_API IEntitySerializer
	{
	public:
		virtual std::string_view GetName() const = 0;
		virtual void Serialize(const class Entity& entity, std::ostream& stream) const = 0;
		virtual void Deserialize(class EntityManager& entityManager, std::istream& stream) const = 0;
	};
}
