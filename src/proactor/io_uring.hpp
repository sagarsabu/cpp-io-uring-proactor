#pragma once

#include <liburing.h>
#include <memory>
#include <functional>

#include "proactor/time.hpp"

namespace Sage
{

using UniqueUringCEvent = std::unique_ptr<io_uring_cqe, std::function<void(io_uring_cqe*)>>;

class IOURing;

using SharedURing = std::shared_ptr<IOURing>;

class IOURing final : public std::enable_shared_from_this<IOURing>
{
public:
    // usually a casted raw pointer
    using UserData = decltype(io_uring_sqe{}.user_data);

    static SharedURing Create(uint queueSize);

    ~IOURing();

    UniqueUringCEvent WaitForEvent();

    bool QueueTimeoutEvent(const UserData& data, TimeNS timeout);

    bool CancelTimeoutEvent(const UserData& data);

private:
    IOURing(const IOURing&) = delete;
    IOURing(IOURing&&) = delete;
    IOURing& operator=(const IOURing&) = delete;
    IOURing& operator=(IOURing&&) = delete;

    explicit IOURing(uint queueSize);

    io_uring_sqe* GetSubmissionEvent();

    bool SubmitEvents();

    struct io_uring m_rawIOURing { };
    const uint m_queueSize;
};

} // namespace Sage
