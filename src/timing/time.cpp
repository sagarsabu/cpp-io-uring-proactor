#include <ctime>

#include "timing/time.hpp"

namespace Sage
{

Timestamp GetCurrentTimeStamp() noexcept
{
    Timestamp ts;
    timespec timeSpec{};
    std::tm localTime{};

    std::timespec_get(&timeSpec, TIME_UTC);
    std::strftime(ts.m_date, sizeof(ts.m_date), "%d-%m-%Y %H:%M:%S", ::localtime_r(&timeSpec.tv_sec, &localTime));
    ::snprintf(ts.m_ns, sizeof(ts.m_ns), ":%09lu", timeSpec.tv_nsec);

    return ts;
}

} // namespace Sage
