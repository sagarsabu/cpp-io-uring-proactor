#include <cstring>
#include <csignal>
#include <sys/signalfd.h>

#include "log/logger.hpp"
#include "proactor/proactor.hpp"
#include "proactor/timer_handler.hpp"
#include "timing/scoped_deadline.hpp"

namespace Sage
{

std::shared_ptr<Proactor> Proactor::Create()
{
    return std::shared_ptr<Proactor>{ new Proactor };
}

void Proactor::StartAllHandlers()
{
    for (auto handler : m_timerHandlers)
    {
        RequestContinuousTimeout(*(handler.second));
    }
}

void Proactor::Run()
{
    AttachExitHandlers();
    StartAllHandlers();

    m_running = true;

    while (m_running)
    {
        UniqueUringCEvent cEvent{ m_ioURing.WaitForEvent() };
        if (cEvent == nullptr)
        {
            continue;
        }

        size_t userData{ cEvent->user_data };
        auto itr{ m_pendingEvents.find(userData) };
        if (itr == m_pendingEvents.end())
        {
            LOG_ERROR("%s failed to find event for user-data=%ld",
                __func__, userData);
            continue;
        }

        auto& event{ itr->second };
        LOG_DEBUG("%s got event=%s", __func__, event->NameAndType().c_str());

        HandleEvent(*event, *cEvent);

        // dont remove the continuously firing timer
        if (event->Type() != EventType::Timeout)
        {
            m_pendingEvents.erase(itr);
        }
    }
}

void Proactor::AddTimerHandler(TimerHandler& handler)
{
    if (m_timerHandlers.contains(handler.m_id))
    {
        LOG_ERROR("%s [%s] handler already in collection", __func__, handler.Name());
        return;
    }

    m_timerHandlers[handler.m_id] = &handler;

    if (m_running)
    {
        StartTimerHandler(handler);
    }
}

void Proactor::StartTimerHandler(TimerHandler& handler)
{
    if (not m_timerHandlers.contains(handler.m_id))
    {
        LOG_CRITICAL("%s [%s] handler not in collection", __func__, handler.Name());
        return;
    }

    RequestContinuousTimeout(handler);
}

void Proactor::RequestContinuousTimeout(TimerHandler& handler)
{
    auto event{ std::make_unique<TimeoutEvent>(handler.m_id) };
    auto eventId{ event->m_id };

    if (auto data{ static_cast<IOURing::UserData>(event->m_id) };
        not m_ioURing.QueueTimeoutEvent(data, handler.m_period))
    {
        LOG_ERROR("%s [%s] kick failed", __func__, handler.Name());
        return;
    }

    m_pendingEvents[eventId] = std::move(event);
    LOG_DEBUG("%s [%s] handler kicked eventId(%ld)", __func__, handler.Name(), eventId);
}

void Proactor::RemoveTimerHandler(TimerHandler& handler)
{
    auto itr = m_timerHandlers.find(handler.m_id);
    if (itr == m_timerHandlers.end())
    {
        LOG_ERROR("%s [%s] handler not in collection", __func__, handler.Name());
        return;
    }

    LOG_INFO("%s [%s] handler removed", __func__, handler.Name());

    RequestTimeoutCancel(handler);
}

void Proactor::AttachExitHandlers()
{
    constexpr std::array exitSignals{ SIGINT, SIGQUIT, SIGTERM };
    auto handler = [this](const signalfd_siginfo& info)
    {
        switch (info.ssi_signo)
        {
            case SIGINT:
            case SIGQUIT:
            case SIGTERM:
            {
                LOG_INFO("SignalHandler received shutdown signal %s(%d)",
                    strsignal(info.ssi_signo), info.ssi_signo);
                m_running = false;
                break;
            }

            default:
            {
                LOG_CRITICAL("SignalHandler received unexpected signal %s(%d). aborting!",
                    strsignal(info.ssi_signo), info.ssi_signo);
                std::abort();
            }
        }
    };

    for (auto sig : exitSignals)
    {
        AddSignalHandler(sig, handler);
    }
}

void Proactor::AddSignalHandler(int sig, SignalHandleFunc&& func)
{
    if (auto itr{ m_signalHandlers.find(sig) }; itr != m_signalHandlers.end())
    {
        LOG_ERROR("%s signal %s(%d) already attached", __func__, strsignal(sig), sig);
        throw std::runtime_error{ "Signal Already Attached" };
    }

    sigset_t sigToAdd{};
    sigemptyset(&sigToAdd);
    sigaddset(&sigToAdd, sig);

    if (sigprocmask(SIG_BLOCK, &sigToAdd, nullptr) == -1)
    {
        int err{ errno };
        LOG_ERROR("%s signal %s(%d) could not be blocked. %s", __func__, strsignal(sig), sig, strerror(err));
        throw std::runtime_error{ "Signal Could Not Be Blocked" };
    }

    int fd{ signalfd(-1, &sigToAdd, 0) };
    if (fd == -1)
    {
        int err{ errno };
        LOG_ERROR("%s signal fd creation failed %s(%d). %s", __func__, strsignal(sig), sig, strerror(err));
        throw std::runtime_error{ "SignalFd Failed" };
    }

    m_signalHandlers[sig] = std::make_unique<SignalHandleData>(fd, sig, func);
    // m_signalHandlers.emplace(sig, SignalHandleData{ .m_fd = fd, .m_signal = sig, .m_callback = func });

    if (not RequestSignalRead(sig, fd))
    {
        throw std::runtime_error{ "RequestSignalRead Failed" };
    }
}

void Proactor::RequestTimeoutCancel(const TimerHandler& handler)
{
    for (auto& pending : m_pendingEvents)
    {
        if (pending.second->Type() != EventType::Timeout)
        {
            continue;
        }

        auto& timerExpireEvent{ static_cast<TimeoutEvent&>(*pending.second) };
        // cancel only the requested handler
        if (timerExpireEvent.m_handlerId != handler.m_id)
        {
            continue;
        }

        std::unique_ptr<TimeoutCancelEvent> cancelEvent{ std::make_unique<TimeoutCancelEvent>(handler.m_id) };
        // the timeout user data must be the same as the inital timeout used data
        IOURing::UserData currentTimerUserData{ timerExpireEvent.m_id };
        IOURing::UserData userData{ cancelEvent->m_id };
        if (not m_ioURing.CancelTimeoutEvent(userData, currentTimerUserData))
        {
            LOG_ERROR("%s [%s] cancel failed", __func__, handler.Name());
            continue;
        }

        m_pendingEvents[userData] = std::move(cancelEvent);
        LOG_DEBUG("%s [%s] handler cancel triggered eventId(%ld)",
            __func__, handler.Name(), timerExpireEvent.m_id);
    }
}

bool Proactor::RequestSignalRead(int signal, int signalFd)
{
    auto event{ std::make_unique<SignalEvent>(signal, signalFd) };

    IOURing::UserData userData{ event->m_id };
    if (not m_ioURing.QueueSignalRead(userData, event->m_signalFd, event->m_signalReadBuff))
    {
        LOG_ERROR("%s signal queue read failed for %s(%d)", __func__, strsignal(signal), signal);
        return false;
    }

    LOG_DEBUG("%s signal %s(%d) read queued", __func__, strsignal(signal), signal);
    m_pendingEvents[userData] = std::move(event);

    return true;
}

void Proactor::HandleEvent(Event& event, const io_uring_cqe& cEvent)
{
    switch (event.Type())
    {
        case EventType::Timeout:
            HandleTimeoutEvent(static_cast<TimeoutEvent&>(event), cEvent);
            break;

        case EventType::TimeoutCancel:
            HandleTimeoutCancelEvent(static_cast<TimeoutCancelEvent&>(event), cEvent);
            break;

        case EventType::Signal:
            HandleSignalEvent(static_cast<SignalEvent&>(event), cEvent);
            break;

        default:
        {
            LOG_ERROR("%s received unknown event=%s", __func__, event.NameAndType().c_str());
            break;
        }
    }
}

void Proactor::HandleTimeoutEvent(TimeoutEvent& event, const io_uring_cqe& cEvent)
{
    int eventRes{ cEvent.res };

    auto itr{ m_timerHandlers.find(event.m_handlerId) };
    if (itr == m_timerHandlers.end())
    {
        LOG_ERROR("%s failed to find handler for eventId(%ld) handlerId(%ld)",
            __func__, event.m_id, event.m_handlerId);
        return;
    }

    auto& handler{ *itr->second };

    switch (eventRes)
    {
        // timer expired
        case -ETIME:
        {
            LOG_DEBUG("%s [%s] triggering handler eventId(%ld)",
                __func__, handler.Name(), event.m_id);

            {
                ScopedDeadline dl{ "TimerHandler:" + handler.NameStr(), 20ms };
                handler.OnTimerExpired();
            }

            break;
        }

        // timer cancelled
        case -ECANCELED:
        {
            LOG_DEBUG("%s [%s] timer cancelled eventId(%ld)",
                __func__, handler.Name(), event.m_id);
            m_timerHandlers.erase(itr);
            break;
        }

        default:
        {
            LOG_ERROR("%s failed eventId(%ld) res(%d) %s",
                __func__, event.m_id, eventRes, strerror(-eventRes));
            break;
        }
    }
}

void Proactor::HandleTimeoutCancelEvent(TimeoutCancelEvent& event, const io_uring_cqe& cEvent)
{
    int eventRes{ cEvent.res };
    auto itr{ m_timerHandlers.find(event.m_handlerId) };
    if (itr == m_timerHandlers.end())
    {
        LOG_ERROR("%s failed to find handler for eventId(%ld) handlerId(%ld)",
            __func__, event.m_id, event.m_handlerId);
        return;
    }

    auto& handler{ *itr->second };

    switch (eventRes)
    {
        // timer cancellation acknowledged
        case 0:
        {
            LOG_DEBUG("%s [%s] timer cancellation acknowledged eventId(%ld)",
                __func__, handler.Name(), event.m_id);
            break;
        }

        default:
        {
            LOG_ERROR("%s failed eventId(%ld) res(%d) %s",
                __func__, event.m_id, eventRes, strerror(-eventRes));
            break;
        }
    }
}

void Proactor::HandleSignalEvent(SignalEvent& event, const io_uring_cqe& cEvent)
{
    int res{ cEvent.res };
    if (res < 0)
    {
        LOG_ERROR("%s read failed for signal %s(%d). %s",
            __func__, strsignal(event.m_signal), event.m_signal, strerror(-res));
        return;
    }

    if (res != sizeof(signalfd_siginfo))
    {
        LOG_ERROR("%s read failed for signal %s(%d). expected-read-size(%ld) actual-read-size(%d)",
            __func__, strsignal(event.m_signal), event.m_signal, sizeof(signalfd_siginfo), res);
        return;
    }

    auto itr{ m_signalHandlers.find(event.m_signal) };
    if (itr == m_signalHandlers.end())
    {
        LOG_ERROR("%s could not find signal %s(%d) in handler map",
            __func__, strsignal(event.m_signal), event.m_signal);
        return;
    }

    const auto& [sig, data] { *itr };

    LOG_INFO("%s invoking signal handler for signal %s(%d)",
        __func__, strsignal(sig), sig);

    (data->m_callback)(event.m_signalReadBuff);

    if (not RequestSignalRead(data->m_signal, data->m_fd))
    {
        LOG_CRITICAL("%s failed to request signal %s(%d) read",
            __func__, strsignal(event.m_signal), event.m_signal);
    }
}

} // namespace Sage
