#pragma once

#include <chrono>
#include <linux/time_types.h>

namespace Sage
{

// Makes life easier
using namespace std::chrono_literals;

// Useful aliases

using Clock = std::chrono::high_resolution_clock;
using TimeNS = std::chrono::nanoseconds;
using TimeUS = std::chrono::microseconds;
using TimeMS = std::chrono::milliseconds;
using TimeS = std::chrono::seconds;

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

} // namespace Sage
