#include "proactor/timer_handler.hpp"
#include "proactor/proactor.hpp"

namespace Sage
{

TimerHandler::TimerHandler(std::string_view name, const TimeNS& period) : m_name{ name }, m_period{ period }
{
    Proactor::Instance()->AddTimerHandler(*this);
}

TimerHandler::~TimerHandler() { Proactor::Instance()->RemoveTimerHandler(*this); }

void TimerHandler::UpdateInterval(const TimeNS& period)
{
    if (m_period == period)
    {
        return;
    }

    m_period = period;
    Proactor::Instance()->UpdateTimerHandler(*this);
}

} // namespace Sage
