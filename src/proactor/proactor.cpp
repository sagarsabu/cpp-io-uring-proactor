#include <csignal>
#include <cstring>
#include <sys/signalfd.h>

#include "log/logger.hpp"
#include "proactor/proactor.hpp"
#include "proactor/timer_handler.hpp"
#include "timing/scoped_deadline.hpp"

namespace Sage
{

std::shared_ptr<Proactor> Proactor::Create()
{
    s_instance = std::shared_ptr<Proactor>{ new Proactor };
    return s_instance;
}

void Proactor::Destroy() { s_instance = nullptr; }

Proactor::Proactor() { LOG_INFO("proactor created"); }

Proactor::~Proactor() { LOG_INFO("proactor deleted"); }

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
            LOG_ERROR("failed to find event for user-data={}", userData);
            continue;
        }

        auto& event{ itr->second };
        LOG_DEBUG("got event={}", event->NameAndType());

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
        LOG_ERROR("[{}] handler already in collection", handler.Name());
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
        LOG_CRITICAL("[{}] handler not in collection", handler.Name());
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
        LOG_ERROR("[{}] kick failed", handler.Name());
        return;
    }

    m_pendingEvents[eventId] = std::move(event);
    LOG_DEBUG("[{}] handler kicked eventId({})", handler.Name(), eventId);
}

void Proactor::RemoveTimerHandler(TimerHandler& handler)
{
    auto itr = m_timerHandlers.find(handler.m_id);
    if (itr == m_timerHandlers.end())
    {
        LOG_ERROR("[{}] handler not in collection", handler.Name());
        return;
    }

    LOG_INFO("[{}] handler removed", handler.Name());

    RequestTimeoutCancel(handler);
}

void Proactor::AttachExitHandlers()
{
    constexpr std::array exitSignals{ SIGINT, SIGQUIT, SIGTERM };
    auto handler = [this](const signalfd_siginfo& info)
    {
        const char* sigStr{ strsignal(static_cast<int>(info.ssi_signo)) };
        switch (info.ssi_signo)
        {
            case SIGINT:
            case SIGQUIT:
            case SIGTERM:
            {
                LOG_INFO("SignalHandler received shutdown signal {}({})", sigStr, info.ssi_signo);
                m_running = false;
                break;
            }

            default:
            {
                LOG_CRITICAL("SignalHandler received unexpected signal {}({}). aborting!", sigStr, info.ssi_signo);
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
        LOG_ERROR("signal {}({}) already attached", strsignal(sig), sig);
        throw std::runtime_error{ "Signal Already Attached" };
    }

    sigset_t sigToAdd{};
    sigemptyset(&sigToAdd);
    sigaddset(&sigToAdd, sig);

    if (sigprocmask(SIG_BLOCK, &sigToAdd, nullptr) == -1)
    {
        int err{ errno };
        LOG_ERROR("signal {}({}) could not be blocked. {}", strsignal(sig), sig, strerror(err));
        throw std::runtime_error{ "Signal Could Not Be Blocked" };
    }

    int fd{ signalfd(-1, &sigToAdd, 0) };
    if (fd == -1)
    {
        int err{ errno };
        LOG_ERROR("signal fd creation failed {}({}). {}", strsignal(sig), sig, strerror(err));
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
            LOG_ERROR("[{}] cancel failed", handler.Name());
            continue;
        }

        m_pendingEvents[userData] = std::move(cancelEvent);
        LOG_DEBUG("[{}] handler cancel triggered eventId({})", handler.Name(), timerExpireEvent.m_id);
    }
}

bool Proactor::RequestSignalRead(int signal, int signalFd)
{
    auto event{ std::make_unique<SignalEvent>(signal, signalFd) };

    IOURing::UserData userData{ event->m_id };
    if (not m_ioURing.QueueSignalRead(userData, event->m_signalFd, event->m_signalReadBuff))
    {
        LOG_ERROR("signal queue read failed for {}({})", strsignal(signal), signal);
        return false;
    }

    LOG_DEBUG("signal {}({}) read queued", strsignal(signal), signal);
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
            LOG_ERROR("received unknown event={}", event.NameAndType().c_str());
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
        LOG_ERROR("failed to find handler for eventId({}) handlerId({})", event.m_id, event.m_handlerId);
        return;
    }

    auto& handler{ *itr->second };

    switch (eventRes)
    {
        // timer expired
        case -ETIME:
        {
            LOG_DEBUG("[{}] triggering handler eventId({})", handler.Name(), event.m_id);

            {
                ScopedDeadline dl{ "TimerHandler:" + handler.NameStr(), 20ms };
                handler.OnTimerExpired();
            }

            break;
        }

        // timer cancelled
        case -ECANCELED:
        {
            LOG_DEBUG("[{}] timer cancelled eventId({})", handler.Name(), event.m_id);
            m_timerHandlers.erase(itr);
            break;
        }

        default:
        {
            LOG_ERROR("failed eventId({}) res({}) {}", event.m_id, eventRes, strerror(-eventRes));
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
        LOG_ERROR("failed to find handler for eventId({}) handlerId({})", event.m_id, event.m_handlerId);
        return;
    }

    auto& handler{ *itr->second };

    switch (eventRes)
    {
        // timer cancellation acknowledged
        case 0:
        {
            LOG_DEBUG("[{}] timer cancellation acknowledged eventId({})", handler.Name(), event.m_id);
            break;
        }

        default:
        {
            LOG_ERROR("failed eventId({}) res({}) {}", event.m_id, eventRes, strerror(-eventRes));
            break;
        }
    }
}

void Proactor::HandleSignalEvent(SignalEvent& event, const io_uring_cqe& cEvent)
{
    int res{ cEvent.res };
    if (res < 0)
    {
        LOG_ERROR("read failed for signal {}({}). {}", strsignal(event.m_signal), event.m_signal, strerror(-res));
        return;
    }

    if (res != sizeof(signalfd_siginfo))
    {
        LOG_ERROR(
            "read failed for signal {}({}). expected-read-size({}) actual-read-size({})",
            strsignal(event.m_signal),
            event.m_signal,
            sizeof(signalfd_siginfo),
            res
        );
        return;
    }

    auto itr{ m_signalHandlers.find(event.m_signal) };
    if (itr == m_signalHandlers.end())
    {
        LOG_ERROR("could not find signal {}({}) in handler map", strsignal(event.m_signal), event.m_signal);
        return;
    }

    const auto& [sig, data]{ *itr };

    LOG_INFO("invoking signal handler for signal {}({})", strsignal(sig), sig);

    (data->m_callback)(event.m_signalReadBuff);

    if (not RequestSignalRead(data->m_signal, data->m_fd))
    {
        LOG_CRITICAL("failed to request signal {}({}) read", strsignal(event.m_signal), event.m_signal);
    }
}

} // namespace Sage
