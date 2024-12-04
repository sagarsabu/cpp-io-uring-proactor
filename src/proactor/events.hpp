#pragma once

#include <limits>

namespace Sage
{

enum class EventType
{
    Timeout = 1,
};

struct Event
{
    virtual EventType Type() const = 0;

    const size_t m_id{ NextId() };

private:
    static size_t NextId()
    {
        static size_t id{ 0 };
        id = std::min(id + 1, std::numeric_limits<size_t>::max() - 1);
        return id;
    }
};

struct TimeoutEvent final : public Event
{
    explicit TimeoutEvent(size_t handlerId) : m_handlerId{ handlerId }
    { }

    EventType Type() const override { return EventType::Timeout; }

    const size_t m_handlerId;
};

} // namespace Sage
