#pragma once

#include <chrono>
#include <linux/time_types.h>

namespace Sage
{

// Makes life easier
using namespace std::chrono_literals;

// Useful aliases

using Clock = std::chrono::steady_clock;
using TimeNS = std::chrono::nanoseconds;
using TimeUS = std::chrono::microseconds;
using TimeMS = std::chrono::milliseconds;
using TimeS = std::chrono::seconds;

struct Timestamp
{
    // e.g "01 - 09 - 2023 00:42 : 19"
    using DateBuffer = char[26];
    // :%09lu requires 22 bytes max
    using NanoSecBuffer = char[22];

    DateBuffer m_date;
    NanoSecBuffer m_ns;
};

// Helpers

// cppcheck-suppress unusedFunction
constexpr timespec ChronoTimeToTimeSpec(const TimeNS& duration) noexcept
{
    const TimeS seconds{ std::chrono::duration_cast<TimeS>(duration) };
    const TimeNS nanoSecs{ duration };
    return timespec{ .tv_sec = seconds.count(), .tv_nsec = (nanoSecs - seconds).count() };
}

constexpr __kernel_timespec ChronoTimeToKernelTimeSpec(const TimeNS& duration) noexcept
{
    const TimeS seconds{ std::chrono::duration_cast<TimeS>(duration) };
    const TimeNS nanoSecs{ duration };
    return __kernel_timespec{ .tv_sec = seconds.count(), .tv_nsec = (nanoSecs - seconds).count() };
}

Timestamp GetCurrentTimeStamp() noexcept;

} // namespace Sage
