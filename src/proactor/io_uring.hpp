#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <liburing.h>
#include <memory>
#include <sys/signalfd.h>
#include <sys/types.h>

#include "timing/time.hpp"

namespace Sage
{

using UniqueUringCEvent = std::unique_ptr<io_uring_cqe, std::function<void(io_uring_cqe*)>>;

class IOURing final
{
public:
    // usually an id to reference against a map
    using UserData = decltype(io_uring_sqe{}.user_data);
    using RxBuffer = std::array<uint8_t, 1024>;

    explicit IOURing(uint queueSize);

    ~IOURing();

    UniqueUringCEvent WaitForEvent();

    bool QueueTimeoutEvent(const UserData& data, const TimeNS& timeout);

    bool CancelTimeoutEvent(const UserData& cancelData, const UserData& timeoutData);

    bool UpdateTimeoutEvent(const UserData& cancelData, const UserData& timeoutData, const TimeNS& timeout);

    bool QueueSignalRead(const UserData& data, int fd, signalfd_siginfo& readBuff);

    /// @returns fd
    int QueueTcpConnect(const UserData& data, const std::string& host, const std::string& port);

    bool QueueTcpSend(const UserData& data, int fd, std::string_view buffer);

    bool QueueTcpRecv(const UserData& data, int fd, RxBuffer& rxBuffer);

private:
    IOURing(const IOURing&) = delete;
    IOURing(IOURing&&) = delete;
    IOURing& operator=(const IOURing&) = delete;
    IOURing& operator=(IOURing&&) = delete;

    io_uring_sqe* GetSubmissionEvent();

    bool SubmitEvents();

    struct io_uring m_rawIOURing{};
    const uint m_queueSize;
};

} // namespace Sage
