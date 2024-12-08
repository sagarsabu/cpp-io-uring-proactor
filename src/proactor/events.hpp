#pragma once

#include <limits>
#include "utils/aliases.hpp"
#include "utils/demangled_name.hpp"

namespace Sage
{

enum class EventType : std::uint32_t
{
    Timeout = 1,
    TimeoutCancel,
};

struct Event
{
    virtual EventType Type() const noexcept = 0;

    std::string NameAndType() const
    {
        const std::string className{ DemangleTypeName(*this) };
        const std::string eventType{ std::to_string(static_cast<int>(Type())) };
        return className + '(' + eventType + ')';
    }

    const EventId m_id{ NextId() };

private:
    static EventId NextId() noexcept
    {
        static EventId id{ 0 };
        id = std::min(id + 1, std::numeric_limits<EventId>::max() - 1);
        return id;
    }
};

struct TimeoutEvent final : public Event
{
    explicit TimeoutEvent(size_t handlerId) : m_handlerId{ handlerId }
    { }

    EventType Type() const noexcept override { return EventType::Timeout; }

    const size_t m_handlerId;
};

struct TimeoutCancelEvent final : public Event
{
    explicit TimeoutCancelEvent(size_t handlerId) : m_handlerId{ handlerId }
    { }

    EventType Type() const noexcept override { return EventType::TimeoutCancel; }

    const size_t m_handlerId;
};

} // namespace Sage
