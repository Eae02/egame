#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "../Inc/Common.hpp"

#include "../Src/EGame/Entity/EntityManager.hpp"
#include "../Src/EGame/Entity/EntitySignature.hpp"
#include "EGame/Entity/ECTransform.hpp"

using namespace eg;

std::set<Entity*> CollectEntities(const EntitySet& set)
{
	std::set<Entity*> ans;
	for (Entity& entity : set)
	{
		ans.insert(&entity);
	}
	return ans;
}

TEST_CASE("ECS", "[EntityManager]")
{
	EntityManager* entityManager = EntityManager::New();
	
	EntitySignature sigPR = EntitySignature::Create<ECPosition3D, ECRotation3D>();
	EntitySignature sigPRS = EntitySignature::Create<ECPosition3D, ECRotation3D, ECScale3D>();
	EntitySignature sigRSP = EntitySignature::Create<ECRotation3D, ECScale3D, ECPosition3D>();
	
	SECTION("Signature subsets")
	{
		REQUIRE(sigPR.IsSubsetOf(sigPRS));
		REQUIRE(sigPR.IsSubsetOf(sigRSP));
		REQUIRE(sigRSP.IsSubsetOf(sigPRS));
		REQUIRE(sigPRS.IsSubsetOf(sigRSP));
		REQUIRE(!sigPRS.IsSubsetOf(sigPR));
		REQUIRE(!sigRSP.IsSubsetOf(sigPR));
		REQUIRE(EntitySignature().IsSubsetOf(sigPRS));
	}
	
	SECTION("Signature equality")
	{
		REQUIRE(sigPR != sigPRS);
		REQUIRE(sigPR != sigRSP);
		REQUIRE(sigRSP == sigPRS);
	}
	
	SECTION("Add & get components")
	{
		Entity& entity1 = entityManager->AddEntity(sigPR);
		Entity& entity2 = entityManager->AddEntity(sigRSP);
		REQUIRE(&entity1 != &entity2);
		REQUIRE(entity1.FindComponent<ECPosition3D>() != nullptr);
		REQUIRE(entity1.FindComponent<ECRotation3D>() != nullptr);
		REQUIRE(entity1.FindComponent<ECScale3D>() == nullptr);
		REQUIRE(entity2.FindComponent<ECPosition3D>() != nullptr);
		REQUIRE(entity2.FindComponent<ECRotation3D>() != nullptr);
		REQUIRE(entity2.FindComponent<ECScale3D>() != nullptr);
	}
	
	SECTION("Add and list components")
	{
		Entity& entity1 = entityManager->AddEntity(sigPR);
		Entity& entity2 = entityManager->AddEntity(sigRSP);
		Entity& entity3 = entityManager->AddEntity(sigPR, &entity2);
		Entity& entity4 = entityManager->AddEntity(sigPRS);
		
		auto setPR = CollectEntities(entityManager->GetEntitySet(sigPR));
		REQUIRE(setPR.size() == 4);
		REQUIRE(setPR.find(&entity1) != setPR.end());
		REQUIRE(setPR.find(&entity2) != setPR.end());
		REQUIRE(setPR.find(&entity3) != setPR.end());
		REQUIRE(setPR.find(&entity4) != setPR.end());
		
		auto setPRS = CollectEntities(entityManager->GetEntitySet(sigPRS));
		REQUIRE(setPRS.size() == 2);
		REQUIRE(setPRS.find(&entity1) == setPRS.end());
		REQUIRE(setPRS.find(&entity2) != setPRS.end());
		REQUIRE(setPRS.find(&entity3) == setPRS.end());
		REQUIRE(setPRS.find(&entity4) != setPRS.end());
		
		entity2.Despawn();
		entityManager->EndFrame();
		
		setPR = CollectEntities(entityManager->GetEntitySet(sigPR));
		REQUIRE(setPR.size() == 2);
		REQUIRE(setPR.find(&entity1) != setPR.end());
		REQUIRE(setPR.find(&entity4) != setPR.end());
		
		setPRS = CollectEntities(entityManager->GetEntitySet(sigPRS));
		REQUIRE(setPRS.size() == 1);
		REQUIRE(setPRS.find(&entity1) == setPRS.end());
		REQUIRE(setPRS.find(&entity4) != setPRS.end());
	}
	
	EntityManager::Delete(entityManager);
}
