#pragma once

#include <string>

namespace Sage
{

namespace Logger
{

enum Level
{
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

void SetupLogger(Level logLevel = Level::Info, const std::string& filename = "");

void EnsureLogFileExist();

namespace Internal
{

[[gnu::format(printf, 1, 2)]]
void Trace(const char* msg, ...);

[[gnu::format(printf, 1, 2)]]
void Debug(const char* msg, ...);

[[gnu::format(printf, 1, 2)]]
void Info(const char* msg, ...);

[[gnu::format(printf, 1, 2)]]
void Warning(const char* msg, ...);

[[gnu::format(printf, 1, 2)]]
void Error(const char* msg, ...);

[[gnu::format(printf, 1, 2)]]
void Critical(const char* msg, ...);

bool ShouldLog(Level level) noexcept;

} // namespace Internal

} // namespace Logger

} // namespace Sage

// Log marcos for lazy va args evaluation

#define LOG_TRACE(fmt, ...) if (Sage::Logger::Internal::ShouldLog(Sage::Logger::Trace)) [[unlikely]] Sage::Logger::Internal::Trace(fmt, ## __VA_ARGS__)

#define LOG_DEBUG(fmt, ...) if (Sage::Logger::Internal::ShouldLog(Sage::Logger::Debug)) [[unlikely]] Sage::Logger::Internal::Debug(fmt, ## __VA_ARGS__)

#define LOG_INFO(fmt, ...) Sage::Logger::Internal::Info(fmt, ## __VA_ARGS__)

#define LOG_WARNING(fmt, ...) Sage::Logger::Internal::Warning(fmt, ## __VA_ARGS__)

#define LOG_ERROR(fmt, ...) Sage::Logger::Internal::Error(fmt, ## __VA_ARGS__)

#define LOG_CRITICAL(fmt, ...) Sage::Logger::Internal::Critical(fmt, ## __VA_ARGS__)
