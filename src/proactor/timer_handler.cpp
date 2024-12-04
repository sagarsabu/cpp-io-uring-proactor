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

size_t TimerHandler::NextId()
{
    static size_t id{ 0 };
    id = std::min(id + 1, std::numeric_limits<size_t>::max() - 1);
    return id;
}

} // namespace Sage
