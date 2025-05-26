#include <cerrno>
#include <cstring>
#include <netdb.h>
#include <sys/socket.h>

#include "log/logger.hpp"
#include "proactor/io_uring.hpp"

namespace Sage
{

IOURing::IOURing(uint queueSize) : m_queueSize{ queueSize }
{
    io_uring_queue_init(
        m_queueSize,
        &m_rawIOURing,
        // Single threaded, so single issuer optimization
        IORING_SETUP_SINGLE_ISSUER
    );
}

IOURing::~IOURing() { io_uring_queue_exit(&m_rawIOURing); }

UniqueUringCEvent IOURing::WaitForEvent()
{
    LOG_TRACE("Waiting for events to populate");

    io_uring_cqe* rawCEvent{ nullptr };
    if (int res = io_uring_wait_cqe(&m_rawIOURing, &rawCEvent); res < 0)
    {
        // Ignore interrupts. i.e debugger pause / suspend
        if (res != -EINTR)
        {
            LOG_ERROR("failed to waiting for event completion. {}", strerror(-res));
        }
        return nullptr;
    }

    return UniqueUringCEvent{ rawCEvent,
                              [this](io_uring_cqe* event)
                              {
                                  if (event != nullptr)
                                  {
                                      io_uring_cqe_seen(&m_rawIOURing, event);
                                  }
                              } };
}

bool IOURing::QueueTimeoutEvent(const UserData& data, const TimeNS& timeout)
{
    io_uring_sqe* submissionEvent{ GetSubmissionEvent() };
    if (submissionEvent == nullptr)
    {
        return false;
    }

    submissionEvent->user_data = data;
    __kernel_timespec ts{ ChronoTimeToKernelTimeSpec(timeout) };
    io_uring_prep_timeout(
        submissionEvent,
        &ts,
        0,
        // ensure timeout keeps firing without rearming
        IORING_TIMEOUT_MULTISHOT | IORING_TIMEOUT_BOOTTIME
    );

    return SubmitEvents();
}

bool IOURing::CancelTimeoutEvent(const UserData& cancelData, const UserData& timeoutData)
{
    io_uring_sqe* submissionEvent{ GetSubmissionEvent() };
    if (submissionEvent == nullptr)
    {
        return false;
    }

    submissionEvent->user_data = cancelData;
    io_uring_prep_timeout_remove(submissionEvent, timeoutData, 0);

    return SubmitEvents();
}

bool IOURing::UpdateTimeoutEvent(const UserData& updateData, const UserData& timeoutData, const TimeNS& timeout)
{
    io_uring_sqe* submissionEvent{ GetSubmissionEvent() };
    if (submissionEvent == nullptr)
    {
        return false;
    }

    submissionEvent->user_data = updateData;
    __kernel_timespec ts{ ChronoTimeToKernelTimeSpec(timeout) };
    io_uring_prep_timeout_update(submissionEvent, &ts, timeoutData, 0);

    return SubmitEvents();
}

bool IOURing::QueueSignalRead(const UserData& data, int fd, signalfd_siginfo& readBuff)
{
    io_uring_sqe* submissionEvent{ GetSubmissionEvent() };
    if (submissionEvent == nullptr)
    {
        return false;
    }

    submissionEvent->user_data = data;
    io_uring_prep_read(submissionEvent, fd, &readBuff, sizeof(signalfd_siginfo), 0);

    return SubmitEvents();
}

int IOURing::QueueTcpConnect(const UserData& data, const std::string& host, const std::string& port)
{
    io_uring_sqe* submissionEvent{ GetSubmissionEvent() };
    if (submissionEvent == nullptr)
    {
        return -1;
    }

    struct addrinfo hints{};
    struct addrinfo* res{ nullptr };

    if (int err{ getaddrinfo(host.c_str(), port.c_str(), &hints, &res) }; err != 0 or res == nullptr)
    {
        LOG_ERROR("failed to create socket. res-nullptr?{} e={}", res == nullptr, gai_strerror(err));
        return -1;
    }

    int err{ 0 };
    int sockFd{ -1 };
    struct sockaddr addr{};
    socklen_t addLen{};

    for (auto iter{ res }; iter != nullptr; iter = iter->ai_next)
    {
        sockFd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockFd == -1)
        {
            err = errno;
            continue;
        }

        addr = *res->ai_addr;
        addLen = res->ai_addrlen;
    }
    freeaddrinfo(res);

    if (sockFd == -1)
    {
        LOG_ERROR("failed to create socket. e={}", strerror(err));
        return -1;
    }

    submissionEvent->user_data = data;
    io_uring_prep_connect(submissionEvent, sockFd, &addr, addLen);

    if (SubmitEvents())
    {
        return sockFd;
    }

    if (::close(sockFd) != 0)
    {
        int err{ errno };
        LOG_ERROR("failed to closed fd. {}", strerror(err));
    }
    return -1;
}


bool IOURing::QueueRcpRecv(const UserData&, const std::string&, const std::string&)
{
    // TODO

    // io_uring_sqe* submissionEvent{ GetSubmissionEvent() };
    // if (submissionEvent == nullptr)
    // {
    //     return false;
    // }

    // submissionEvent->user_data = data;

    // return SubmitEvents();
    return false;
}

bool IOURing::SubmitEvents()
{
    int res{ io_uring_submit(&m_rawIOURing) };
    bool success{ res >= 0 };

    if (success)
    {
        LOG_TRACE("submitted {} event(s)", res);
    }
    else
    {
        LOG_ERROR("failed. {}", strerror(-res));
    }

    return success;
}

io_uring_sqe* IOURing::GetSubmissionEvent()
{
    io_uring_sqe* submissionEvent{ io_uring_get_sqe(&m_rawIOURing) };
    if (submissionEvent == nullptr)
    {
        LOG_ERROR("failed. submission queue may be full?");
    }

    return submissionEvent;
}

} // namespace Sage
