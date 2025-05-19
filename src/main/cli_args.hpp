#pragma once

#include <string>

#include "log/log_levels.hpp"

namespace Sage
{

struct CliArgs
{
    Logger::Level level;
    std::string logFile;
};

CliArgs GetCliArgs(int argc, char* const argv[]);

} // namespace Sage
