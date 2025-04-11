#pragma once

#include <functional>
#include <memory>

#include "proactor/events.hpp"
#include "proactor/io_uring.hpp"

namespace Sage
{

class TimerHandler;

class Proactor
{
public:
    using SignalHandleFunc = std::function<void(const signalfd_siginfo&)>;

public:
    static std::shared_ptr<Proactor> Create();

    static void Destroy();

    static std::shared_ptr<Proactor> Instance() { return s_instance; }

    ~Proactor();

    void Run();

    void AddTimerHandler(TimerHandler& handler);

    void StartTimerHandler(TimerHandler& handler);

    void RemoveTimerHandler(TimerHandler& handler);

private:
    // creation via factory
    Proactor();

    void StartAllHandlers();

    void AttachExitHandlers();

    void AddSignalHandler(int signal, SignalHandleFunc&& func);

    void RequestContinuousTimeout(TimerHandler& handler);

    void RequestTimeoutCancel(const TimerHandler& handler);

    bool RequestSignalRead(int signal, int signalFd);

    void HandleEvent(Event& event, const io_uring_cqe& cEvent);

    void HandleTimeoutEvent(TimeoutEvent& event, const io_uring_cqe& cEvent);

    void HandleTimeoutCancelEvent(TimeoutCancelEvent& event, const io_uring_cqe& cEvent);

    void HandleSignalEvent(SignalEvent& event, const io_uring_cqe& cEvent);

private:
    static inline std::shared_ptr<Proactor> s_instance{ nullptr };

    IOURing m_ioURing{ 10'000 };
    bool m_running{ false };
    std::unordered_map<EventId, std::unique_ptr<Event>> m_pendingEvents;
    std::unordered_map<HandlerId, TimerHandler*> m_timerHandlers;

    struct SignalHandleData
    {
        const int m_fd;
        const int m_signal;
        const SignalHandleFunc m_callback;
    };

    // map signal num -> handler data
    std::unordered_map<int, std::unique_ptr<SignalHandleData>> m_signalHandlers;
};

} // namespace Sage
