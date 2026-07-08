module;

#include <string>	
#include "EventMacros.h"	

export module ResourceReprocessedEvent;

import EventBase;

export class ResourceReprocessedEvent : public EventBase
{
public:
    DEFINE_EVENT_TYPE(ResourceReprocessedEvent, static_cast<int>(EventCategory::Resource))

    
    ResourceReprocessedEvent(std::string InResourceID) : ResourceID(std::move(InResourceID)) {}

    const std::string& GetResourceID() const { return ResourceID; }

private:
    std::string ResourceID;
};