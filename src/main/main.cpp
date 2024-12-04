#include "log/logger.hpp"
#include "log/logfile_checker.hpp"
#include "proactor/proactor.hpp"
#include "proactor/timer_handler.hpp"
#include "main/cli_args.hpp"

namespace Sage
{

class TestTimerHandler final : public TimerHandler
{
public:
    TestTimerHandler() :
        TimerHandler{ "testHandler", 1s }
    { }

    void OnTimerExpired() override
    {
        LOG_INFO("[%s] timer expired", Name());
    }
};

} // namespace Sage

int main(int argc, char* const argv[])
{
    using namespace Sage;

    int res{ 0 };

    try
    {
        auto [logLevel, logFile] { GetCLiArgs(argc, argv) };
        Logger::SetupLogger(logLevel, logFile);

        LOG_INFO("cpp-io-uring-proactor starting");

        std::shared_ptr<Proactor> proactor{ Proactor::Create() };
        TimerHandler::SetProactor(proactor);

        LogFileChecker logChecker{ Logger::EnsureLogFileExist };
        TestTimerHandler handler;

        proactor->Run();
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("caught std exception. %s", e.what());
        res = 1;
    }

    LOG_INFO("cpp-io-uring-proactor exiting code(%d)", res);

    return res;
}
