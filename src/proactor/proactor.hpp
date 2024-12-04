#pragma once

#include <memory>
#include <unordered_set>

#include "proactor/io_uring.hpp"

namespace Sage
{

class Proactor;
class TimerHandler;

using SharedProactor = std::shared_ptr<Proactor>;

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
    void CancelTimerHandler(TimerHandler& handler);
    void TriggerTimerHandlerExpiry(TimerHandler& handler, const io_uring_cqe& event);

    SharedURing m_uring{ IOURing::Create(100) };
    bool m_running{ false };
    std::unordered_set<TimerHandler*> m_timerHandlers{};
};

} // namespace Sage
