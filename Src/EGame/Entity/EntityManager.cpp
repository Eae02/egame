#include "EntityManager.hpp"
#include "../IOUtils.hpp"
#include "../Log.hpp"

#include <mutex>

namespace eg
{
	static std::mutex s_globalManagersMutex;
	std::vector<EntityManager*> EntityManager::s_globalManagersList;
	static uint16_t s_nextManagerParity = 0;
	
	EntityManager* EntityManager::New()
	{
		std::lock_guard<std::mutex> lock(s_globalManagersMutex);
		
		uint32_t index = s_globalManagersList.size();
		for (uint32_t i = 0; i < s_globalManagersList.size(); i++)
		{
			if (s_globalManagersList[i] == nullptr)
			{
				index = i;
				break;
			}
		}
		if (index == s_globalManagersList.size())
		{
			s_globalManagersList.push_back(nullptr);
		}
		
		uint32_t id = index | ((uint32_t)(s_nextManagerParity++) << 16);
		return s_globalManagersList[index] = new EntityManager(id);
	}
	
	void EntityManager::Delete(EntityManager* manager)
	{
		if (manager == nullptr)
			return;
		std::lock_guard<std::mutex> lock(s_globalManagersMutex);
		
		uint32_t index = manager->m_managerId & 0xFFFF;
		if (s_globalManagersList[index] == nullptr || s_globalManagersList[index]->m_managerId != manager->m_managerId)
		{
			EG_PANIC("Double delete of entity manager detected!");
		}
		
		s_globalManagersList[index] = nullptr;
		delete manager;
	}
	
	EntityManager* EntityManager::FromManagerId(uint32_t id)
	{
		uint32_t index = id & 0xFFFF;
		if (index >= s_globalManagersList.size())
			return nullptr;
		if (s_globalManagersList[index] == nullptr || s_globalManagersList[index]->m_managerId != id)
			return nullptr;
		return s_globalManagersList[index];
	}
	
	Entity& EntityManager::AddEntity(const EntitySignature& signature, Entity* parent,
		const IEntitySerializer* serializer)
	{
		//Searches for an available page
		uint32_t page = 0;
		while (page < m_pages.size() && m_pages[page].numAvailable == 0)
			page++;
		
		if (page == m_pages.size())
		{
			EG_PANIC("Too many entities!");
		}
		
		//Initializes the next page if none was available
		if (m_pages[page].page == nullptr)
		{
			m_pages[page].page = std::make_unique<EntityPage>();
			for (uint32_t i = 0; i < 256; i++)
				m_pages[page].page->availIndices[i] = 255 - i;
		}
		
		m_pages[page].numAvailable--;
		uint32_t index = m_pages[page].page->availIndices[m_pages[page].numAvailable];
		
		uint32_t parity = m_pages[page].page->nextParity++;
		
		//Initializes the entity
		Entity& entity = m_pages[page].page->entities[index];
		uint32_t id = page | (index << 8) | (parity << 16);
		entity.Initialize(m_managerId, id, signature, m_componentAllocator, serializer);
		
		if (parent != nullptr)
		{
			if (parent->ManagerId() != m_managerId)
			{
				EG_PANIC("Entity parent must be from the same entity manager");
			}
			parent->AddChild(entity);
		}
		
		for (EntitySet& set : m_entitySets)
		{
			set.MaybeAdd(entity);
		}
		
		return entity;
	}
	
	Entity* EntityManager::PrivFromEntityId(uint32_t id) const
	{
		uint32_t page = id & 0xFF;
		if (m_pages[page].page == nullptr)
			return nullptr;
		
		Entity& entity = m_pages[page].page->entities[(id >> 8) & 0xFF];
		if (entity.ManagerId() == UINT32_MAX || entity.Id() != id)
			return nullptr;
		
		return &entity;
	}
	
