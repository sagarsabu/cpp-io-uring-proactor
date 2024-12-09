#pragma once

#include <limits>
#include <sys/signalfd.h>

#include "utils/aliases.hpp"
#include "utils/demangled_name.hpp"

namespace Sage
{

enum class EventType : std::uint32_t
{
    Timeout = 1,
    TimeoutCancel,
    Signal,
};

class Event
{
public:
    virtual ~Event() noexcept = default;

    EventType Type() const noexcept { return m_type; }

    std::string NameAndType() const
    {
        const std::string className{ DemangleTypeName(*this) };
        const std::string eventType{ std::to_string(static_cast<int>(Type())) };
        return className + '(' + eventType + ')';
    }

    const EventId m_id{ NextId() };

protected:
    explicit Event(EventType type) noexcept : m_type{ type }
    { }

private:
    Event() = delete;

    static EventId NextId() noexcept
    {
        static EventId id{ 0 };
        id = std::min(id + 1, std::numeric_limits<EventId>::max() - 1);
        return id;
    }

    const EventType m_type;
};

class TimeoutEvent final : public Event
{
public:
    explicit TimeoutEvent(size_t handlerId) :
        Event{ EventType::Timeout },
        m_handlerId{ handlerId }
    { }

    const size_t m_handlerId;
};

class TimeoutCancelEvent final : public Event
{
public:
    explicit TimeoutCancelEvent(size_t handlerId) :
        Event{ EventType::TimeoutCancel },
        m_handlerId{ handlerId }
    { }

    const size_t m_handlerId;
};

class SignalEvent final : public Event
{
public:
    SignalEvent(int sig, int sigFd) :
        Event{ EventType::Signal },
        m_signal{ sig },
        m_signalFd{ sigFd }
    { }

    const int m_signal;
    const int m_signalFd;
    signalfd_siginfo m_signalReadBuff{};
};

} // namespace Sage
