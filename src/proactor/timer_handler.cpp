#include "proactor/timer_handler.hpp"

namespace Sage
{

TimerHandler::TimerHandler(std::string_view name, const TimeNS& period) : m_name{ name }, m_period{ period }
{
    Proactor::Instance()->AddTimerHandler(*this);
}

TimerHandler::~TimerHandler() { Proactor::Instance()->RemoveTimerHandler(*this); }

HandlerId TimerHandler::NextId() noexcept
{
    static HandlerId id{ 0 };
    id = std::min(id + 1, std::numeric_limits<HandlerId>::max() - 1);
    return id;
}

} // namespace Sage
