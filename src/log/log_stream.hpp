#pragma once

#include <chrono>
#include <fstream>
#include <iostream>
#include <source_location>
#include <string>

#include "log/log_levels.hpp"

namespace Sage::Logger::Internal
{

class LogStreamer
{
public:
    using Stream = std::ostream;

    LogStreamer() = default;

    void Setup(const std::string& filename, Level level);

    Level GetLogLevel() const noexcept { return m_logLevel; }

    void EnsureLogFileWriteable();

private:
    // Nothing in here is movable or copyable
    LogStreamer(const LogStreamer&) = delete;
    LogStreamer(LogStreamer&&) = delete;
    LogStreamer& operator=(const LogStreamer&) = delete;
    LogStreamer& operator=(LogStreamer&&) = delete;

    void SetStreamToConsole();

    void SetStreamToFile(std::ofstream fileStream);

private:
    static constexpr std::reference_wrapper<Stream> s_consoleStream{ std::cout };
    static constexpr auto s_logFileCreatorPeriod{ std::chrono::seconds{ 60 } };

    std::reference_wrapper<Stream> m_streamRef{ s_consoleStream };
    std::string m_logFilename{};
    Level m_logLevel{ Level::Info };
    size_t m_lostLogTime{ 0 };
    std::ofstream m_logFileStream{};

    template<typename... Args>
    friend inline void
    LogToStream(Level level, std::format_string<Args...> fmt, const std::source_location& loc, Args&&... args);
};

LogStreamer& GetLogStreamer() noexcept;

} // namespace Sage::Logger::Internal
