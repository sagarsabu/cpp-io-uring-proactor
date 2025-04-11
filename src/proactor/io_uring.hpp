#pragma once

#include <memory>
#include <functional>
#include <sys/signalfd.h>

#include "timing/time.hpp"

#pragma GCC system_header
#include <liburing.h>

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

private:
    IOURing(const IOURing&) = delete;
    IOURing(IOURing&&) = delete;
    IOURing& operator=(const IOURing&) = delete;
    IOURing& operator=(IOURing&&) = delete;

    io_uring_sqe* GetSubmissionEvent();

    bool SubmitEvents();

    struct io_uring m_rawIOURing { };
    const uint m_queueSize;
};

} // namespace Sage
