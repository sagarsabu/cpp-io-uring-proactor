#pragma once

#include "timing/time.hpp"

namespace Sage
{

struct ScopedDeadline final
{
    ScopedDeadline(const std::string& tag, const TimeMS& deadline) :
        m_tag{ tag },
        m_deadline{ deadline }
    { }

    ~ScopedDeadline();

private:
    const Clock::time_point m_start{ Clock::now() };
    const std::string m_tag;
    const TimeMS m_deadline;
};

} // namespace Sage
