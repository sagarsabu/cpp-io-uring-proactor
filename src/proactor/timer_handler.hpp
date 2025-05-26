#pragma once

#include <string_view>

#include "proactor/handler.hpp"
#include "proactor/proactor.hpp"
#include "timing/time.hpp"

namespace Sage
{

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
    const Handler::Id m_id{ Handler::NextId() };

    friend class Proactor;
};

} // namespace Sage
