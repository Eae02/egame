#pragma once

#include <mutex>
#include <cstdint>
#include <atomic>
#include <typeindex>

#include "InputState.hpp"
#include "Utils.hpp"
#include "API.hpp"

#define EG_ON_INIT(callback) static eg::detail::CallbackNodeSetter EG_CONCAT(_onInit, __LINE__) { &callback, &eg::detail::onInit };
#define EG_ON_SHUTDOWN(callback) static eg::detail::CallbackNodeSetter EG_CONCAT(_onShutdown, __LINE__) { &callback, &eg::detail::onShutdown };

namespace eg
{
	namespace detail
	{
		struct CallbackNode
		{
			void (*callback)();
			CallbackNode* next;
		};
		
		EG_API extern CallbackNode* onInit;
		EG_API extern CallbackNode* onShutdown;
		
		struct CallbackNodeSetter
		{
			CallbackNode node;
			EG_API CallbackNodeSetter(void (*callback)(), CallbackNode** firstNode);
		};
	}
	
	struct ResolutionChangedEvent
	{
		int newWidth;
		int newHeight;
	};
	
	struct ButtonEvent
	{
		Button button;
		bool newState;
		bool isRepeat;
	};
	
	struct TextCompositionEvent
	{
		std::string text;
	};
	
	struct RelativeMouseModeLostEvent { };
	
	constexpr size_t EVENT_PAGE_SIZE = 512;
	constexpr size_t MAX_TRAIL_DIST = EVENT_PAGE_SIZE - 32;
	
	struct EventPage
	{
		std::type_index type;
		std::atomic_uint64_t position;
		char* events;
		
		EventPage(const std::type_index& _type, char* _events)
			: type(_type), position(0), events(_events) { }
	};
	
	namespace detail
	{
		EG_API EventPage* GetEventPage(std::type_index typeIndex);
		
		EG_API void DefineEventType(std::type_index typeIndex, size_t typeSize, size_t typeAlignment);
	}
	
	template <typename T>
	void DefineEventType()
	{
		detail::DefineEventType(std::type_index(typeid(T)), sizeof(T), alignof(T));
	}
	
	template <typename T>
	inline void RaiseEvent(T&& event)
	{
		EventPage* page = detail::GetEventPage(std::type_index(typeid(T)));
		const size_t index = (page->position++) % EVENT_PAGE_SIZE;
		new (page->events + sizeof(T) * index) T (std::forward<T>(event));
	}
	
	template <typename T>
	class EventListener
	{
	public:
		EventListener()
			: m_page(detail::GetEventPage(std::type_index(typeid(T)))), m_position(m_page->position.load()) { }
		
		template <typename Callback>
		void ProcessAll(Callback callback)
		{
			while (ProcessOne(callback)) { }
		}
		
		template <typename Callback>
		bool ProcessLast(Callback callback)
		{
			uint64_t maxPos = m_page->position.load();
			if (m_position >= maxPos)
				return false;
			
			const size_t idx = (maxPos + EVENT_PAGE_SIZE - 1) % EVENT_PAGE_SIZE;
			callback(reinterpret_cast<const T*>(m_page->events)[idx]);
			m_position = maxPos;
			return true;
		}
		
		template <typename Callback>
		bool ProcessOne(Callback callback)
		{
			uint64_t maxPos = m_page->position.load();
			uint64_t minPos = std::max<uint64_t>(maxPos, MAX_TRAIL_DIST) - MAX_TRAIL_DIST;
			if (m_position >= maxPos)
				return false;
			if (m_position < minPos)
				m_position = minPos;
			
			callback(reinterpret_cast<const T*>(m_page->events)[m_position % EVENT_PAGE_SIZE]);
			m_position++;
			return true;
		}
		
	private:
		EventPage* m_page;
		uint64_t m_position = 0;
	};
}
