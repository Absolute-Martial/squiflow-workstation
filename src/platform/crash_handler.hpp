#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace squiflow::platform {

class CrashBreadcrumb;
class LogSink;

enum class CrashHandlerState : std::uint8_t {
    Idle,
    Installed,
    Crashed,
};

struct CrashRecord {
    std::string dump_path{};
    std::string report_path{};
    std::string breadcrumb_path{};
    std::string reason{};
    std::int64_t timestamp_milliseconds{0};
    bool dump_written{false};
    bool report_written{false};
    bool breadcrumb_written{false};
    bool restart_available{false};
};

class CrashHandler {
public:
    CrashHandler(const CrashHandler&) = delete;
    CrashHandler& operator=(const CrashHandler&) = delete;
    CrashHandler(CrashHandler&&) = delete;
    CrashHandler& operator=(CrashHandler&&) = delete;
    virtual ~CrashHandler() = default;

    virtual bool install(const std::string& crash_directory,
                         LogSink& direct_sink,
                         CrashBreadcrumb& breadcrumb) = 0;
    virtual void uninstall() noexcept = 0;
    virtual CrashHandlerState state() const noexcept = 0;

    // Test seam. A production handler may terminate rather than return. The
    // deterministic fake writes equivalent artifacts and returns their record.
    virtual CrashRecord trigger_test_crash(std::string_view reason) = 0;

protected:
    CrashHandler() = default;
};

// The host-platform CrashCatch adapter. The dependency stays behind this
// factory so no CrashCatch type crosses the platform boundary.
std::unique_ptr<CrashHandler> make_crash_handler();

}  // namespace squiflow::platform
