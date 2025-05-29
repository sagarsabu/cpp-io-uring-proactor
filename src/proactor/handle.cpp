#include <algorithm>
#include <limits>

#include "proactor/handle.hpp"

namespace Sage::Handle
{

Id NextId() noexcept
{
    static Id id{ 0 };
    id = std::min(id + 1, std::numeric_limits<Id>::max() - 1);
    return id;
}

} // namespace Sage::Handle
