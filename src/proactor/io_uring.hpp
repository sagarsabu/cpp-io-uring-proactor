#pragma once

#include <functional>
#include <memory>
#include <sys/signalfd.h>

#include "proactor/sys_liburing.hpp" // IWYU pragma: keep
#include "timing/time.hpp"

namespace Sage
{

using UniqueUringCEvent = std::unique_ptr<io_uring_cqe, std::function<void(io_uring_cqe*)>>;

class IOURing final
{
public:
    // usually an id to reference against a map
    using UserData = decltype(io_uring_sqe{}.user_data);

    explicit IOURing(uint queueSize);

    ~IOURing();

    UniqueUringCEvent WaitForEvent();

    bool QueueTimeoutEvent(const UserData& data, TimeNS timeout);

    bool CancelTimeoutEvent(const UserData& cancelData, const UserData& timeoutData);

    bool QueueSignalRead(const UserData& data, int fd, signalfd_siginfo& readBuff);

    /// @returns fd
    int QueueTcpConnect(const UserData& data, const std::string& host, const std::string& port);

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
