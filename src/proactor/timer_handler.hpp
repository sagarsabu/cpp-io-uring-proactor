#pragma once

#include <string_view>

#include "proactor/handle.hpp"
#include "proactor/proactor.hpp"
#include "timing/time.hpp"

namespace Sage
{

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

class TimerHandler
{
public:
    TimerHandler(std::string_view name, const TimeNS& period);

    virtual ~TimerHandler();

    std::string_view Name() const noexcept { return m_name; }

    void UpdateInterval(const TimeNS& period);

private:
    TimerHandler() = delete;
    TimerHandler(const TimerHandler&) = delete;
    TimerHandler(TimerHandler&&) = delete;
    TimerHandler& operator=(const TimerHandler&) = delete;
    TimerHandler& operator=(TimerHandler&&) = delete;

    virtual void OnTimerExpired() = 0;

private:
    const std::string m_name;
    TimeNS m_period;
    const Handle::Id m_id{ Handle::NextId() };

    friend class Proactor;
};

} // namespace Sage
