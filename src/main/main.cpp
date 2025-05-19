#include "log/logfile_checker.hpp"
#include "log/logger.hpp"
#include "main/cli_args.hpp"
#include "proactor/proactor.hpp"
#include "proactor/tcp_client.hpp"
#include "proactor/timer_handler.hpp"

namespace Sage
{

class TestTimerHandler final : public TimerHandler
{
public:
    TestTimerHandler() : TimerHandler{ "testHandler", 1s } {}

    void OnTimerExpired() override { LOG_INFO("[{}] timer expired", Name()); }
};

class TestTcpClient final : public TcpClient
{
public:
    TestTcpClient() : TcpClient{ "127.0.0.1", "8080" } {}

    void OnConnect() override { LOG_INFO("connected to '{}:{}'", m_host, m_port); }
};

} // namespace Sage

int main(int argc, char* const argv[])
{
    using namespace Sage;

    int res{ 0 };

    try
    {
        auto [logLevel, logFile]{ GetCliArgs(argc, argv) };
        Logger::SetupLogger(logFile, logLevel);

        LOG_INFO("cpp-io-uring-proactor starting");

        {
            std::shared_ptr<Proactor> proactor{ Proactor::Create() };

            {
                LogFileChecker logChecker{ Logger::EnsureLogFileExist };
                TestTimerHandler handler;
                TestTcpClient h2;
                proactor->Run();
            }

            Proactor::Destroy();
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("caught std exception. {}", e.what());
        res = 1;
    }

    LOG_INFO("cpp-io-uring-proactor exiting code({})", res);

    return res;
}
