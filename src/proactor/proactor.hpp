#pragma once

#include <functional>
#include <memory>

#include "proactor/events.hpp"
#include "proactor/handle.hpp"
#include "proactor/io_uring.hpp"

namespace Sage
{

class TimerHandler;
class TcpClient;
class TcpConnect;
class TcpRecv;
class TcpSend;
class SignalEvent;

class Proactor
{
public:
    using SignalHandleFunc = std::move_only_function<void(const signalfd_siginfo&)>;

public:
    static void Create();

    static void Destroy();

    static Proactor& Instance() { return *s_instance; }

    ~Proactor();

    void Run();

    void AddTimerHandler(TimerHandler& handler);

    void StartTimerHandler(TimerHandler& handler);

    void UpdateTimerHandler(TimerHandler& handler);

    void RemoveTimerHandler(TimerHandler& handler);

    void AddSocketClient(TcpClient& handler);

    void StartSocketClient(TcpClient& handler);

    void RemoveSocketClient(TcpClient& handler);

    void RequestTcpSend(TcpClient&, std::string);

    void RequestTcpRecv(TcpClient&);

private:
    // creation via factory
    Proactor();

    Proactor(const Proactor&) = delete;
    Proactor(Proactor&&) = delete;
    Proactor& operator=(const Proactor&) = delete;
    Proactor& operator=(Proactor&&) = delete;

    void StartAllHandlers();

    void AttachExitHandlers();

    void AddSignalHandler(int signal, SignalHandleFunc&& func);

    void RequestTimerContinuous(TimerHandler& handler);

    void RequestTimerUpdate(TimerHandler& handler);

    void RequestTimerCancel(TimerHandler& handler);

    bool RequestSignalRead(int signal, int signalFd);

    void RequestTcpConnect(TcpClient&);

    void CompleteTimerExpiredEvent(Event& event, const io_uring_cqe& cEvent);

    void CompleteTimerUpdateEvent(Event& event, const io_uring_cqe& cEvent);

    void CompleteTimerCancelEvent(Event& event, const io_uring_cqe& cEvent);

    void CompleteSignalEvent(SignalEvent& event, const io_uring_cqe& cEvent);

    void CompleteTcpConnect(TcpConnect& event, const io_uring_cqe& cEvent);

    void CompleteTcpSend(TcpSend& event, const io_uring_cqe& cEvent);

    void CompleteTcpRecv(TcpRecv& event, const io_uring_cqe& cEvent);

    template<typename ET> auto FindPendingEvent(Handle::Id id)
    {
        ET* res{ nullptr };
        for (auto& [_, pending] : m_pendingEvents)
        {
            auto child{ dynamic_cast<ET*>(pending.get()) };
            if (child == nullptr)
            {
                continue;
            }

            if (pending->m_handlerId != id)
            {
                continue;
            }

            res = child;
            break;
        }

        return res;
    }

private:
    static inline Proactor* s_instance{ nullptr };

    IOURing m_ioURing{ 10'000 };
    bool m_running{ false };
    std::unordered_map<EventId, std::unique_ptr<Event>> m_pendingEvents;
    std::unordered_map<Handle::Id, TimerHandler*> m_timerHandlers;
    std::unordered_map<Handle::Id, TcpClient*> m_tcpClients;

    struct SignalHandleData
    {
        const int m_fd;
        const int m_signal;
        SignalHandleFunc m_callback;
    };

    // map signal num -> handler data
    std::unordered_map<int, std::unique_ptr<SignalHandleData>> m_signalHandlers;
};

} // namespace Sage
