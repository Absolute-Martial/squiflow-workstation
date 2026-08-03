#include "platform/crash_handler.hpp"

#if defined(_WIN32)

#include <atomic>
#include <filesystem>
#include <memory>
#include <string>

#include <CrashCatch.hpp>

#include "platform/crash_breadcrumb.hpp"
#include "platform/log_sink.hpp"

namespace squiflow::platform {
namespace {

std::atomic<LogSink*> g_sink{nullptr};
std::atomic<CrashBreadcrumb*> g_ring{nullptr};
std::atomic<std::atomic<CrashHandlerState>*> g_state{nullptr};

void after_crash(const CrashCatch::CrashContext& context) noexcept {
    try {
        if (auto* state = g_state.load(std::memory_order_acquire)) {
            state->store(CrashHandlerState::Crashed, std::memory_order_release);
        }
        if (auto* ring = g_ring.load(std::memory_order_acquire)) {
            std::filesystem::path path = context.logFilePath;
            path.replace_extension(".breadcrumb.txt");
            static_cast<void>(ring->dump_to_file(path.string()));
        }
        if (auto* sink = g_sink.load(std::memory_order_acquire)) {
            static_cast<void>(sink->write_line(
                "fatal crash captured by CrashCatch"));
            sink->flush();
        }
    } catch (...) {
    }
}

class WindowsCrashCatchHandler final : public CrashHandler {
public:
    ~WindowsCrashCatchHandler() override { uninstall(); }

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

        // Read the previous process filter without losing it during startup.
        previous_ = SetUnhandledExceptionFilter(nullptr);
        SetUnhandledExceptionFilter(previous_);

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

        g_sink.store(&direct_sink, std::memory_order_release);
        g_ring.store(&breadcrumb, std::memory_order_release);
        g_state.store(&state_, std::memory_order_release);
        if (!CrashCatch::initialize(config)) {
            clear_bridges();
            return false;
        }
        state_.store(CrashHandlerState::Installed, std::memory_order_release);
        return true;
    }

    void uninstall() noexcept override {
        if (state_.load(std::memory_order_acquire) == CrashHandlerState::Idle) {
            return;
        }
        clear_bridges();
        SetUnhandledExceptionFilter(previous_);
        CrashCatch::globalConfig.onCrash = nullptr;
        CrashCatch::globalConfig.onCrashUpload = nullptr;
        SymCleanup(GetCurrentProcess());
        state_.store(CrashHandlerState::Idle, std::memory_order_release);
    }

    CrashHandlerState state() const noexcept override {
        return state_.load(std::memory_order_acquire);
    }

    CrashRecord trigger_test_crash(std::string_view) override { return {}; }

private:
    static void clear_bridges() noexcept {
        g_state.store(nullptr, std::memory_order_release);
        g_ring.store(nullptr, std::memory_order_release);
        g_sink.store(nullptr, std::memory_order_release);
    }

    std::atomic<CrashHandlerState> state_{CrashHandlerState::Idle};
    LPTOP_LEVEL_EXCEPTION_FILTER previous_{nullptr};
};

}  // namespace

std::unique_ptr<CrashHandler> make_crash_handler() {
    return std::make_unique<WindowsCrashCatchHandler>();
}

}  // namespace squiflow::platform

#endif
