#include "proactor/tcp_client.hpp"
#include "log/logger.hpp"
#include "proactor/proactor.hpp"

#include <cerrno>
#include <cstring>
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
        {
            Proactor::Instance()->StartSocketClient(*this);
            UpdateInterval(1s);
            break;
        }

        case Connecting:
        {
            UpdateInterval(50ms);
            break;
        }

        // TODO: check its still connected
        case Connected:
        {
            if (not CheckSocketConnected())
            {
                m_state = Broken;
                if (::close(m_fd) != 0)
                {
                    int err{ errno };
                    LOG_ERROR("failed to close fd. {}", strerror(err));
                }
                m_fd = -1;
            }

            UpdateInterval(20ms);
            break;
        }
    }
}

bool TcpClient::CheckSocketConnected()
{
    char noop;
    ssize_t result = ::recv(m_fd, &noop, sizeof(noop), MSG_PEEK | MSG_DONTWAIT);
    if (result == 0)
    {
        return false;
    }

    if (result > 0)
    {
        return true;
    }

    int err{ errno };
    switch (err)
    {
        case EINTR:
        case EWOULDBLOCK:
            return true;

        default:
            return false;
    }
}

} // namespace Sage
