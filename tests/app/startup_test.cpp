#include "app/activation_controller.hpp"
#include "app/application.hpp"
#include "app/composition_root.hpp"
#include "app/qt_message_mapping.hpp"
#include "support/check.hpp"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace squiflow::app;

namespace {

struct Runtime final : StartupRuntime {
    std::vector<StartupStep> started{};
    std::vector<StartupStep> stopped{};
    std::vector<ShutdownReason> stop_reasons{};
    std::vector<RollbackFailure> diagnostics{};
    int fail_at{-1};
    int secondary_at{-1};
    std::optional<StartupStep> throw_stop{};
    StartupSequence* sequence{nullptr};

    std::mutex gate_mutex{};
    std::condition_variable gate_changed{};
    int block_at{-1};
    bool callback_entered{false};
    bool release_callback{false};

    StepResult start(StartupStep step) override {
        started.push_back(step);
        const int index = static_cast<int>(started.size()) - 1;
        if (index == block_at) {
            std::unique_lock lock(gate_mutex);
            callback_entered = true;
            gate_changed.notify_all();
            gate_changed.wait(lock, [this] { return release_callback; });
        }
        if (index == secondary_at) {
            return {StepDisposition::SecondaryInstance, {}};
        }
        if (index == fail_at) {
            return {StepDisposition::Failed, "failed"};
        }
        if (index == 4 && sequence != nullptr) {
            sequence->shutdown(ShutdownReason::WindowClosed);
        }
        return {};
    }

    void stop(StartupStep step, ShutdownReason reason) override {
        stopped.push_back(step);
        stop_reasons.push_back(reason);
        if (throw_stop && step == *throw_stop) {
            throw std::runtime_error("rollback failed");
        }
    }

    void rollback_diagnostic(const RollbackFailure& failure) noexcept override {
        diagnostics.push_back(failure);
    }
};

struct Surface final : ApplicationSurface {
    int shown{0};
    int activated{0};
    int closed{0};
    void show() override { ++shown; }
    void activate() override { ++activated; }
    void close() override { ++closed; }
};

std::vector<StartupStep> prefix(std::size_t count) {
    const auto order = startup_order();
    return {order.begin(), order.begin() + static_cast<std::ptrdiff_t>(count)};
}

std::vector<StartupStep> reversed_prefix(std::size_t count) {
    auto result = prefix(count);
    std::reverse(result.begin(), result.end());
    return result;
}

}  // namespace

