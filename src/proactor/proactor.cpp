#include "log/logger.hpp"
#include "proactor/proactor.hpp"
#include "proactor/timer_handler.hpp"
#include "proactor/scoped_deadline.hpp"

namespace Sage
{

SharedProactor Proactor::Create()
{
    return SharedProactor{ new Proactor };
}

void Proactor::StartAllHandlers()
{
    for (auto handler : m_timerHandlers)
    {
        KickTimerHandler(*handler);
    }
}

void Proactor::Run()
{
    StartAllHandlers();

    m_running = true;

    while (true)
    {
        UniqueUringCEvent event{ m_uring->WaitForEvent() };
        if (event == nullptr)
        {
            continue;
        }

        TimerHandler& handler{ *reinterpret_cast<TimerHandler*>(event->user_data) };
        TriggerTimerHandlerExpiry(handler, *event);
    }

    m_running = false;
}

void Proactor::AddTimerHandler(TimerHandler& handler)
{
    if (m_timerHandlers.contains(&handler))
    {
        LOG_ERROR("[%s] %s handler already in collection", handler.Name(), __func__);
        return;
    }

    m_timerHandlers.emplace(&handler);

    if (m_running)
    {
        StartTimerHandler(handler);
    }
}

void Proactor::StartTimerHandler(TimerHandler& handler)
{
    if (not m_timerHandlers.contains(&handler))
    {
        LOG_CRITICAL("[%s] %s handler not in collection", handler.Name(), __func__);
        return;
    }

    KickTimerHandler(handler);
}

void Proactor::RemoveTimerHandler(TimerHandler& handler)
{
    auto itr = m_timerHandlers.find(&handler);
    if (itr == m_timerHandlers.end())
    {
        LOG_ERROR("[%s] %s handler not in collection", handler.Name(), __func__);
        return;
    }

    LOG_INFO("[%s] %s handler removed", handler.Name(), __func__);

    CancelTimerHandler(handler);
    m_timerHandlers.erase(itr);
}


void Proactor::KickTimerHandler(TimerHandler& handler)
{
    if (auto data{ reinterpret_cast<IOURing::UserData>(&handler) };
        not m_uring->QueueTimeoutEvent(data, handler.m_period))
    {
        LOG_ERROR("[%s] %s failed", handler.Name(), __func__);
    }
}

void Proactor::CancelTimerHandler(TimerHandler& handler)
{
    if (auto data{ reinterpret_cast<IOURing::UserData>(&handler) };
        not m_uring->CancelTimeoutEvent(data))
    {
        LOG_ERROR("[%s] %s failed", handler.Name(), __func__);
    }
}

void Proactor::TriggerTimerHandlerExpiry(TimerHandler& handler, const io_uring_cqe& event)
{
    int eventRes{ event.res };
    switch (eventRes)
    {
        // timer expired
        case -ETIME:
        {
            LOG_DEBUG("[%s] timer expired. triggering handler", handler.Name());

            {
                ScopedDeadline dl{ "TimerHandler:" + handler.NameStr(), 20ms };
                handler.OnTimerExpired();
            }

            KickTimerHandler(handler);
            break;
        }

        // timer cancelled
        case -ECANCELED:
        {
            LOG_DEBUG("[%s] timer cancelled. triggering handler", handler.Name());
            break;
        }

        default:
        {
            LOG_WARNING("[%s] timer expiry got unexpected result(%d)", handler.Name(), eventRes);
            break;
        }
    }
}

} // namespace Sage
