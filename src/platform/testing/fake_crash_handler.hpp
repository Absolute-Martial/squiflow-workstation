#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "platform/crash_breadcrumb.hpp"
#include "platform/crash_handler.hpp"
#include "platform/log_sink.hpp"

namespace squiflow::platform::testing {

class FakeCrashHandler final : public CrashHandler {
public:
    bool install(const std::string& crash_directory, LogSink& direct_sink,
                 CrashBreadcrumb& breadcrumb) override {
        if (state_ != CrashHandlerState::Idle || crash_directory.empty() ||
            !std::filesystem::path(crash_directory).is_absolute()) {
            return false;
        }
        std::error_code error;
        std::filesystem::create_directories(crash_directory, error);
        if (error || !std::filesystem::is_directory(crash_directory, error)) {
            return false;
        }
        directory_ = crash_directory;
        sink_ = &direct_sink;
        breadcrumb_ = &breadcrumb;
        state_ = CrashHandlerState::Installed;
        return true;
    }

    void uninstall() noexcept override {
        sink_ = nullptr;
        breadcrumb_ = nullptr;
        directory_.clear();
        state_ = CrashHandlerState::Idle;
    }

    CrashHandlerState state() const noexcept override { return state_; }

    CrashRecord trigger_test_crash(std::string_view reason) override {
        CrashRecord record;
        if (state_ != CrashHandlerState::Installed || sink_ == nullptr ||
            breadcrumb_ == nullptr || reason.empty()) {
            return record;
        }
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        record.timestamp_milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        record.reason.assign(reason.substr(0, 120));
        const std::string stem = directory_ + "/crash_test_" +
                                 std::to_string(++sequence_);
        record.dump_path = stem + ".dmp";
        record.report_path = stem + ".txt";
        record.breadcrumb_path = stem + ".breadcrumb.txt";

        record.dump_written = write_text(record.dump_path,
                                         "Synthetic CrashCatch dump\n" +
                                             record.reason + "\n");
        record.report_written = write_text(
            record.report_path,
            "Crash Report\n============\nReason: " + record.reason + "\n");
        record.breadcrumb_written =
            breadcrumb_->dump_to_file(record.breadcrumb_path);

        const std::string crash_line =
            "fatal crash captured reason=" + record.reason;
        static_cast<void>(sink_->write_line(crash_line));
        sink_->flush();
        record.restart_available = true;
        state_ = CrashHandlerState::Crashed;
        last_record_ = record;
        return record;
    }

    const CrashRecord& last_record() const noexcept { return last_record_; }

private:
    static bool write_text(const std::string& path,
                           const std::string& text) noexcept {
        try {
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            output.write(text.data(), static_cast<std::streamsize>(text.size()));
            output.flush();
            return output.good();
        } catch (...) {
            return false;
        }
    }

    CrashHandlerState state_{CrashHandlerState::Idle};
    std::string directory_{};
    LogSink* sink_{nullptr};
    CrashBreadcrumb* breadcrumb_{nullptr};
    std::uint64_t sequence_{0};
    CrashRecord last_record_{};
};

}  // namespace squiflow::platform::testing
