#include <cstring>

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

bool IOURing::QueueTimeoutEvent(const UserData& data, TimeNS timeout)
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
        IORING_TIMEOUT_MULTISHOT
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
