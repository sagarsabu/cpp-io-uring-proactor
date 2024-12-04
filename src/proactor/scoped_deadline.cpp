#include "log/logger.hpp"
#include "proactor/time.hpp"
#include "proactor/scoped_deadline.hpp"

namespace Sage
{

ScopedDeadline::~ScopedDeadline()
{
    auto now = Clock::now();
    auto duration = std::chrono::duration_cast<TimeMS>(now - m_start);
    if (duration <= m_deadline)
    {
        LOG_TRACE("ScopedDeadline '%s' took:%ld ms", m_tag.c_str(), duration.count());
    }
    else
    {
        LOG_WARNING("ScopedDeadline '%s' took:%ld ms deadline:%ld ms",
            m_tag.c_str(), duration.count(), m_deadline.count());
    }
}

} // namespace Sage
