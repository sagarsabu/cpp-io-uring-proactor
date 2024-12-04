#pragma once

#include <string_view>

#include "proactor/proactor.hpp"
#include "proactor/time.hpp"

namespace Sage
{

class TimerHandler
{
public:
    TimerHandler(std::string_view name, const TimeNS& period);

    virtual ~TimerHandler();

    static void SetProactor(SharedProactor proactor);

    const char* Name() const noexcept { return m_name.c_str(); }

    const std::string& NameStr() const noexcept { return m_name; }

private:
    TimerHandler() = delete;

    virtual void OnTimerExpired() = 0;

private:
    const std::string m_name;
    TimeNS m_period;

    static inline SharedProactor s_sharedProactor{ nullptr };
    static constexpr itimerspec DISABLED_TIMER{
        .it_interval = ChronoTimeToTimeSpec(0ns),
        .it_value = ChronoTimeToTimeSpec(0ns)
    };

    friend class Proactor;
};

} // namespace Sage
