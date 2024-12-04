#include "proactor/timer_handler.hpp"

namespace Sage
{

TimerHandler::TimerHandler(std::string_view name, const TimeNS& period) :
    m_name{ name },
    m_period{ period }
{
    s_sharedProactor->AddTimerHandler(*this);
}

TimerHandler::~TimerHandler()
{
    s_sharedProactor->RemoveTimerHandler(*this);
}

void TimerHandler::SetProactor(SharedProactor proactor)
{
    s_sharedProactor = proactor;
}

} // namespace Sage
