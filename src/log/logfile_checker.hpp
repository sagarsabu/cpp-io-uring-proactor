#pragma once

#include <functional>

#include "proactor/timer_handler.hpp"
#include "log/logger.hpp"

namespace Sage
{

class LogFileChecker final : public TimerHandler
{
public:
    using CheckFunc = std::function<void()>;

    explicit LogFileChecker(CheckFunc&& cb) :
        TimerHandler{ "LogFileChecker", 250ms },
        m_checkCb{ std::move(cb) }
    { }

    void OnTimerExpired() override
    {
        LOG_DEBUG("[{}] invoking log file check callback", Name());
        m_checkCb();
    }

private:
    const CheckFunc m_checkCb;

};

} // namespace Sage
