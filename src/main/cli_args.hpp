#pragma once

#include <array>
#include <getopt.h>
#include <iostream>
#include <print>
#include <string>

#include "log/log_levels.hpp"

namespace Sage
{

auto GetCLiArgs(int argc, char* const argv[])
{
    constexpr std::array argOptions{
        option{ "help",  no_argument,       nullptr, 'h' },
        option{ "level", required_argument, nullptr, 'l' },
        option{ "file",  required_argument, nullptr, 'f' },
        option{ 0,       0,                 0,       0   }
    };

    auto usage = [&argv]
    {
        std::string_view progName{ argv[0] };
        if (size_t pos{ progName.find_last_of('/') }; pos != std::string::npos)
        {
            progName = progName.substr(pos + 1);
        }

        std::println(
            std::cerr,
            "Usage: %s"
            "\n\t[optional] --level|-l <t|trace|d|debug|i|info|w|warn|e|error|c|critical>"
            "\n\t[optional] --file|-f <filename> "
            "\n\t[optional] --help|-h",
            progName
        );
    };

    auto getLogLevel = [](std::string_view logArg) -> Logger::Level
    {
        Logger::Level level{ Logger::Level::Info };
        if (logArg == "trace" or logArg == "t")
        {
            level = Logger::Level::Trace;
        }
        else if (logArg == "debug" or logArg == "d")
        {
            level = Logger::Level::Debug;
        }
        else if (logArg == "info" or logArg == "i")
        {
            level = Logger::Level::Info;
        }
        else if (logArg == "warn" or logArg == "w")
        {
            level = Logger::Level::Warning;
        }
        else if (logArg == "error" or logArg == "e")
        {
            level = Logger::Level::Error;
        }
        else if (logArg == "critical" or logArg == "c")
        {
            level = Logger::Level::Critical;
        }

        return level;
    };

    Logger::Level logLevel{ Logger::Info };
    std::string logFile;

    int option;
    int optIndex;
    while ((option = getopt_long(argc, argv, "hl:f:", argOptions.data(), &optIndex)) != -1)
    {
        switch (option)
        {
            case 'h':
                usage();
                std::exit(0);
                break;

            case 'l':
                logLevel = getLogLevel(optarg);
                break;

            case 'f':
                logFile = optarg;
                break;

            case '?':
            default:
                usage();
                std::exit(1);
                break;
        }
    }

    return std::make_pair(logLevel, logFile);
}

} // namespace Sage
