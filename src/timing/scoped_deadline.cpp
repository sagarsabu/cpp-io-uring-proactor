#include "log/logger.hpp"
#include "timing/time.hpp"
#include "timing/scoped_deadline.hpp"

namespace Sage
{

ScopedDeadline::~ScopedDeadline()
{
    auto now = Clock::now();
    auto duration = std::chrono::duration_cast<TimeMS>(now - m_start);
    if (duration <= m_deadline)
    {
        LOG_TRACE("ScopedDeadline '{}' took:{}", m_tag, duration);
    }
    else
    {
        LOG_WARNING("ScopedDeadline '{}' took:{} deadline:{}",
            m_tag, duration, m_deadline);
    }
}

} // namespace Sage
