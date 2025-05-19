#pragma once

#include <limits>
#include <string>
#include <sys/signalfd.h>

#include "proactor/handler.hpp"
#include "utils/aliases.hpp"
#include "utils/demangled_name.hpp"

namespace Sage
{

enum class EventType : std::uint32_t
{
    Timeout = 1,
    TimeoutCancel,
    Signal,
    TcpConnect,
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
    explicit Event(EventType type) noexcept : m_type{ type } {}

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
    explicit TimeoutEvent(Handler::Id handlerId) : Event{ EventType::Timeout }, m_handlerId{ handlerId } {}

    Handler::Id m_handlerId;
};

class TimeoutCancelEvent final : public Event
{
public:
    explicit TimeoutCancelEvent(Handler::Id handlerId) : Event{ EventType::TimeoutCancel }, m_handlerId{ handlerId } {}

    Handler::Id m_handlerId;
};

class SignalEvent final : public Event
{
public:
    SignalEvent(int sig, int sigFd) : Event{ EventType::Signal }, m_signal{ sig }, m_signalFd{ sigFd } {}

    int m_signal;
    int m_signalFd;
    signalfd_siginfo m_signalReadBuff{};
};

class TcpConnect final : public Event
{
public:
    TcpConnect(Handler::Id handlerId, const std::string& host, const std::string& port) :
        Event{ EventType::TcpConnect },
        m_handlerId{ handlerId },
        m_host{ host },
        m_port{ port }
    {
    }

    Handler::Id m_handlerId;
    std::string m_host;
    std::string m_port;
    int m_fd{ -1 };
};

} // namespace Sage
