#include "proactor/tcp_client.hpp"
#include "proactor/proactor.hpp"

#include <string>
#include <string_view>

namespace Sage
{

TcpClient::TcpClient(const std::string& host, const std::string& port) :
    TimerHandler{ host + '@' + port, 1s },
    m_host{ host },
    m_port{ port },
    m_tag{ host + '@' + port }
{
    Proactor::Instance()->AddSocketClient(*this);
}

TcpClient::~TcpClient() { Proactor::Instance()->RemoveSocketClient(*this); }

std::string_view TcpClient::Name() noexcept { return m_tag; }

void TcpClient::OnTimerExpired()
{
    switch (m_state)
    {
        case Unknown:
        case Broken:
            Proactor::Instance()->StartSocketClient(*this);
            break;

        case Connecting:
            break;

        // TODO: check its still connected
        case Connected:
            break;
    }
}

} // namespace Sage
