#pragma once

#include <functional>
#include <liburing.h>
#include <limits>
#include <string>
#include <sys/signalfd.h>

#include "proactor/handle.hpp"
#include "proactor/io_uring.hpp"
#include "utils/demangled_name.hpp"

namespace Sage
{

using EventId = size_t;

class Event
{
public:
    using OnCompleteFunc = std::move_only_function<void(Event&, const io_uring_cqe&)>;

    virtual ~Event() noexcept = default;

    std::string NameAndType() const { return DemangleTypeName(*this); }

    const EventId m_id{ NextId() };
    const Handle::Id m_handlerId;
    OnCompleteFunc m_onCompleteCb;
    bool m_removeOnComplete{ true };

protected:
    Event(Handle::Id handlerId, OnCompleteFunc&& onComplete) noexcept :
        m_handlerId{ handlerId },
        m_onCompleteCb{ std::move(onComplete) }
    {
    }

private:
    Event() = delete;

    static EventId NextId() noexcept
    {
        static EventId id{ 0 };
        id = std::min(id + 1, std::numeric_limits<EventId>::max() - 1);
        return id;
    }
};

class TimerExpiredEvent final : public Event
{
public:
    TimerExpiredEvent(Handle::Id handlerId, OnCompleteFunc&& onComplete) : Event{ handlerId, std::move(onComplete) }
    {
        m_removeOnComplete = false;
    }
};

class TimerUpdateEvent final : public Event
{
public:
    TimerUpdateEvent(Handle::Id handlerId, OnCompleteFunc&& onComplete) : Event{ handlerId, std::move(onComplete) } {}
};

class TimerCancelEvent final : public Event
{
public:
    TimerCancelEvent(Handle::Id handlerId, OnCompleteFunc&& onComplete) : Event{ handlerId, std::move(onComplete) } {}
};

class SignalEvent final : public Event
{
public:
    SignalEvent(int sig, int sigFd, OnCompleteFunc&& onComplete) :
        Event{ 0, std::move(onComplete) },
        m_signal{ sig },
        m_signalFd{ sigFd }
    {
    }

    int m_signal;
    int m_signalFd;
    signalfd_siginfo m_signalReadBuff{};
};

class TcpConnect final : public Event
{
public:
    TcpConnect(Handle::Id handlerId, OnCompleteFunc&& onComplete, const std::string& host, const std::string& port) :
        Event{ handlerId, std::move(onComplete) },
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
    TcpSend(
        Handle::Id handlerId, OnCompleteFunc&& onComplete, const std::string& host, const std::string& port, int fd,
        std::string data
    ) :
        Event{ handlerId, std::move(onComplete) },
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
    TcpRecv(
        Handle::Id handlerId, OnCompleteFunc&& onComplete, const std::string& host, const std::string& port, int fd
    ) :
        Event{ handlerId, std::move(onComplete) },
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
