#pragma once

#include <cstddef>
#include <format>
#include <print>
#include <source_location>
#include <string>
#include <string_view>

#include "log/log_stream.hpp"
#include "timing/time.hpp"

namespace Sage
{

namespace Logger
{

void SetupLogger(const std::string& filename = "", Level logLevel = Level::Info);

void EnsureLogFileExist();

namespace Internal
{

std::string_view GetLevelFormatter(Level level) noexcept;

std::string_view GetLevelName(Level level) noexcept;

std::string_view GetFormatEnd() noexcept;

inline bool ShouldLog(Level level) noexcept { return level >= GetLogStreamer().GetLogLevel(); }

constexpr std::string_view GetFilenameStem(const std::string_view fileName) noexcept
{
    const std::string_view fnameStem{ fileName };
    const size_t pos{ fnameStem.find_last_of('/') };

    if (pos == std::string_view::npos)
    {
        return fnameStem;
    }

    return fnameStem.substr(pos + 1);
}

template<typename... Args>
inline void LogToStream(Level level, std::format_string<Args...> fmt, const std::source_location& loc, Args&&... args)
{
    if (not Internal::ShouldLog(level))
        return;

    Timestamp ts{ GetCurrentTimeStamp() };

    auto& logStreamer{ GetLogStreamer() };
    LogStreamer::Stream& stream{ logStreamer.m_streamRef.get() };
    std::println(
        stream,
        "{}[{}{}] [{}] [{}:{}] {}{}",
        GetLevelFormatter(level),
        ts.m_date,
        ts.m_ns,
        GetLevelName(level),
        GetFilenameStem(loc.file_name()),
        loc.line(),
        std::format(fmt, std::forward_like<Args>(args)...),
        GetFormatEnd()
    );
    std::flush(stream);
}

template<typename... Args>
inline void Trace(std::format_string<Args...> fmt, const std::source_location& loc, Args&&... args)
{
    Logger::Internal::LogToStream(Logger::Trace, fmt, loc, std::forward_like<Args>(args)...);
}

template<typename... Args>
inline void Debug(std::format_string<Args...> fmt, const std::source_location& loc, Args&&... args)
{
    Logger::Internal::LogToStream(Logger::Debug, fmt, loc, std::forward_like<Args>(args)...);
}

template<typename... Args>
inline void Info(std::format_string<Args...> fmt, const std::source_location& loc, Args&&... args)
{
    Logger::Internal::LogToStream(Logger::Info, fmt, loc, std::forward_like<Args>(args)...);
}

template<typename... Args>
inline void Warning(std::format_string<Args...> fmt, const std::source_location& loc, Args&&... args)
{
    Logger::Internal::LogToStream(Logger::Warning, fmt, loc, std::forward_like<Args>(args)...);
}

template<typename... Args>
inline void Error(std::format_string<Args...> fmt, const std::source_location& loc, Args&&... args)
{
    Logger::Internal::LogToStream(Logger::Error, fmt, loc, std::forward_like<Args>(args)...);
}

template<typename... Args>
inline void Critical(std::format_string<Args...> fmt, const std::source_location& loc, Args&&... args)
{
    Logger::Internal::LogToStream(Logger::Critical, fmt, loc, std::forward_like<Args>(args)...);
}

} // namespace Internal

} // namespace Logger

} // namespace Sage

// Log marcos for lazy va args evaluation

#define LOG_TRACE(fmt, ...)                                                                                            \
    if (Sage::Logger::Internal::ShouldLog(Sage::Logger::Trace)) [[unlikely]]                                           \
    Sage::Logger::Internal::Trace(fmt, std::source_location::current(), ##__VA_ARGS__)

#define LOG_DEBUG(fmt, ...)                                                                                            \
    if (Sage::Logger::Internal::ShouldLog(Sage::Logger::Debug)) [[unlikely]]                                           \
    Sage::Logger::Internal::Debug(fmt, std::source_location::current(), ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) Sage::Logger::Internal::Info(fmt, std::source_location::current(), ##__VA_ARGS__)

#define LOG_WARNING(fmt, ...) Sage::Logger::Internal::Warning(fmt, std::source_location::current(), ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) Sage::Logger::Internal::Error(fmt, std::source_location::current(), ##__VA_ARGS__)

#define LOG_CRITICAL(fmt, ...) Sage::Logger::Internal::Critical(fmt, std::source_location::current(), ##__VA_ARGS__)
