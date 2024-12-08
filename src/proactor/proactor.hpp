#pragma once

#include <memory>
#include <unordered_set>

#include "proactor/io_uring.hpp"
#include "proactor/events.hpp"

namespace Sage
{

class TimerHandler;

class Proactor
{
public:
    static std::shared_ptr<Proactor> Create();

    void Run();

    void AddTimerHandler(TimerHandler& handler);

    void StartTimerHandler(TimerHandler& handler);

    void RemoveTimerHandler(TimerHandler& handler);

private:
    Proactor() = default;

    void StartAllHandlers();

    void RequestContinuousTimeout(TimerHandler& handler);

    void RequestTimeoutCancel(const TimerHandler& handler);

    void HandleEvent(Event& event, io_uring_cqe& cEvent);

    void HandleTimeoutEvent(TimeoutEvent& event, const io_uring_cqe& cEvent);

    void HandleTimeoutCancelEvent(TimeoutCancelEvent& event, const io_uring_cqe& cEvent);

    IOURing m_ioURing{ 100 };
    bool m_running{ false };
    std::unordered_map<HandlerId, TimerHandler*> m_timerHandlers{};
    std::unordered_map<EventId, std::unique_ptr<Event>> m_pendingEvents{};
};

} // namespace Sage
