#pragma once

#include "EntityManager.hpp"

namespace eg
{
	/**
	 * An entity component which provides dynamic dispatch.
	 */
	template <typename... Args> 
	class ECDynamicInvoke
	{
	public:
		using CallbackType = void(*)(Entity&, Args...);
		
		explicit ECDynamicInvoke(CallbackType callback = nullptr)
			: m_callback(callback) { }
		
		CallbackType Callback() const
		{
			return m_callback;
		}
		
		void SetCallback(CallbackType callback)
		{
			m_callback = callback;
		}
		
		void operator()(Entity& entity, Args... args)
		{
			m_callback(entity, args...);
		}
		
	private:
		CallbackType m_callback = nullptr;
	};
	
	template <typename EC, typename... Args>
	void EntitiesInvoke(EntityManager& entityManager, Args... args)
	{
		static EntitySignature signature = EntitySignature::Create<EC>();
		for (Entity& entity : entityManager.GetEntitySet(signature))
		{
			entity.FindComponent<EC>()->operator()(entity, args...);
		}
	}
}
