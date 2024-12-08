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

void TimerHandler::SetProactor(std::shared_ptr<Proactor> proactor)
{
    s_sharedProactor = proactor;
}

HandlerId TimerHandler::NextId() noexcept
{
    static HandlerId id{ 0 };
    id = std::min(id + 1, std::numeric_limits<HandlerId>::max() - 1);
    return id;
}

} // namespace Sage
