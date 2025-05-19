#pragma once

#include "proactor/handler.hpp"
#include "proactor/proactor.hpp"
#include "proactor/timer_handler.hpp"

#include <string>
#include <string_view>

namespace Sage
{

class TcpClient : public TimerHandler
{
public:
    enum State
    {
        Unknown = 0,
        Broken,
        Connecting,
        Connected
    };

    TcpClient(const std::string& host, const std::string& port);

    ~TcpClient();

    std::string_view Name() noexcept;

    virtual void OnConnect() = 0;

    void OnTimerExpired() override;

    std::string m_host;
    std::string m_port;
    std::string m_tag;
    State m_state{ Unknown };

private:
    const Handler::Id m_id{ Handler::NextId() };

    friend class Proactor;
};

} // namespace Sage
