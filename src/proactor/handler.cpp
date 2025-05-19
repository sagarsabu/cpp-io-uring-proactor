#include <algorithm>
#include <limits>

#include "proactor/handler.hpp"

namespace Sage::Handler
{

Id NextId() noexcept
{
    static Id id{ 0 };
    id = std::min(id + 1, std::numeric_limits<Id>::max() - 1);
    return id;
}

} // namespace Sage::Handler
