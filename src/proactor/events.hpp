#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <sys/signalfd.h>

#include "proactor/handler.hpp"
#include "proactor/io_uring.hpp"
#include "utils/demangled_name.hpp"

namespace Sage
{

using EventId = size_t;

enum class EventType : uint32_t
{
    TimerExpired = 1,
    TimerUpdate,
    TimerCancel,
    Signal,
    TcpConnect,
    TcpSend,
    TcpRecv,
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
    const Handler::Id m_handlerId;

protected:
    Event(Handler::Id handlerId, EventType type) noexcept : m_handlerId{ handlerId }, m_type{ type } {}

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

class TimerExpiredEvent final : public Event
{
public:
    explicit TimerExpiredEvent(Handler::Id handlerId) : Event{ handlerId, EventType::TimerExpired } {}
};

class TimerUpdateEvent final : public Event
{
public:
    explicit TimerUpdateEvent(Handler::Id handlerId) : Event{ handlerId, EventType::TimerUpdate } {}
};

class TimerCancelEvent final : public Event
{
public:
    explicit TimerCancelEvent(Handler::Id handlerId) : Event{ handlerId, EventType::TimerCancel } {}
};

class SignalEvent final : public Event
{
public:
    SignalEvent(int sig, int sigFd) : Event{ 0, EventType::Signal }, m_signal{ sig }, m_signalFd{ sigFd } {}

    int m_signal;
    int m_signalFd;
    signalfd_siginfo m_signalReadBuff{};
};

class TcpConnect final : public Event
{
public:
    TcpConnect(Handler::Id handlerId, const std::string& host, const std::string& port) :
        Event{ handlerId, EventType::TcpConnect },
        m_host{ host },
        m_port{ port }
    {
    }

    std::string m_host;
    std::string m_port;
    int m_fd{ -1 };
};

class TcpSend final : public Event
{
public:
    TcpSend(Handler::Id handlerId, const std::string& host, const std::string& port, int fd, std::string data) :
        Event{ handlerId, EventType::TcpSend },
        m_host{ host },
        m_port{ port },
        m_fd{ fd },
        m_data{ std::move(data) }
    {
    }

    std::string m_host;
    std::string m_port;
    int m_fd;
    std::string m_data;
};

class TcpRecv final : public Event
{
public:
    TcpRecv(Handler::Id handlerId, const std::string& host, const std::string& port, int fd) :
        Event{ handlerId, EventType::TcpRecv },
        m_host{ host },
        m_port{ port },
        m_fd{ fd }
    {
    }

    std::string m_host;
    std::string m_port;
    int m_fd;
    IOURing::RxBuffer m_data;
};

} // namespace Sage