int main() {
    namespace test = squiflow::testing;

    test::section("fixed startup and reverse shutdown");
    Runtime normal;
    StartupSequence sequence(normal);
    const auto result = sequence.start();
    test::check(result.disposition == StartupDisposition::Running,
                "startup reaches running");
    test::check(normal.started == prefix(kStartupStepCount),
                "all steps run in the single declared order");
    test::check(sequence.completed_steps() == prefix(kStartupStepCount),
                "completed steps retain declared order while running");
    sequence.shutdown(ShutdownReason::NormalExit);
    test::check(normal.stopped == reversed_prefix(kStartupStepCount),
                "normal shutdown is exact reverse order");
    test::check(std::all_of(normal.stop_reasons.begin(), normal.stop_reasons.end(),
                            [](ShutdownReason reason) {
                                return reason == ShutdownReason::NormalExit;
                            }),
                "normal shutdown reason reaches every step");
    test::check(sequence.state() == LifecycleState::Stopped,
                "normal shutdown reaches stopped");
    sequence.shutdown(ShutdownReason::FatalApplicationError);
    test::check(normal.stopped.size() == kStartupStepCount,
                "second shutdown is a no-op");

    test::section("failure matrix");
    const auto order = startup_order();
    for (std::size_t failure_index = 0; failure_index < order.size();
         ++failure_index) {
        Runtime runtime;
        runtime.fail_at = static_cast<int>(failure_index);
        StartupSequence failing(runtime);
        const auto failure = failing.start();
        test::check(failure.disposition == StartupDisposition::Failed,
                    "injected startup step fails");
        test::check(failure.failure && failure.failure->step == order[failure_index],
                    "failure identifies the exact step");
        test::check(runtime.started == prefix(failure_index + 1),
                    "nothing starts after the failed step");
        test::check(runtime.stopped == reversed_prefix(failure_index),
                    "only completed steps unwind in reverse order");
        test::check(std::all_of(runtime.stop_reasons.begin(),
                                runtime.stop_reasons.end(),
                                [](ShutdownReason reason) {
                                    return reason == ShutdownReason::StartupFailure;
                                }),
                    "startup failure reason reaches every rollback");
        test::check(failing.completed_steps().empty(),
                    "failed startup retains no completed resources");
        test::check(failing.state() == LifecycleState::Failed,
                    "failed startup reaches failed state");
    }

    test::section("rollback failures do not stop rollback");
    Runtime rollback;
    rollback.fail_at = 5;
    rollback.throw_stop = order[2];
    StartupSequence rollback_sequence(rollback);
    const auto rollback_result = rollback_sequence.start();
    test::check(rollback_result.disposition == StartupDisposition::Failed,
                "startup still reports its original failure");
    test::check(rollback.stopped == reversed_prefix(5),
                "rollback continues past a failing stop callback");
    const auto rollback_failures = rollback_sequence.rollback_failures();
    test::check(rollback_failures.size() == 1 &&
                    rollback_failures.front().step == order[2],
                "rollback failure is retained with its step");
    test::check(rollback.diagnostics.size() == 1 &&
                    rollback.diagnostics.front().step == order[2],
                "rollback diagnostic is emitted exactly once");

    test::section("secondary instance stops before database");
    Runtime secondary;
    secondary.secondary_at = 3;
    StartupSequence secondary_sequence(secondary);
    const auto secondary_result = secondary_sequence.start();
    test::check(secondary_result.disposition == StartupDisposition::SecondaryInstance,
                "secondary disposition is preserved");
    test::check(secondary.started == prefix(4),
                "secondary stops after the instance step");
    test::check(secondary.stopped == reversed_prefix(3),
                "pre-instance resources are released");
    test::check(secondary_sequence.state() == LifecycleState::SecondaryInstance,
                "secondary lifecycle state is terminal");

    test::section("every shutdown reason uses one reverse path");
    const std::vector<ShutdownReason> reasons = {
        ShutdownReason::NormalExit,
        ShutdownReason::WindowClosed,
        ShutdownReason::SessionEnding,
        ShutdownReason::SystemShutdown,
        ShutdownReason::StartupFailure,
        ShutdownReason::FatalApplicationError,
    };
    for (const ShutdownReason reason : reasons) {
        Runtime runtime;
        StartupSequence running(runtime);
        test::check(running.start().disposition == StartupDisposition::Running,
                    "shutdown fixture reaches running");
        running.shutdown(reason);
        test::check(runtime.stopped == reversed_prefix(kStartupStepCount),
                    "shutdown uses exact reverse order");
        test::check(std::all_of(runtime.stop_reasons.begin(),
                                runtime.stop_reasons.end(),
                                [reason](ShutdownReason actual) {
                                    return actual == reason;
                                }),
                    "shutdown reason is preserved for every step");
    }

    test::section("cross-thread shutdown during startup");
    Runtime concurrent;
    concurrent.block_at = 4;
    StartupSequence concurrent_sequence(concurrent);
    std::optional<StartupResult> concurrent_result;
    std::thread starter([&] { concurrent_result = concurrent_sequence.start(); });
    {
        std::unique_lock lock(concurrent.gate_mutex);
        concurrent.gate_changed.wait(lock,
                                     [&] { return concurrent.callback_entered; });
    }
    concurrent_sequence.shutdown(ShutdownReason::SystemShutdown);
    {
        std::lock_guard lock(concurrent.gate_mutex);
        concurrent.release_callback = true;
    }
    concurrent.gate_changed.notify_all();
    starter.join();
    test::check(concurrent_result &&
                    concurrent_result->disposition == StartupDisposition::Failed,
                "shutdown request interrupts startup safely");
    test::check(concurrent.started == prefix(5),
                "no later startup callback runs after shutdown request");
    test::check(concurrent.stopped == reversed_prefix(5),
                "cross-thread shutdown unwinds completed callbacks");
    test::check(std::all_of(concurrent.stop_reasons.begin(),
                            concurrent.stop_reasons.end(),
                            [](ShutdownReason reason) {
                                return reason == ShutdownReason::SystemShutdown;
                            }),
                "cross-thread shutdown reason is preserved");
    test::check(concurrent_sequence.state() == LifecycleState::Stopped,
                "interrupted startup reaches stopped");

    test::section("reentrant shutdown");
    Runtime reentrant;
    StartupSequence reentrant_sequence(reentrant);
    reentrant.sequence = &reentrant_sequence;
    const auto reentrant_result = reentrant_sequence.start();
    test::check(reentrant_result.disposition == StartupDisposition::Failed,
                "reentrant shutdown interrupts startup");
    test::check(reentrant.started == prefix(5),
                "reentrant shutdown is observed after the callback");
    test::check(reentrant.stopped == reversed_prefix(5),
                "reentrant shutdown unwinds completed callbacks");

    test::section("activation coalescing");
    ActivationController activation;
    Surface surface;
    activation.request();
    activation.request();
    test::check(activation.pending(), "pre-surface activation is pending");
    activation.attach(surface);
    test::check(surface.activated == 1, "pending burst is coalesced");
    activation.request();
    test::check(surface.activated == 2, "live activation is delivered");
    activation.stop();
    activation.request();
    test::check(surface.activated == 2, "stopped activation is ignored");

    test::section("Qt mapping and recursion guard");
    test::check(map_qt_message(QtMessageKind::Fatal) ==
                    squiflow::platform::LogLevel::Fatal,
                "fatal Qt message maps to fatal logging");
    QtMessageRecursionGuard outer;
    QtMessageRecursionGuard inner;
    test::check(outer.entered() && !inner.entered(),
                "recursive Qt logging bridge is suppressed");

    test::section("composition manifest");
    test::check(kCompositionModules.size() == 12, "twelve modules are declared");
    auto names = kCompositionModules;
    std::sort(names.begin(), names.end());
    test::check(std::adjacent_find(names.begin(), names.end()) == names.end(),
                "module names are unique");

    return test::report();
}
