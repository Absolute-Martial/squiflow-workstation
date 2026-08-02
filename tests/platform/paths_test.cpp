// Phase 6.1: where the shop's files live.
//
// The interesting cases are the ones a developer machine will not produce on
// request: a program-data folder that exists but refuses writes, a file that
// occupies the name of a required directory, a parent that will not answer,
// and a per-account cache that has been deleted while the application was
// running. All of them are arranged against the fake in microseconds. The
// standard-library probe is then exercised against a real temporary directory,
// because a fake that disagrees with reality proves nothing.

#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

#include "platform/local_directory_probe.hpp"
#include "platform/path_environment.hpp"
#include "platform/paths.hpp"
#include "platform/testing/fake_directory_probe.hpp"
#include "support/check.hpp"

namespace {

namespace platform = squiflow::platform;
using platform::DirectoryState;
using platform::PathFault;
using platform::PathResolution;
using platform::PathResolver;
using platform::PathRole;
using platform::testing::FakeDirectoryProbe;
using squiflow::testing::check;
using squiflow::testing::section;

platform::PathEnvironment windows_like_environment() {
    platform::PathEnvironment environment;
    environment.shared_data_root = "C:/ProgramData";
    environment.user_cache_root = "C:/Users/counter/AppData/Local/Temp";
    environment.identity.organization = "SquiFlow";
    environment.identity.application = "Workstation";
    return environment;
}

std::string repeated(char value, std::size_t count) {
    return std::string(count, value);
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

bool any_warning_mentions(const PathResolution& resolution,
                          const std::string& needle) {
    for (const std::string& warning : resolution.warnings) {
        if (contains(warning, needle)) {
            return true;
        }
    }
    return false;
}

// ------------------------------------------------------------- normalisation

void normalisation() {
    section("path normalisation");

    check(platform::normalize_path("C:\\ProgramData\\SquiFlow")
              == "C:/ProgramData/SquiFlow",
          "backslashes become separators");
    check(platform::normalize_path("C:/ProgramData//SquiFlow///x")
              == "C:/ProgramData/SquiFlow/x",
          "repeated separators collapse");
    check(platform::normalize_path("C:/ProgramData/") == "C:/ProgramData",
          "a trailing separator is removed");
    check(platform::normalize_path("C:/ProgramData\\\\") == "C:/ProgramData",
          "trailing mixed separators are removed");
    check(platform::normalize_path("/") == "/", "the posix root survives");
    check(platform::normalize_path("") == "", "an empty path stays empty");
    check(platform::normalize_path("\\\\server\\share\\data")
              == "//server/share/data",
          "a network share keeps its leading double separator");
    check(platform::normalize_path("//server//share//") == "//server/share",
          "a network share is collapsed but not flattened");
    check(platform::normalize_path("relative/dir") == "relative/dir",
          "a relative path is normalised, not rescued");
}

void absoluteness() {
    section("absolute path detection");

    check(platform::is_absolute_path("/var/lib"), "a posix root is absolute");
    check(platform::is_absolute_path("C:/ProgramData"),
          "a drive specification is absolute");
    check(platform::is_absolute_path("c:/programdata"),
          "a lower case drive letter is absolute");
    check(!platform::is_absolute_path("C:"),
          "a bare drive without a separator is not absolute");
    check(!platform::is_absolute_path("C:ProgramData"),
          "a drive relative path is not absolute");
    check(!platform::is_absolute_path("data"), "a bare name is not absolute");
    check(!platform::is_absolute_path(""), "an empty path is not absolute");
    check(!platform::is_absolute_path("//server"),
          "a server without a share is not a usable root");
    check(!platform::is_absolute_path("//server/"),
          "a server with an empty share is not a usable root");
    check(platform::is_absolute_path("//server/share"),
          "a server and share together are absolute");
    check(!platform::is_absolute_path("1:/data"),
          "a digit is not a drive letter");

    check(platform::contains_traversal("C:/data/../other"),
          "a parent segment is traversal");
    check(platform::contains_traversal("C:/data/./other"),
          "a current segment is traversal");
    check(!platform::contains_traversal("C:/data/..hidden"),
          "a name that merely starts with dots is not traversal");
    check(!platform::contains_traversal("C:/ProgramData/SquiFlow"),
          "an ordinary path has no traversal");
}

void identity_names() {
    section("identity names");

    check(platform::is_acceptable_identity_name("SquiFlow"),
          "an ordinary name is acceptable");
    check(platform::is_acceptable_identity_name("Squi Flow-2_x"),
          "spaces, hyphens and underscores are acceptable");
    check(!platform::is_acceptable_identity_name(""),
          "an empty name is refused");
    check(!platform::is_acceptable_identity_name(" SquiFlow"),
          "a leading space is refused");
    check(!platform::is_acceptable_identity_name("SquiFlow "),
          "a trailing space is refused, because Windows silently strips it");
    check(!platform::is_acceptable_identity_name("SquiFlow."),
          "a trailing dot is refused for the same reason");
    check(!platform::is_acceptable_identity_name("Squi/Flow"),
          "a separator inside a name is refused");
    check(!platform::is_acceptable_identity_name("Squi\\Flow"),
          "a backslash inside a name is refused");
    check(!platform::is_acceptable_identity_name("Squi:Flow"),
          "a colon inside a name is refused");
    check(!platform::is_acceptable_identity_name("Squi*Flow"),
          "a wildcard inside a name is refused");
    check(!platform::is_acceptable_identity_name(std::string("Squi\x01" "Flow")),
          "a control character inside a name is refused");
}

// ------------------------------------------------------------- happy resolve

void resolves_a_shared_layout() {
    section("a resolved layout");

    FakeDirectoryProbe probe;
    probe.add_directory("C:/ProgramData");
    probe.add_directory("C:/Users/counter/AppData/Local/Temp");

    const PathResolver resolver(probe);
    const PathResolution resolution = resolver.resolve(windows_like_environment());

    check(resolution.ok, "an ordinary machine resolves");
    check(resolution.fault == PathFault::None, "with no fault");
    check(resolution.warnings.empty(), "and with no warning");
    check(resolution.layout.is_resolved(), "the layout reports itself resolved");

    const std::string data = resolution.layout.directory(PathRole::Data);
    check(data == "C:/ProgramData/SquiFlow/Workstation",
          "records live under the machine-wide root");
    check(resolution.layout.directory(PathRole::Logs) == data + "/logs",
          "logs live beside the records");
    check(resolution.layout.directory(PathRole::Backups) == data + "/backups",
          "backups live beside the records");
    check(resolution.layout.directory(PathRole::Crash) == data + "/crash",
          "crash dumps live beside the records");
    check(resolution.layout.directory(PathRole::Secrets) == data + "/secrets",
          "the secrets store lives beside the records");
    check(resolution.layout.directory(PathRole::Cache)
              == "C:/Users/counter/AppData/Local/Temp/SquiFlow/Workstation/cache",
          "the cache lives under the per-account root");
    check(resolution.layout.database_file()
              == data + "/squiflow.db",
          "the database file sits directly in the data directory");

    check(!contains(data, "Users"),
          "no part of the data location is per-account");
    check(probe.has_directory(data + "/secrets"),
          "every required directory was created");
    check(probe.write_checks() >= 6,
          "every directory was proven writable, not merely created");
    check(contains(resolution.layout.describe(), "database: "),
          "the description names the database file");
}

void a_second_account_sees_the_same_records() {
    section("a second Windows account");

    FakeDirectoryProbe probe;
    probe.add_directory("C:/ProgramData");
    probe.add_directory("C:/Users/counter/AppData/Local/Temp");
    probe.add_directory("C:/Users/evening/AppData/Local/Temp");

    const PathResolver resolver(probe);

    platform::PathEnvironment morning = windows_like_environment();
    platform::PathEnvironment evening = windows_like_environment();
    evening.user_cache_root = "C:/Users/evening/AppData/Local/Temp";

    const PathResolution first = resolver.resolve(morning);
    const PathResolution second = resolver.resolve(evening);

    check(first.ok && second.ok, "both accounts resolve");
    check(first.layout.directory(PathRole::Data)
              == second.layout.directory(PathRole::Data),
          "both accounts see the same records");
    check(first.layout.database_file() == second.layout.database_file(),
          "and therefore the same database");
    check(first.layout.directory(PathRole::Cache)
              != second.layout.directory(PathRole::Cache),
          "but each keeps its own cache");
}

void resolution_is_idempotent() {
    section("resolving twice");

    FakeDirectoryProbe probe;
    probe.add_directory("C:/ProgramData");
    probe.add_directory("C:/Users/counter/AppData/Local/Temp");

    const PathResolver resolver(probe);
    const PathResolution first = resolver.resolve(windows_like_environment());
    const std::size_t created_after_first = probe.created().size();
    const PathResolution second = resolver.resolve(windows_like_environment());

    check(second.ok, "the second resolution succeeds");
    check(first.layout.describe() == second.layout.describe(),
          "and produces an identical layout");
    check(probe.created().size() == created_after_first,
          "nothing is created a second time");
}

// ----------------------------------------------------------- failure paths

void refuses_bad_identity() {
    section("unusable identity names");

    FakeDirectoryProbe probe;
    probe.add_directory("C:/ProgramData");
    const PathResolver resolver(probe);

    platform::PathEnvironment environment = windows_like_environment();
    environment.identity.organization.clear();
    check(resolver.resolve(environment).fault == PathFault::IdentityEmpty,
          "an empty organization is refused");

    environment = windows_like_environment();
    environment.identity.application = repeated('a', 49);
    check(resolver.resolve(environment).fault == PathFault::IdentityTooLong,
          "a name one character over the limit is refused");

    environment.identity.application = repeated('a', 48);
    check(resolver.resolve(environment).ok,
          "a name exactly at the limit is accepted");

    environment = windows_like_environment();
    environment.identity.application = "nul";
    const PathResolution reserved = resolver.resolve(environment);
    check(reserved.fault == PathFault::IdentityReserved,
          "a reserved device name is refused whatever its case");
    check(contains(reserved.message, "reserved"),
          "and the message says why");

    environment.identity.application = "COM9";
    check(resolver.resolve(environment).fault == PathFault::IdentityReserved,
          "every reserved device name is refused, not only the famous ones");

    environment.identity.application = "COM10";
    check(resolver.resolve(environment).ok,
          "a name that merely looks like a device is allowed");

    environment.identity.application = "Work<station";
    check(resolver.resolve(environment).fault == PathFault::IdentityCharacter,
          "a character Windows refuses is refused here first");
}

void refuses_bad_roots() {
    section("unusable roots");

    FakeDirectoryProbe probe;
    probe.add_directory("C:/ProgramData");
    const PathResolver resolver(probe);

    platform::PathEnvironment environment = windows_like_environment();
    environment.shared_data_root.clear();
    check(resolver.resolve(environment).fault == PathFault::RootEmpty,
          "an empty shared root is refused");

    environment = windows_like_environment();
    environment.shared_data_root = "ProgramData";
    const PathResolution relative = resolver.resolve(environment);
    check(relative.fault == PathFault::RootNotAbsolute,
          "a relative shared root is refused");
    check(contains(relative.message, "working directory"),
          "and the message explains the danger");

    environment.shared_data_root = "C:/ProgramData/../../Windows";
    check(resolver.resolve(environment).fault == PathFault::RootTraversal,
          "a traversal in the shared root is refused");

    environment.shared_data_root = "C:/Program\x01" "Data";
    check(resolver.resolve(environment).fault == PathFault::RootCharacter,
          "a control character in the shared root is refused");

    environment.shared_data_root = "C:/Program:Data";
    check(resolver.resolve(environment).fault == PathFault::RootCharacter,
          "a stream separator in the shared root is refused");

    environment.shared_data_root = "C:/" + repeated('d', 118);
    check(resolver.resolve(environment).fault == PathFault::RootTooLong,
          "a shared root one character over the limit is refused");
}

void refuses_an_unusable_machine() {
    section("a machine that cannot host the application");

    const platform::PathEnvironment environment = windows_like_environment();
    const std::string data = "C:/ProgramData/SquiFlow/Workstation";

    {
        FakeDirectoryProbe probe;
        probe.add_directory("C:/ProgramData");
        probe.add_file(data);
        const PathResolver resolver(probe);
        const PathResolution resolution = resolver.resolve(environment);
        check(!resolution.ok, "a file where the data directory belongs stops startup");
        check(resolution.fault == PathFault::OccupiedByFile, "with the right fault");
        check(resolution.offending_path == data, "naming the offending path");
        check(contains(resolution.message, "data directory"),
              "and naming the role that failed");
        check(!resolution.layout.is_resolved(),
              "and the layout is left unresolved");
    }

    {
        FakeDirectoryProbe probe;
        probe.add_directory("C:/ProgramData");
        probe.make_undeterminable(data);
        const PathResolver resolver(probe);
        const PathResolution resolution = resolver.resolve(environment);
        check(resolution.fault == PathFault::Undeterminable,
              "a location that cannot be examined is not assumed to be free");
    }

    {
        FakeDirectoryProbe probe;
        probe.add_directory("C:/ProgramData");
        probe.deny_creation_under(data);
        const PathResolver resolver(probe);
        const PathResolution resolution = resolver.resolve(environment);
        check(resolution.fault == PathFault::CreateFailed,
              "a creation that fails stops startup");
    }

    {
        FakeDirectoryProbe probe;
        probe.add_directory("C:/ProgramData");
        probe.add_directory(data);
        probe.deny_writes_under(data);
        const PathResolver resolver(probe);
        const PathResolution resolution = resolver.resolve(environment);
        check(resolution.fault == PathFault::NotWritable,
              "a data directory that exists but refuses writes stops startup");
        check(contains(resolution.message, "cannot write"),
              "and says so in words a support call can use");
    }

    {
        FakeDirectoryProbe probe;
        probe.add_directory("C:/ProgramData");
        probe.deny_writes_under(data + "/secrets");
        const PathResolver resolver(probe);
        const PathResolution resolution = resolver.resolve(environment);
        check(resolution.fault == PathFault::NotWritable,
              "one unwritable subdirectory is enough to stop startup");
        check(contains(resolution.message, "secrets"),
              "and the failing role is named");
    }

    {
        FakeDirectoryProbe probe;
        probe.add_directory("C:/" + repeated('d', 100));
        platform::PathEnvironment deep = environment;
        deep.shared_data_root = "C:/" + repeated('d', 100);
        deep.identity.organization = repeated('o', 45);
        deep.identity.application = repeated('a', 45);
        const PathResolver resolver(probe);
        const PathResolution resolution = resolver.resolve(deep);
        check(resolution.fault == PathFault::DirectoryTooLong,
              "a composed directory too long for Windows is refused");
        check(contains(resolution.message, "file names"),
              "and the reason mentions the room left for file names");
    }
}

// ------------------------------------------------------------- cache policy

void a_disposable_cache_never_stops_the_shop() {
    section("the cache is allowed to disappoint");

    const std::string data = "C:/ProgramData/SquiFlow/Workstation";

    {
        FakeDirectoryProbe probe;
        probe.add_directory("C:/ProgramData");
        platform::PathEnvironment environment = windows_like_environment();
        environment.user_cache_root.clear();
        const PathResolver resolver(probe);
        const PathResolution resolution = resolver.resolve(environment);
        check(resolution.ok, "no cache root at all still starts the shop");
        check(resolution.layout.directory(PathRole::Cache) == data + "/cache",
              "and the cache moves inside the data directory");
        check(any_warning_mentions(resolution, "cache"),
              "the substitution is reported, not hidden");
    }

    {
        FakeDirectoryProbe probe;
        probe.add_directory("C:/ProgramData");
        probe.deny_creation_under("C:/Users");
        const PathResolver resolver(probe);
        const PathResolution resolution = resolver.resolve(windows_like_environment());
        check(resolution.ok, "a cache that cannot be created still starts the shop");
        check(resolution.layout.directory(PathRole::Cache) == data + "/cache",
              "and falls back inside the data directory");
        check(resolution.warnings.size() == 1, "with exactly one warning");
    }

    {
        FakeDirectoryProbe probe;
        probe.add_directory("C:/ProgramData");
        platform::PathEnvironment environment = windows_like_environment();
        environment.user_cache_root = "Local/Temp";
        const PathResolver resolver(probe);
        const PathResolution resolution = resolver.resolve(environment);
        check(resolution.ok, "a relative cache root does not stop the shop");
        check(any_warning_mentions(resolution, "absolute"),
              "and the warning explains what was wrong with it");
    }

    {
        FakeDirectoryProbe probe;
        probe.add_directory("C:/ProgramData");
        probe.deny_creation_under("C:/Users");
        probe.deny_creation_under(data + "/cache");
        const PathResolver resolver(probe);
        const PathResolution resolution = resolver.resolve(windows_like_environment());
        check(!resolution.ok,
              "when even the fallback cache cannot be made, startup refuses");
        check(resolution.fault == PathFault::CreateFailed, "with the real fault");
        check(contains(resolution.message, "cache"), "naming the cache");
    }

    {
        FakeDirectoryProbe probe;
        probe.add_directory("C:/ProgramData");
        platform::PathEnvironment environment = windows_like_environment();
        environment.user_cache_root = "C:/ProgramData";
        const PathResolver resolver(probe);
        const PathResolution resolution = resolver.resolve(environment);
        check(resolution.ok, "a cache root equal to the data root is allowed");
        check(resolution.layout.directory(PathRole::Cache)
                  != resolution.layout.directory(PathRole::Data),
              "but the cache is never the data directory itself");
    }
}

void an_unresolved_layout_refuses_to_answer() {
    section("an unresolved layout");

    const platform::PathLayout layout;
    check(!layout.is_resolved(), "a default layout is not resolved");
    check(layout.describe() == "paths: unresolved",
          "and says so when described");

    bool threw = false;
    try {
        (void)layout.directory(PathRole::Data);
    } catch (const std::logic_error&) {
        threw = true;
    } catch (const std::exception&) {
        threw = false;
    }
    check(threw,
          "asking an unresolved layout for a directory is a programming error");
}

// -------------------------------------------------------- the real probe

std::filesystem::path make_scratch_directory() {
    const std::filesystem::path base =
        std::filesystem::temp_directory_path()
        / ("squiflow-paths-test-"
           + std::to_string(static_cast<unsigned long long>(
               std::chrono::steady_clock::now().time_since_epoch().count())));
    std::error_code error;
    std::filesystem::create_directories(base, error);
    return base;
}

void the_real_probe_agrees_with_the_fake() {
    section("the standard-library probe");

    platform::LocalDirectoryProbe probe;
    const std::filesystem::path scratch = make_scratch_directory();
    const std::string root = scratch.generic_string();

    check(probe.inspect(root) == DirectoryState::Directory,
          "a directory is reported as a directory");
    check(probe.inspect(root + "/absent") == DirectoryState::Missing,
          "an absent name is reported as missing");
    check(probe.inspect("") == DirectoryState::Unknown,
          "an empty path is not a question that can be answered");
    check(!probe.create_directory_tree(""),
          "and cannot be created");

    check(probe.create_directory_tree(root + "/one/two/three"),
          "a missing tree is created in full");
    check(probe.inspect(root + "/one/two") == DirectoryState::Directory,
          "including the intermediate directories");
    check(probe.create_directory_tree(root + "/one/two/three"),
          "creating an existing tree is not a failure");
    check(probe.can_write(root + "/one/two/three"),
          "a fresh directory is writable");
    check(!probe.can_write(root + "/absent"),
          "a missing directory is not writable");

    {
        const std::filesystem::path occupied = scratch / "occupied";
        std::ofstream handle(occupied);
        handle << "x";
    }
    check(probe.inspect(root + "/occupied") == DirectoryState::Blocked,
          "a file is not mistaken for a directory");
    check(!probe.create_directory_tree(root + "/occupied"),
          "and a directory cannot be created over it");
    check(!probe.can_write(root + "/occupied"),
          "and it is not writable as a directory");

    // The probe must leave nothing behind: a shop machine should not collect a
    // file per startup.
    std::size_t leftovers = 0;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(scratch / "one" / "two" / "three")) {
        (void)entry;
        ++leftovers;
    }
    check(leftovers == 0, "the write check leaves nothing behind");

    platform::PathEnvironment environment;
    environment.shared_data_root = root + "/shared";
    environment.user_cache_root = root + "/user";
    environment.identity.organization = "SquiFlow";
    environment.identity.application = "Workstation";

    const PathResolver resolver(probe);
    const PathResolution resolution = resolver.resolve(environment);
    check(resolution.ok, "a real directory tree resolves");
    check(resolution.warnings.empty(), "with no warning");
    for (std::size_t index = 0; index < platform::kPathRoleCount; ++index) {
        const platform::PathRole role = static_cast<platform::PathRole>(index);
        check(std::filesystem::is_directory(resolution.layout.directory(role)),
              std::string("the ") + platform::role_name(role)
                  + " directory exists on disk");
    }
    check(std::filesystem::path(resolution.layout.database_file())
              .parent_path()
              .generic_string()
              == resolution.layout.directory(PathRole::Data),
          "the database file belongs to the data directory");

    std::error_code error;
    std::filesystem::remove_all(scratch, error);
    check(!error, "the scratch directory is removed again");
}

void discovery_reports_a_usable_environment() {
    section("platform discovery");

    platform::ApplicationIdentity identity;
    identity.organization = "SquiFlow";
    identity.application = "Workstation";

    const platform::EnvironmentDiscovery discovery =
        platform::discover_path_environment(identity);

    check(discovery.ok, "discovery succeeds on this machine");
    check(!discovery.message.empty(), "and says where the root came from");
    check(discovery.environment.identity.organization == "SquiFlow",
          "the identity is carried through unchanged");
    check(platform::is_absolute_path(
              platform::normalize_path(discovery.environment.shared_data_root)),
          "the discovered shared root is absolute");
    check(!platform::contains_traversal(
              platform::normalize_path(discovery.environment.shared_data_root)),
          "and contains no traversal");
}

}  // namespace

int main() {
    normalisation();
    absoluteness();
    identity_names();
    resolves_a_shared_layout();
    a_second_account_sees_the_same_records();
    resolution_is_idempotent();
    refuses_bad_identity();
    refuses_bad_roots();
    refuses_an_unusable_machine();
    a_disposable_cache_never_stops_the_shop();
    an_unresolved_layout_refuses_to_answer();
    the_real_probe_agrees_with_the_fake();
    discovery_reports_a_usable_environment();
    return squiflow::testing::report();
}
