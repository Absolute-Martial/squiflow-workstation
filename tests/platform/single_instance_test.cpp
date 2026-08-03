#include <csignal>
#include <filesystem>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

#include "platform/single_instance.hpp"
#include "platform/single_instance_name.hpp"
#include "platform/testing/fake_single_instance_lock.hpp"
#include "support/check.hpp"

namespace p = squiflow::platform;
using squiflow::testing::check;
using squiflow::testing::section;

int main() {
    section("stable and private instance identity");
    const auto a = p::make_single_instance_names("SquiFlow.Workstation", "/tmp/shop/../shop/data");
    const auto b = p::make_single_instance_names("SquiFlow.Workstation", "/tmp/shop/data");
    const auto c = p::make_single_instance_names("SquiFlow.Workstation", "/tmp/other/data");
    check(a.ok && b.ok && c.ok, "valid identities are accepted");
    check(a.mutex_name == b.mutex_name, "lexically equal paths share one identity");
    check(a.mutex_name != c.mutex_name, "different data roots do not collide");
    check(a.mutex_name.find("/tmp/shop") == std::string::npos, "raw data path is not disclosed");
    check(a.mutex_name != a.activation_name, "mutex and activation objects are distinct");
    check(!p::make_single_instance_names("", "/tmp/data").ok, "empty application id refused");
    check(!p::make_single_instance_names("bad\\name", "/tmp/data").ok, "namespace injection refused");
    check(!p::make_single_instance_names("SquiFlow", "relative").ok, "relative data root refused");

    section("deterministic fake lifecycle");
    p::testing::FakeSingleInstanceLock fake;
    check(fake.acquire("ignored", "ignored").state == p::InstanceState::Primary,
          "fake becomes primary");
    fake.request_activation();
    check(fake.take_activation_request(), "fake returns activation once");
    check(!fake.take_activation_request(), "fake activation is consumed");
    fake.release();
    check(fake.state() == p::InstanceState::Idle, "fake release restores idle");
    fake.next = {p::InstanceState::Failed, p::InstanceFault::PermissionDenied,
                 "denied", false};
    check(fake.acquire("ignored", "ignored").fault == p::InstanceFault::PermissionDenied,
          "fake injects acquisition failure");

    section("real exclusion and activation");
    char pattern[] = "/tmp/squiflow-instance-XXXXXX";
    const char* made = ::mkdtemp(pattern);
    check(made != nullptr, "temporary data directory created");
    const std::string directory = made == nullptr ? "/tmp" : made;
    auto first = p::make_single_instance_lock();
    auto second = p::make_single_instance_lock();
    check(first->acquire("SquiFlow.Workstation", directory).state == p::InstanceState::Primary,
          "first owner becomes primary");
    check(first->state() == p::InstanceState::Primary, "primary state retained");
    check(second->acquire("SquiFlow.Workstation", directory).state == p::InstanceState::Secondary,
          "second owner is excluded");
    check(first->take_activation_request(), "secondary launch signals primary");
    check(!first->take_activation_request(), "activation signal is one-shot");
    check(!second->take_activation_request(), "secondary cannot consume activation");
    second->release();
    first->release();
    check(first->state() == p::InstanceState::Idle, "release restores idle");
    check(first->acquire("SquiFlow.Workstation", directory).state == p::InstanceState::Primary,
          "released lock can be reacquired");
    check(first->acquire("SquiFlow.Workstation", directory).state == p::InstanceState::Failed,
          "duplicate acquire is refused");
    first->release();

    section("process death releases kernel ownership");
    int ready[2]{};
    check(::pipe(ready) == 0, "coordination pipe created");
    const pid_t child = ::fork();
    check(child >= 0, "child process created");
    if (child == 0) {
        ::close(ready[0]);
        auto owner = p::make_single_instance_lock();
        const bool primary = owner->acquire("SquiFlow.Workstation", directory).state
                             == p::InstanceState::Primary;
        const char byte = primary ? '1' : '0';
        (void)::write(ready[1], &byte, 1);
        for (;;) { ::pause(); }
    }
    ::close(ready[1]);
    char byte{};
    check(::read(ready[0], &byte, 1) == 1 && byte == '1', "child owns lock before termination");
    ::close(ready[0]);
    check(::kill(child, SIGKILL) == 0, "child terminated abruptly");
    int status{};
    check(::waitpid(child, &status, 0) == child, "terminated child reaped");
    auto recovered = p::make_single_instance_lock();
    check(recovered->acquire("SquiFlow.Workstation", directory).state == p::InstanceState::Primary,
          "kernel releases ownership after process death");
    recovered->release();

    section("unsafe paths fail closed");
    std::filesystem::remove(directory + "/.squiflow.instance.lock");
    std::filesystem::create_symlink("/tmp", directory + "/.squiflow.instance.lock");
    auto unsafe = p::make_single_instance_lock();
    check(unsafe->acquire("SquiFlow.Workstation", directory).state == p::InstanceState::Failed,
          "symbolic-link lock file is refused");
    check(unsafe->state() == p::InstanceState::Failed, "failure is explicit");
    unsafe->release();
    check(unsafe->state() == p::InstanceState::Idle, "failed object can be reset");
    std::filesystem::remove_all(directory);

    return squiflow::testing::report();
}
