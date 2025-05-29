#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstring>
#include <sys/signalfd.h>
#include <unistd.h>
#include <utility>

#include "log/logger.hpp"
#include "proactor/events.hpp"
#include "proactor/proactor.hpp"
#include "proactor/tcp_client.hpp"
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
    for (auto [_, handler] : m_timerHandlers)
    {
        RequestTimerContinuous(*handler);
    }

    for (auto [_, handler] : m_tcpClients)
    {
        RequestTcpConnect(*handler);
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

        CompleteEvent(*event, *cEvent);

        // dont remove the continuously firing timer
        if (event->Type() != EventType::TimerExpired)
        {
            m_pendingEvents.erase(itr);
        }
    }
}

void Proactor::AddTimerHandler(TimerHandler& handler)
{
    if (m_timerHandlers.contains(handler.m_id))
    {
        LOG_ERROR("[{}] timer handler already in collection", handler.Name());
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

    RequestTimerContinuous(handler);
}

void Proactor::UpdateTimerHandler(TimerHandler& handler)
{
    if (not m_timerHandlers.contains(handler.m_id))
    {
        LOG_CRITICAL("[{}] handler not in collection", handler.Name());
        return;
    }

    RequestTimerUpdate(handler);
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

    RequestTimerCancel(handler);
}

void Proactor::AddSocketClient(TcpClient& handler)
{
    if (m_tcpClients.contains(handler.m_id))
    {
        LOG_ERROR("[{}] handler already in collection", handler.Name());
        return;
    }

    m_tcpClients[handler.m_id] = &handler;

    if (m_running)
    {
        StartSocketClient(handler);
    }
}

void Proactor::StartSocketClient(TcpClient& handler)
{
    if (not m_tcpClients.contains(handler.m_id))
    {
        LOG_CRITICAL("[{}] handler not in collection", handler.Name());
        return;
    }

    RequestTcpConnect(handler);
}

void Proactor::RemoveSocketClient(TcpClient& handler)
{
    auto itr = m_tcpClients.find(handler.m_id);
    if (itr == m_tcpClients.end())
    {
        LOG_ERROR("[{}] handler not in collection", handler.Name());
        return;
    }

    LOG_INFO("[{}] handler removed", handler.Name());

    m_tcpClients.erase(itr);
}

void Proactor::RequestTimerContinuous(TimerHandler& handler)
{
    auto event{ std::make_unique<TimerExpiredEvent>(handler.m_id) };
    auto eventId{ event->m_id };

    if (auto data{ static_cast<IOURing::UserData>(event->m_id) };
        not m_ioURing.QueueTimeoutEvent(data, handler.m_period))
    {
        LOG_ERROR("[{}] kick failed", handler.Name());
        return;
    }

    m_pendingEvents[eventId] = std::move(event);
    LOG_DEBUG("[{}] timer kicked eventId({})", handler.Name(), eventId);
}

void Proactor::RequestTimerUpdate(TimerHandler& handler)
{
    const auto timerExpireEvent{ FindPendingEvent(handler.m_id, EventType::TimerExpired) };
    if (timerExpireEvent == nullptr)
    {
        LOG_ERROR("[{}] failed to find pending expiry timer for handler {}", handler.Name(), handler.m_id);
        return;
    }

    std::unique_ptr<TimerUpdateEvent> updateEvent{ std::make_unique<TimerUpdateEvent>(handler.m_id) };
    // the timeout user data must be the same as the inital timeout used data
    IOURing::UserData currentTimerUserData{ timerExpireEvent->m_id };
    IOURing::UserData userData{ updateEvent->m_id };
    if (not m_ioURing.UpdateTimeoutEvent(userData, currentTimerUserData, handler.m_period))
    {
        LOG_ERROR("[{}] update timer failed", handler.Name());
        return;
    }

    m_pendingEvents[userData] = std::move(updateEvent);
    LOG_INFO(
        "[{}] timer update triggered eventId({}) new-timeout: {}",
        handler.Name(),
        timerExpireEvent->m_id,
        std::chrono::duration_cast<TimeMS>(handler.m_period)
    );
}

void Proactor::RequestTimerCancel(TimerHandler& handler)
{
    const auto timerExpireEvent{ FindPendingEvent(handler.m_id, EventType::TimerExpired) };
    if (timerExpireEvent == nullptr)
    {
        LOG_ERROR("[{}] failed to find pending expiry timer for handler {}", handler.Name(), handler.m_id);
        return;
    }

    std::unique_ptr<TimerCancelEvent> cancelEvent{ std::make_unique<TimerCancelEvent>(handler.m_id) };
    // the timeout user data must be the same as the inital timeout used data
    IOURing::UserData currentTimerUserData{ timerExpireEvent->m_id };
    IOURing::UserData userData{ cancelEvent->m_id };
    if (not m_ioURing.CancelTimeoutEvent(userData, currentTimerUserData))
    {
        LOG_ERROR("[{}] cancel failed", handler.Name());
        return;
    }

    m_pendingEvents[userData] = std::move(cancelEvent);
    LOG_DEBUG("[{}] handler cancel triggered eventId({})", handler.Name(), timerExpireEvent->m_id);
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

    if (::signal(SIGPIPE, SIG_IGN) != 0)
    {
        int err{ errno };
        LOG_CRITICAL("failed to ignore sigpipe. e: {}", strerror(err));
        std::exit(1);
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

void Proactor::RequestTcpConnect(TcpClient& handler)
{
    handler.m_state = TcpClient::Broken;

    auto event{ std::make_unique<TcpConnect>(handler.m_id, handler.m_host, handler.m_port) };
    IOURing::UserData userData{ event->m_id };
    event->m_fd = m_ioURing.QueueTcpConnect(userData, event->m_host, event->m_port);
    if (event->m_fd == -1)
    {
        LOG_ERROR("net connect queue failed for '{}:{}'", handler.m_host, handler.m_port);
        return;
    }

    handler.m_state = TcpClient::Connecting;
    LOG_DEBUG("net connect queued for '{}:{}'", handler.m_host, handler.m_port);
    m_pendingEvents[userData] = std::move(event);
}

void Proactor::RequestTcpSend(TcpClient& handler, std::string data)
{
    auto event{
        std::make_unique<TcpSend>(handler.m_id, handler.m_host, handler.m_port, handler.m_fd, std::move(data))
    };
    IOURing::UserData userData{ event->m_id };

    if (not m_ioURing.QueueTcpSend(userData, event->m_fd, event->m_data))
    {
        LOG_ERROR("[{}] failed to queue tcp send", handler.Name());
        return;
    }

    m_pendingEvents[userData] = std::move(event);
}

void Proactor::RequestTcpRecv(TcpClient& handler)
{
    auto event{ std::make_unique<TcpRecv>(handler.m_id, handler.m_host, handler.m_port, handler.m_fd) };
    IOURing::UserData userData{ event->m_id };

    if (not m_ioURing.QueueTcpRecv(userData, event->m_fd, event->m_data))
    {
        LOG_ERROR("[{}] failed to queue tcp send", handler.Name());
        return;
    }

    m_pendingEvents[userData] = std::move(event);
}

void Proactor::CompleteEvent(Event& event, const io_uring_cqe& cEvent)
{
    switch (event.Type())
    {
        case EventType::TimerExpired:
            CompleteTimerExpiredEvent(static_cast<TimerExpiredEvent&>(event), cEvent);
            break;

        case EventType::TimerUpdate:
            CompleteTimerUpdateEvent(static_cast<TimerUpdateEvent&>(event), cEvent);
            break;

        case EventType::TimerCancel:
            CompleteTimerCancelEvent(static_cast<TimerCancelEvent&>(event), cEvent);
            break;

        case EventType::Signal:
            CompleteSignalEvent(static_cast<SignalEvent&>(event), cEvent);
            break;

        case EventType::TcpConnect:
            CompleteTcpConnect(static_cast<TcpConnect&>(event), cEvent);
            break;

        case EventType::TcpSend:
            CompleteTcpSend(static_cast<TcpSend&>(event), cEvent);
            break;

        case EventType::TcpRecv:
            CompleteTcpRecv(static_cast<TcpRecv&>(event), cEvent);
            break;

        default:
        {
            LOG_ERROR("received unknown event={}", event.NameAndType());
            break;
        }
    }
}

void Proactor::CompleteTimerExpiredEvent(TimerExpiredEvent& event, const io_uring_cqe& cEvent)
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
                ScopedDeadline dl{ "TimerHandler:" + std::string{ handler.Name() }, 20ms };
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

void Proactor::CompleteTimerUpdateEvent(TimerUpdateEvent& event, const io_uring_cqe& cEvent)
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
        // timer update acknowledged
        case 0:
        {
            LOG_DEBUG("[{}] timer update acknowledged eventId({})", handler.Name(), event.m_id);
            break;
        }

        default:
        {
            LOG_ERROR("failed eventId({}) res({}) {}", event.m_id, eventRes, strerror(-eventRes));
            break;
        }
    }
}

void Proactor::CompleteTimerCancelEvent(TimerCancelEvent& event, const io_uring_cqe& cEvent)
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

void Proactor::CompleteSignalEvent(SignalEvent& event, const io_uring_cqe& cEvent)
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

void Proactor::CompleteTcpConnect(TcpConnect& event, const io_uring_cqe& cEvent)
{
    int res{ cEvent.res };
    auto itr{ m_tcpClients.find(event.m_handlerId) };
    if (itr == m_tcpClients.end())
    {
        LOG_ERROR("failed to find socket client for '{}:{}'", event.m_host, event.m_port);
        return;
    }

    auto [_, handler] = *itr;
    handler->m_state = TcpClient::Broken;
    if (res < 0)
    {
        LOG_ERROR("[{}] tcp connect failed. {}. closing fd", handler->Name(), strerror(-res));

        if (event.m_fd > 0 and ::close(event.m_fd) == -1)
        {
            int err{ errno };
            LOG_ERROR("[{}] tcp connect res failed. failed to close socket. {}", handler->Name(), strerror(err));
        }
        return;
    }

    handler->m_state = TcpClient::Connected;
    handler->m_fd = event.m_fd;
    handler->OnConnect();
}

void Proactor::CompleteTcpSend(TcpSend& event, const io_uring_cqe& cEvent)
{
    int res{ cEvent.res };
    auto itr{ m_tcpClients.find(event.m_handlerId) };
    if (itr == m_tcpClients.end())
    {
        LOG_ERROR("failed to find socket client for '{}:{}'", event.m_host, event.m_port);
        return;
    }

    auto [_, handler] = *itr;
    if (res < 0)
    {
        LOG_ERROR("[{}] tcp send res failed. {}", handler->Name(), strerror(-res));
    }
}

void Proactor::CompleteTcpRecv(TcpRecv& event, const io_uring_cqe& cEvent)
{
    int res{ cEvent.res };
    auto itr{ m_tcpClients.find(event.m_handlerId) };
    if (itr == m_tcpClients.end())
    {
        LOG_ERROR("failed to find socket client for '{}:{}'", event.m_host, event.m_port);
        return;
    }

    auto [_, handler] = *itr;
    handler->m_rxPending = false;

    if (res < 0)
    {
        LOG_ERROR("[{}] tcp recv res failed. {}", handler->Name(), strerror(-res));
        return;
    }

    if (res == 0)
    {
        LOG_INFO("[{}] tcp connection received 0 bytes", handler->Name());
        return;
    }

    std::span buff{ event.m_data.begin(), static_cast<size_t>(res) };
    handler->OnReceive(buff);
    // re-queue  for another recv
    handler->QueueRecv();
}

Event* Proactor::FindPendingEvent(Handler::Id id, EventType eType)
{
    for (auto& [_, pending] : m_pendingEvents)
    {
        if (pending->Type() != eType)
        {
            continue;
        }

        if (pending->m_handlerId != id)
        {
            continue;
        }

        return pending.get();
    }

    return nullptr;
}

} // namespace Sage
