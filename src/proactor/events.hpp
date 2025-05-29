#pragma once

#include <functional>
#include <liburing.h>
#include <limits>
#include <string>
#include <sys/signalfd.h>

#include "proactor/handle.hpp"
#include "utils/demangled_name.hpp"

namespace Sage
{

using EventId = size_t;

class Event
{
public:
    using OnCompleteFunc = std::move_only_function<void(Event&, const io_uring_cqe&)>;

    virtual ~Event() noexcept = default;

    std::string NameAndType() const { return DemangleTypeName(*this); }

    const EventId m_id{ NextId() };
    const Handle::Id m_handlerId;
    OnCompleteFunc m_onCompleteCb;
    bool m_removeOnComplete{ true };

protected:
    Event(Handle::Id handlerId, OnCompleteFunc&& onComplete) noexcept :
        m_handlerId{ handlerId },
        m_onCompleteCb{ std::move(onComplete) }
    {
    }

private:
    Event() = delete;

    static EventId NextId() noexcept
    {
        static EventId id{ 0 };
        id = std::min(id + 1, std::numeric_limits<EventId>::max() - 1);
        return id;
    }
};



} // namespace Sage
