#pragma once
#include "platform/log_record.hpp"
#include <string_view>
namespace squiflow::app {
enum class QtMessageKind {Debug,Info,Warning,Critical,Fatal,Unknown};
platform::LogLevel map_qt_message(QtMessageKind)noexcept;
class QtMessageRecursionGuard final{public:QtMessageRecursionGuard()noexcept;~QtMessageRecursionGuard();bool entered()const noexcept{return entered_;}private:bool entered_{false};};
}
