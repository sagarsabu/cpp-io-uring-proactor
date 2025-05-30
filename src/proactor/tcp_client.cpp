#include "proactor/tcp_client.hpp"
#include "log/logger.hpp"
#include "proactor/proactor.hpp"
#include "timing/time.hpp"

#include <cerrno>
#include <cstring>
#include <format>
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
    LOG_DEBUG("[{}] c'tor", ClientName());
    Proactor::Instance().AddSocketClient(*this);
}

TcpClient::~TcpClient()
{
    LOG_DEBUG("[{}] d'tor", ClientName());
    Proactor::Instance().RemoveSocketClient(*this);
}

std::string_view TcpClient::ClientName() const noexcept { return m_tag; }

void TcpClient::OnTimerExpired()
{
    switch (m_state)
    {
        case Unknown:
        case Broken:
        {
            Proactor::Instance().StartSocketClient(*this);
            UpdateInterval(1s);
            break;
        }

        case Connecting:
        {
            UpdateInterval(50ms);
            break;
        }

        case Connected:
        {
            if (not IsSocketConnected())
            {
                UpdateInterval(20ms);
                if (::close(m_fd) != 0)
                {
                    int err{ errno };
                    LOG_ERROR("failed to close fd. {}", strerror(err));
                }
                m_state = Broken;
                m_rxPending = false;
                m_fd = -1;
            }
            else
            {
                UpdateInterval(5s);

                Timestamp ts{ GetCurrentTimeStamp() };
                m_txBuffer.emplace(std::format("client said hi at {}{}\n", ts.m_date, ts.m_ns));

                SendPending();
                QueueRecv();
            }

            break;
        }
    }
}

void TcpClient::SendPending()
{
    while (not m_txBuffer.empty())
    {
        std::string data{ m_txBuffer.front() };
        m_txBuffer.pop();
        Proactor::Instance().RequestTcpSend(*this, std::move(data));
    }
}

void TcpClient::QueueRecv()
{
    if (not m_rxPending)
    {
        Proactor::Instance().RequestTcpRecv(*this);
        m_rxPending = true;
    }
}

bool TcpClient::IsSocketConnected()
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
