#include <cstring>

#include "log/logger.hpp"
#include "proactor/io_uring.hpp"

namespace Sage
{

SharedURing IOURing::Create(uint queueSize)
{
    return SharedURing{ new IOURing{ queueSize } };
}

IOURing::IOURing(uint queueSize) :
    m_queueSize{ queueSize }
{
    io_uring_queue_init(m_queueSize, &m_rawIOURing, 0);
}

IOURing::~IOURing()
{
    io_uring_queue_exit(&m_rawIOURing);
}

UniqueUringCEvent IOURing::WaitForEvent()
{
    LOG_TRACE("%s Waiting for events to populate", __func__);

    io_uring_cqe* rawCEvent{ nullptr };
    if (int res = io_uring_wait_cqe(&m_rawIOURing, &rawCEvent);
        res < 0)
    {
        LOG_ERROR("%s failed to waiting for event completion. %s", __func__, strerror(-res));
        return nullptr;
    }

    return UniqueUringCEvent
    {
        rawCEvent,
        [self = shared_from_this()](io_uring_cqe* event)
        {
            if (event != nullptr)
            {
                io_uring_cqe_seen(&self->m_rawIOURing, event);
            }
        }
    };
}

bool IOURing::QueueTimeoutEvent(const UserData& data, TimeNS timeout)
{
    io_uring_sqe* submissionEvent{ GetSubmissionEvent() };
    if (submissionEvent == nullptr)
    {
        return false;
    }

    submissionEvent->user_data = data;
    __kernel_timespec ts{ ChronoTimeToKernelTimeSpec(timeout) };
    io_uring_prep_timeout(submissionEvent, &ts, 0, 0);

    return SubmitEvents();
}

bool IOURing::CancelTimeoutEvent(const UserData& data)
{
    io_uring_sqe* submissionEvent{ GetSubmissionEvent() };
    if (submissionEvent == nullptr)
    {
        return false;
    }

    submissionEvent->user_data = data;
    io_uring_prep_timeout_remove(submissionEvent, data, 0);

    return SubmitEvents();
}

bool IOURing::SubmitEvents()
{
    int res{ io_uring_submit(&m_rawIOURing) };
    bool success{ res >= 0 };

    if (success)
    {
        LOG_TRACE("%s submitted %d events", __func__, res);
    }
    else
    {
        LOG_ERROR("%s failed. %s", __func__, strerror(-res));
    }

    return success;
}

io_uring_sqe* IOURing::GetSubmissionEvent()
{
    io_uring_sqe* submissionEvent{ io_uring_get_sqe(&m_rawIOURing) };
    if (submissionEvent == nullptr)
    {
        LOG_ERROR("%s failed. submission queue may be full?", __func__);
    }

    return submissionEvent;
}

} // namespace Sage
