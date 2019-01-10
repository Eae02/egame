#include "Event.hpp"
#include "Utils.hpp"

#include <map>
#include <cstdlib>

namespace eg
{
	static std::vector<EventPage*> pages;
	
	inline bool EventPageTypeLess(const EventPage* a, const std::type_index& b)
	{
		return a->type < b;
	}
	
	EventPage* detail::GetEventPage(std::type_index typeIndex)
	{
		auto it = std::lower_bound(pages.begin(), pages.end(), typeIndex, EventPageTypeLess);
		
		if (it == pages.end() || (*it)->type != typeIndex)
		{
			EG_PANIC("Undefined event type!");
		}
		
		return *it;
	}
	
	void detail::DefineEventType(std::type_index typeIndex, size_t typeSize, size_t typeAlignment)
	{
		auto it = std::lower_bound(pages.begin(), pages.end(), typeIndex, EventPageTypeLess);
		if (it != pages.end() && (*it)->type == typeIndex)
			return;
		
		size_t eventsOffset = RoundToNextMultiple(sizeof(EventPage), typeAlignment);
		char* memory = reinterpret_cast<char*>(std::malloc(eventsOffset + typeSize * EVENT_PAGE_SIZE));
		
		EventPage* page = new (memory) EventPage (typeIndex, memory + eventsOffset);
		pages.insert(it, page);
	}
}
