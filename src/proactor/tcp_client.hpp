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

class TcpConnect final : public Event
{
public:
    TcpConnect(Handle::Id handlerId, OnCompleteFunc&& onComplete, const std::string& host, const std::string& port) :
        Event{ handlerId, std::move(onComplete) },
        m_host{ host },
        m_port{ port }
    {
    }

    std::string m_host;
    std::string m_port;
    int m_fd{ -1 };
};

class TcpSend final : public Event
{
public:
    TcpSend(
        Handle::Id handlerId, OnCompleteFunc&& onComplete, const std::string& host, const std::string& port, int fd,
        std::string data
    ) :
        Event{ handlerId, std::move(onComplete) },
        m_host{ host },
        m_port{ port },
        m_fd{ fd },
        m_data{ std::move(data) }
    {
    }

    std::string m_host;
    std::string m_port;
    int m_fd;
    std::string m_data;
};

class TcpRecv final : public Event
{
public:
    TcpRecv(
        Handle::Id handlerId, OnCompleteFunc&& onComplete, const std::string& host, const std::string& port, int fd
    ) :
        Event{ handlerId, std::move(onComplete) },
        m_host{ host },
        m_port{ port },
        m_fd{ fd }
    {
    }

    std::string m_host;
    std::string m_port;
    int m_fd;
    IOURing::RxBuffer m_data{};
};

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
