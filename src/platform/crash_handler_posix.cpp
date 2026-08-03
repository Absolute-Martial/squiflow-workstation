#include "platform/crash_handler.hpp"

#if !defined(_WIN32)

#include <array>
#include <atomic>
#include <csignal>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

#include <CrashCatch.hpp>

#include "platform/crash_breadcrumb.hpp"
#include "platform/log_sink.hpp"

namespace squiflow::platform {
namespace {

constexpr std::array<int, 5> kSignals{
    SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS};

std::atomic<LogSink*> g_direct_sink{nullptr};
std::atomic<CrashBreadcrumb*> g_breadcrumb{nullptr};
std::atomic<std::atomic<CrashHandlerState>*> g_state{nullptr};

std::string breadcrumb_path(const std::filesystem::path& report) {
    std::filesystem::path path = report;
    path.replace_extension(".breadcrumb.txt");
    return path.string();
}

void after_crash(const CrashCatch::CrashContext& context) noexcept {
    try {
        std::atomic<CrashHandlerState>* state =
            g_state.load(std::memory_order_acquire);
        if (state != nullptr) {
            state->store(CrashHandlerState::Crashed, std::memory_order_release);
        }
        CrashBreadcrumb* ring = g_breadcrumb.load(std::memory_order_acquire);
        if (ring != nullptr) {
            static_cast<void>(ring->dump_to_file(
                breadcrumb_path(context.logFilePath)));
        }
        LogSink* sink = g_direct_sink.load(std::memory_order_acquire);
        if (sink != nullptr) {
            static_cast<void>(sink->write_line(
                "fatal crash captured by CrashCatch"));
            sink->flush();
        }
    } catch (...) {
        // A crash callback has no recovery path. CrashCatch still owns the
        // primary report even when this best-effort bridge cannot add to it.
    }
}

class PosixCrashCatchHandler final : public CrashHandler {
public:
    ~PosixCrashCatchHandler() override { uninstall(); }

    bool install(const std::string& crash_directory, LogSink& direct_sink,
                 CrashBreadcrumb& breadcrumb) override {
        if (state_.load(std::memory_order_acquire) != CrashHandlerState::Idle ||
            crash_directory.empty()) {
            return false;
        }
        const std::filesystem::path directory(crash_directory);
        if (!directory.is_absolute()) {
            return false;
        }
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        if (error || !std::filesystem::is_directory(directory, error)) {
            return false;
        }

        for (std::size_t index = 0; index < kSignals.size(); ++index) {
            if (::sigaction(kSignals[index], nullptr, &previous_[index]) != 0) {
                return false;
            }
        }

        CrashCatch::Config config;
        config.dumpFolder = directory;
        config.dumpFileName = "crash";
        config.enableTextLog = true;
        config.autoTimestamp = true;
        config.showCrashDialog = false;
        config.includeStackTrace = true;
        config.appVersion = "SquiFlow workstation";
        config.additionalNotes =
            "Restart is offered by the SquiFlow shell, never by CrashCatch.";
        config.onCrash = after_crash;
        config.onCrashUpload = nullptr;

        g_direct_sink.store(&direct_sink, std::memory_order_release);
        g_breadcrumb.store(&breadcrumb, std::memory_order_release);
        g_state.store(&state_, std::memory_order_release);
        if (!CrashCatch::initialize(config)) {
            clear_bridges();
            return false;
        }
        directory_ = crash_directory;
        state_.store(CrashHandlerState::Installed, std::memory_order_release);
        return true;
    }

    void uninstall() noexcept override {
        if (state_.load(std::memory_order_acquire) == CrashHandlerState::Idle) {
            return;
        }
        clear_bridges();
        for (std::size_t index = 0; index < kSignals.size(); ++index) {
            static_cast<void>(::sigaction(kSignals[index], &previous_[index],
                                          nullptr));
        }
        CrashCatch::globalConfig.onCrash = nullptr;
        CrashCatch::globalConfig.onCrashUpload = nullptr;
        directory_.clear();
        state_.store(CrashHandlerState::Idle, std::memory_order_release);
    }

    CrashHandlerState state() const noexcept override {
        return state_.load(std::memory_order_acquire);
    }

    CrashRecord trigger_test_crash(std::string_view) override {
        // A real CrashCatch signal path terminates the process. Tests use the
        // deterministic fake or a dedicated child process instead.
        return {};
    }

private:
    void clear_bridges() noexcept {
        g_state.store(nullptr, std::memory_order_release);
        g_breadcrumb.store(nullptr, std::memory_order_release);
        g_direct_sink.store(nullptr, std::memory_order_release);
    }

    std::atomic<CrashHandlerState> state_{CrashHandlerState::Idle};
    std::string directory_{};
    std::array<struct sigaction, kSignals.size()> previous_{};
};

}  // namespace

std::unique_ptr<CrashHandler> make_crash_handler() {
    return std::make_unique<PosixCrashCatchHandler>();
}

}  // namespace squiflow::platform

#endif
