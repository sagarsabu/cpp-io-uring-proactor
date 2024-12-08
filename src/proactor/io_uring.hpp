#pragma once

#include <liburing.h>
#include <memory>
#include <functional>

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

    bool CancelTimeoutEvent(const UserData& data);

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
