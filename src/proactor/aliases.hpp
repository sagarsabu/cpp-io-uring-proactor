#pragma once

#include <memory>

namespace Sage
{

class IOURing;
class Proactor;
class TimerHandler;
class Event;

using SharedProactor = std::shared_ptr<Proactor>;

} // namespace Sage
