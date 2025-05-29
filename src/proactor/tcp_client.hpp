#pragma once

#include "proactor/handle.hpp"
#include "proactor/proactor.hpp"
#include "proactor/timer_handler.hpp"

#include <cstdint>
#include <queue>
#include <span>
#include <string>
#include <string_view>

namespace Sage
{

class TcpClient : public TimerHandler
{
public:
    enum ConnectionState
    {
        Unknown = 0,
        Broken,
        Connecting,
        Connected
    };

    TcpClient(const std::string& host, const std::string& port);

    ~TcpClient() override;

    std::string_view ClientName() const noexcept;

protected:
    virtual void OnConnect() = 0;

    virtual void OnReceive(std::span<uint8_t> buff) = 0;

private:
    void OnTimerExpired() override;

    void SendPending();

    void QueueRecv();

    bool IsSocketConnected();

    std::string m_host;
    std::string m_port;
    std::string m_tag;
    int m_fd{ -1 };
    ConnectionState m_state{ Unknown };
    const Handle::Id m_id{ Handle::NextId() };
    std::queue<std::string> m_txBuffer;
    bool m_rxPending{ false };

    friend class Proactor;
};

} // namespace Sage
