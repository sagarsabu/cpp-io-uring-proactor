#include <string_view>

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
    TestTimerHandler() : TimerHandler{ "test-timer", 1s } {}

    void OnTimerExpired() override { LOG_INFO("[{}] timer expired", Name()); }
};

class TestTcpClient final : public TcpClient
{
public:
    TestTcpClient() : TcpClient{ "127.0.0.1", "8080" } {}

    void OnConnect() override { LOG_INFO("[{}] connected", Name()); }

    void OnReceive(std::span<uint8_t> buff) override
    {
        std::string_view data{ reinterpret_cast<char*>(buff.data()), buff.size() };
        if (data.ends_with('\n'))
        {
            data.remove_suffix(1);
        }

        LOG_INFO("[{}] rx data: {}", Name(), data);
    }
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
            Proactor::Create();

            {
                LogFileChecker logChecker{ Logger::EnsureLogFileExist };
                TestTimerHandler handler;
                TestTcpClient h2;
                Proactor::Instance().Run();
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
