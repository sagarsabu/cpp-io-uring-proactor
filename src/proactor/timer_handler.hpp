#pragma once

#include <string_view>

#include "proactor/proactor.hpp"
#include "timing/time.hpp"
#include "utils/aliases.hpp"

namespace Sage
{

class Proactor;

// All instances must be shared pointers
class TimerHandler
{
public:
    TimerHandler(std::string_view name, const TimeNS& period);

    virtual ~TimerHandler();

    std::string_view Name() const noexcept { return m_name; }

    const std::string& NameStr() const noexcept { return m_name; }

private:
    TimerHandler() = delete;
    TimerHandler(const TimerHandler&) = delete;
    TimerHandler(TimerHandler&&) = delete;
    TimerHandler& operator=(const TimerHandler&) = delete;
    TimerHandler& operator=(TimerHandler&&) = delete;

    virtual void OnTimerExpired() = 0;

    static HandlerId NextId() noexcept;

private:
    const std::string m_name;
    TimeNS m_period;
    const HandlerId m_id{ NextId() };

    friend class Proactor;
};

} // namespace Sage
