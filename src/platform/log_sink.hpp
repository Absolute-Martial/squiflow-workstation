#pragma once

// Where a formatted line goes.
//
// The logger knows nothing about files. That is what lets the same logger be
// pointed at a rotating file on the shop counter, at a recording sink in a
// test, and later at a second sink for the crash reporter, without any of
// them knowing about each other.
//
// A sink never throws. Losing a log line must never break the operation that
// produced it, so a failure is reported as false and counted, not raised.

#include <string_view>

namespace squiflow::platform {

class LogSink {
public:
    virtual ~LogSink() = default;

    LogSink(const LogSink&) = delete;
    LogSink& operator=(const LogSink&) = delete;
    LogSink(LogSink&&) = delete;
    LogSink& operator=(LogSink&&) = delete;

    // Writes one complete line. The sink appends the line ending.
    virtual bool write_line(std::string_view line) = 0;

    // Pushes anything buffered towards the disk. Called on shutdown and after
    // anything at Error or above, because the interesting lines are the ones
    // written just before a machine is switched off at the wall.
    virtual void flush() = 0;

protected:
    LogSink() = default;
};

}  // namespace squiflow::platform
