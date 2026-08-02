#include "platform/path_environment.hpp"

#include <cstdlib>
#include <string>

// Discovery for the verification lane.
//
// The shipping application runs on Windows and uses the Qt implementation of
// this same function. This file exists so that the machine which compiles and
// runs the strict suite, which has neither Qt nor Windows, can still start the
// real resolver against real directories. The two implementations answer the
// same question with the same rules: a machine-wide location for records, a
// per-account location for cache, and an explicit refusal rather than a quiet
// substitution when the machine-wide one cannot be determined.

namespace squiflow::platform {
namespace {

// The filesystem hierarchy standard location for machine-wide variable state.
// This is a platform constant, not a machine-specific path: it contains no
// user name, no drive, and no install location.
constexpr char kSharedStateDirectory[] = "/var/lib";
constexpr char kCacheHomeVariable[] = "XDG_CACHE_HOME";
constexpr char kHomeVariable[] = "HOME";

std::string from_environment(const char* name) {
    const char* const value = std::getenv(name);
    if (value == nullptr) {
        return std::string();
    }
    return std::string(value);
}

}  // namespace

EnvironmentDiscovery discover_path_environment(const ApplicationIdentity& identity) {
    EnvironmentDiscovery discovery;
    discovery.environment.identity = identity;

    std::string shared = from_environment(kSharedDataRootVariable);
    const bool overridden = !shared.empty();
    if (shared.empty()) {
        shared = kSharedStateDirectory;
    }

    std::string cache = from_environment(kUserCacheRootVariable);
    if (cache.empty()) {
        cache = from_environment(kCacheHomeVariable);
    }
    if (cache.empty()) {
        const std::string home = from_environment(kHomeVariable);
        if (!home.empty()) {
            cache = home + "/.cache";
        }
    }

    discovery.environment.shared_data_root = shared;

    // An empty cache root is not an error. The resolver treats an unusable
    // per-account cache as a warning and places one inside the data directory,
    // because a cache is disposable by definition.
    discovery.environment.user_cache_root = cache;
    discovery.ok = true;
    discovery.message = overridden
                            ? "shared data root taken from the environment "
                              "override"
                            : "shared data root taken from the machine-wide "
                              "state directory";
    return discovery;
}

}  // namespace squiflow::platform
