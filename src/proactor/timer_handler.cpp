#include "proactor/timer_handler.hpp"

namespace Sage
{

TimerHandler::TimerHandler(std::string_view name, const TimeNS& period) : m_name{ name }, m_period{ period }
{
    Proactor::Instance()->AddTimerHandler(*this);
}

TimerHandler::~TimerHandler() { Proactor::Instance()->RemoveTimerHandler(*this); }

} // namespace Sage
