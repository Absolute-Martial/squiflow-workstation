#include <filesystem>
#include <fstream>
#include <string>

#include "platform/crash_breadcrumb.hpp"
#include "platform/crash_handler.hpp"
#include "platform/logger.hpp"
#include "platform/testing/fake_crash_handler.hpp"
#include "platform/testing/manual_log_clock.hpp"
#include "platform/testing/recording_log_sink.hpp"
#include "support/check.hpp"

using squiflow::testing::check;
using squiflow::testing::section;
namespace p = squiflow::platform;

namespace {
std::string read(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}
}

int main() {
    const std::string directory = "/tmp/squiflow-phase63-crash";
    std::filesystem::remove_all(directory);

    section("breadcrumb retention and bounds");
    p::CrashBreadcrumb ring(2);
    check(ring.capacity() == 2, "requested capacity retained");
    ring.push(p::LogLevel::Info, "one", "first", 1);
    ring.push(p::LogLevel::Warning, "two", "second", 2);
    ring.push(p::LogLevel::Error, std::string(100, 'c'),
              std::string(300, 'm'), 3);
    check(ring.push_count() == 3, "push count is monotonic");
    const std::string crumbs = directory + ".breadcrumbs";
    check(ring.dump_to_file(crumbs), "breadcrumb file written");
    const std::string text = read(crumbs);
    check(text.find("first") == std::string::npos, "oldest entry evicted");
    check(text.find("second") < text.find("mmmm"), "retained entries oldest first");
    const p::BreadcrumbEntry latest = ring.entry_at(0);
    check(latest.valid && latest.category[p::kBreadcrumbCategoryLength - 1] == '\0',
          "long category is terminated");
    check(latest.message[p::kBreadcrumbMessageLength - 1] == '\0',
          "long message is terminated");
    check(!ring.dump_to_file(""), "empty destination refused");
    check(ring.dump_to_fd(-1) == 0, "invalid descriptor refused");
    p::CrashBreadcrumb clamped(0);
    check(clamped.capacity() == 1, "zero capacity clamps safely");

    section("logger publishes only passing records");
    p::testing::RecordingLogSink log_sink;
    p::testing::ManualLogClock clock(1000);
    p::Logger logger(log_sink, clock, p::LogLevel::Info);
    p::CrashBreadcrumb logger_ring(4);
    logger.set_breadcrumb(&logger_ring);
    logger.debug("hidden", "not retained");
    logger.info("work", "retained");
    check(logger_ring.push_count() == 1, "one passing record retained");
    logger.set_breadcrumb(nullptr);
    logger.error("detached", "not retained");
    check(logger_ring.push_count() == 1, "null pointer detaches ring");

    section("deterministic crash artifacts and direct sink bypass");
    p::testing::RecordingLogSink direct;
    p::testing::FakeCrashHandler fake;
    check(!fake.install("relative", direct, ring), "relative crash directory refused");
    check(fake.install(directory, direct, ring), "fake installs in absolute directory");
    check(!fake.install(directory, direct, ring), "duplicate install refused");
    const p::CrashRecord crash = fake.trigger_test_crash("SIGABRT test");
    check(crash.dump_written && std::filesystem::exists(crash.dump_path),
          "synthetic dump exists");
    check(crash.report_written && std::filesystem::exists(crash.report_path),
          "CrashCatch-style report exists");
    check(crash.breadcrumb_written &&
              std::filesystem::exists(crash.breadcrumb_path),
          "breadcrumb sidecar exists");
    check(read(crash.breadcrumb_path).find("second") != std::string::npos,
          "breadcrumb sidecar is readable");
    check(!direct.lines().empty(), "crash line reaches direct sink");
    check(direct.flushes() == 1, "direct sink flushed once");
    check(fake.state() == p::CrashHandlerState::Crashed,
          "fake records crashed state");
    check(crash.restart_available, "restart handoff is recorded");
    fake.uninstall();
    check(fake.state() == p::CrashHandlerState::Idle, "uninstall returns to idle");
    check(fake.install(directory, direct, ring), "handler can reinstall cleanly");
    fake.uninstall();

    section("CrashCatch platform adapter lifecycle");
    auto real = p::make_crash_handler();
    check(real->install(directory, direct, ring), "platform adapter installs");
    check(real->state() == p::CrashHandlerState::Installed,
          "platform adapter reports installed");
    check(!real->install(directory, direct, ring),
          "platform adapter refuses duplicate install");
    real->uninstall();
    check(real->state() == p::CrashHandlerState::Idle,
          "platform adapter restores idle state");

    return squiflow::testing::report();
}
