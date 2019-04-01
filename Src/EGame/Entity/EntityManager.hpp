#pragma once

#include <typeindex>

#include "Entity.hpp"
#include "EntitySet.hpp"
#include "EntitySignature.hpp"
#include "IEntitySerializer.hpp"
#include "../Span.hpp"

namespace eg
{
	class EG_API EntityManager
	{
	public:
		static EntityManager* New();
		static void Delete(EntityManager* manager);
		
		static EntityManager* FromManagerId(uint32_t id);
		
		void EndFrame();
		
		void Serialize(std::ostream& stream) const;
		
		static EntityManager* Deserialize(std::istream& stream, Span<const IEntitySerializer*> serializers);
		
		Entity& AddEntity(const EntitySignature& signature, Entity* parent = nullptr,
			const IEntitySerializer* serializer = nullptr);
		
		const EntitySet& GetEntitySet(const EntitySignature& signature);
		
		const Entity* FromEntityId(uint32_t id) const
		{
			return PrivFromEntityId(id);
		}
		
		Entity* FromEntityId(uint32_t id)
		{
			return PrivFromEntityId(id);
		}
		
	private:
		friend class Entity;
		
		explicit EntityManager(uint32_t managerId)
			: m_managerId(managerId) { }
		
		Entity* PrivFromEntityId(uint32_t id) const;
		
		static std::vector<EntityManager*> s_globalManagersList;
		
		uint32_t m_managerId;
		
		struct EntityPage
		{
			uint32_t nextParity = 0;
			uint8_t availIndices[256];
			Entity entities[256];
		};
		
		struct EntityPageOuter
		{
			uint32_t numAvailable = 256;
			std::unique_ptr<EntityPage> page;
		};
		
		std::array<EntityPageOuter, 256> m_pages;
		
		ComponentAllocator m_componentAllocator;
		
		std::vector<EntitySet> m_entitySets;
		
		std::vector<EntityHandle> m_despawnQueue;
	};
	
	struct EntityManagerDeleter
	{
		void operator()(EntityManager* em) const
		{
			EntityManager::Delete(em);
		}
	};
	
	using EntityManagerUP = std::unique_ptr<EntityManager, EntityManagerDeleter>;
}
