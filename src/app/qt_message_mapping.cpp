#include "app/qt_message_mapping.hpp"
namespace squiflow::app {namespace {thread_local bool inside=false;}
platform::LogLevel map_qt_message(QtMessageKind k)noexcept{switch(k){case QtMessageKind::Debug:return platform::LogLevel::Debug;case QtMessageKind::Info:return platform::LogLevel::Info;case QtMessageKind::Warning:return platform::LogLevel::Warning;case QtMessageKind::Critical:return platform::LogLevel::Error;case QtMessageKind::Fatal:return platform::LogLevel::Fatal;case QtMessageKind::Unknown:return platform::LogLevel::Warning;}return platform::LogLevel::Warning;}
QtMessageRecursionGuard::QtMessageRecursionGuard()noexcept{if(!inside){inside=true;entered_=true;}}
QtMessageRecursionGuard::~QtMessageRecursionGuard(){if(entered_)inside=false;}
}