	const EntitySet& EntityManager::GetEntitySet(const EntitySignature& signature)
	{
		for (const EntitySet& entitySet : m_entitySets)
		{
			if (entitySet.Signature() == signature)
				return entitySet;
		}
		
		EntitySet& entitySet = m_entitySets.emplace_back(signature);
		for (const EntityPageOuter& page : m_pages)
		{
			if (page.page == nullptr)
				break;
			for (size_t i = 0; i < 256; i++)
			{
				if (page.page->entities[i].ManagerId() != UINT32_MAX)
				{
					entitySet.MaybeAdd(page.page->entities[i]);
				}
			}
		}
		return entitySet;
	}
	
	void EntityManager::EndFrame()
	{
		if (m_despawnQueue.empty())
			return;
		
		for (EntityHandle entityHandle : m_despawnQueue)
		{
			uint32_t page = entityHandle.Id() & 0xFF;
			if (m_pages[page].page == nullptr)
				continue;
			
			uint32_t index = (entityHandle.Id() >> 8) & 0xFF;
			Entity& entity = m_pages[page].page->entities[index];
			if (entity.ManagerId() == UINT32_MAX || entity.Id() != entityHandle.Id())
				continue;
			
			entity.Uninitialize();
			m_pages[page].page->availIndices[m_pages[page].numAvailable++] = index;
		}
		m_despawnQueue.clear();
		
		for (EntitySet& entitySet : m_entitySets)
		{
			entitySet.RemoveDead();
		}
	}
	
	void EntityManager::Serialize(std::ostream& stream) const
	{
		//Counts the number of entities to be serialized
		uint32_t numEntities = 0;
		for (const EntityPageOuter& page : m_pages)
		{
			if (page.page == nullptr)
				break;
			for (size_t i = 0; i < 256; i++)
			{
				const Entity& entity = page.page->entities[i];
				if (entity.ManagerId() != UINT32_MAX && entity.Serializer() != nullptr)
					numEntities++;
			}
		}
		
		BinWrite(stream, numEntities);
		
		//Serializes entities
		std::ostringstream tempStream;
		for (const EntityPageOuter& page : m_pages)
		{
			if (page.page == nullptr)
				break;
			for (size_t i = 0; i < 256; i++)
			{
				const Entity& entity = page.page->entities[i];
				if (entity.ManagerId() != UINT32_MAX && entity.Serializer() != nullptr)
				{
					BinWrite(stream, HashFNV1a32(entity.Serializer()->GetName()));
					
					tempStream.str({ });
					entity.Serializer()->Serialize(entity, tempStream);
					
					std::string serializedStr = tempStream.str();
					BinWrite<uint32_t>(stream, serializedStr.size());
					stream.write(serializedStr.data(), serializedStr.size());
				}
			}
		}
	}
	
	EntityManager* EntityManager::Deserialize(std::istream& stream, Span<const IEntitySerializer*> serializers)
	{
		//Initializes the serializer map
		std::vector<std::pair<uint32_t, const IEntitySerializer*>> serializerMap;
		for (const IEntitySerializer* serializer : serializers)
		{
			serializerMap.emplace_back(HashFNV1a32(serializer->GetName()), serializer);
		}
		std::sort(serializerMap.begin(), serializerMap.end());
		
		EntityManager* manager = EntityManager::New();
		
		//Deserializes entities
		std::vector<char> readBuffer;
		uint32_t numEntities = BinRead<uint32_t>(stream);
		for (uint32_t i = 0; i < numEntities; i++)
		{
			uint32_t serializerHash = BinRead<uint32_t>(stream);
			uint32_t bytes = BinRead<uint32_t>(stream);
			readBuffer.resize(bytes);
			stream.read(readBuffer.data(), bytes);
			
			auto it = std::lower_bound(serializerMap.begin(), serializerMap.end(),
				std::make_pair(serializerHash, (const IEntitySerializer*)nullptr));
			if (it->first != serializerHash)
			{
				eg::Log(eg::LogLevel::Error, "ecs", "Failed to find entity serializer with hash {0}", serializerHash);
				continue;
			}
			
			MemoryStreambuf streambuf(readBuffer);
			std::istream deserializeStream(&streambuf);
			it->second->Deserialize(*manager, deserializeStream);
		}
		
		return manager;
	}
}
