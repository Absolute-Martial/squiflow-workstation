#pragma once

// The two roots the rest of path resolution is built from, and who names them.
//
// Discovery is deliberately separated from resolution. Asking the operating
// system where the machine-wide application data lives is platform work and
// cannot be executed on the verification machine; deciding what to do with the
// answer is ordinary logic and is tested exhaustively. Only this file changes
// when a new platform is added.
//
// See docs/adr/0004-machine-wide-data-root.md.

#include <string>

namespace squiflow::platform {

// Names the vendor and product folders. Held as data rather than baked into a
// literal so tests can drive the awkward cases and so a rebrand is one call
// site.
struct ApplicationIdentity {
    std::string organization;
    std::string application;
};

// Shared data is machine-wide and holds every record. Cache is per account and
// is disposable: losing all of it may cost time, never a record.
struct PathEnvironment {
    std::string shared_data_root;
    std::string user_cache_root;
    ApplicationIdentity identity;
};

struct EnvironmentDiscovery {
    bool ok{false};
    PathEnvironment environment{};
    std::string message{};
};

// Support and testing override, read from the process environment. Documented
// rather than hidden, because an undocumented override is a support incident
// waiting to happen.
inline constexpr char kSharedDataRootVariable[] = "SQUIFLOW_DATA_ROOT";
inline constexpr char kUserCacheRootVariable[] = "SQUIFLOW_CACHE_ROOT";

// Implemented once per lane: path_environment_qt.cpp for the shipping Windows
// build, path_environment_posix.cpp for the verification machine. Never
// guesses: if the machine-wide location cannot be determined it refuses, and
// it never quietly substitutes a per-account folder.
EnvironmentDiscovery discover_path_environment(const ApplicationIdentity& identity);

}  // namespace squiflow::platform
