#pragma once

#include <memory>
#include <unordered_set>

#include "proactor/aliases.hpp"
#include "proactor/io_uring.hpp"
#include "proactor/events.hpp"

namespace Sage
{

class Proactor
{
public:
    static SharedProactor Create();

    void Run();

    void AddTimerHandler(TimerHandler& handler);

    void StartTimerHandler(TimerHandler& handler);

    void RemoveTimerHandler(TimerHandler& handler);

private:
    Proactor() = default;

    void StartAllHandlers();

    void KickTimerHandler(TimerHandler& handler);
    void CancelTimerHandler(const TimerHandler& handler);
    void TriggerTimerHandlerExpiry(Event& event, const io_uring_cqe& cEvent);

    IOURing m_ioURing{ 100 };
    bool m_running{ false };
    std::unordered_map<size_t, TimerHandler*> m_timerHandlers{};
    std::unordered_map<size_t, std::unique_ptr<Event>> m_pendingEvents{};
};

} // namespace Sage
