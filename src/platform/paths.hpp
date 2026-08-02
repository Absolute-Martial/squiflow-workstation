#pragma once

// Where the shop's files live, decided once and proven before the application
// is allowed to start.
//
// Two rules drive everything in this file. Records go to the machine-wide
// program-data location, because a shop counter is shared and a second Windows
// account must not make the shop's history appear to vanish. Cache goes to the
// per-account location, because it is disposable by definition: deleting all
// of it may cost time, never a record.
//
// Nothing here contains a literal drive, user name, or install location. The
// two roots arrive as data from platform discovery; this file only validates
// them, composes the layout, and refuses clearly when the machine cannot host
// the application.
//
// See docs/adr/0004-machine-wide-data-root.md.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "platform/directory_probe.hpp"
#include "platform/path_environment.hpp"

namespace squiflow::platform {

// Every directory the application is allowed to write into. A caller asks for
// a role; nobody composes a path by hand, which is what keeps the layout in
// one place.
enum class PathRole : std::uint8_t {
    Data,
    Logs,
    Backups,
    Crash,
    Secrets,
    Cache,
};

inline constexpr std::size_t kPathRoleCount = 6;

const char* role_name(PathRole role);

// Why these numbers: the legacy Windows path limit is 260 characters including
// the file name and the terminator. A backup file name with a timestamp needs
// about fifty, so a directory is capped well below the limit rather than
// discovering the problem on the day a backup silently fails.
inline constexpr std::size_t kMaxIdentityNameLength = 48;
inline constexpr std::size_t kMaxRootLength = 120;
inline constexpr std::size_t kMaxDirectoryLength = 200;

inline constexpr char kDatabaseFileName[] = "squiflow.db";

enum class PathFault : std::uint8_t {
    None,
    IdentityEmpty,
    IdentityTooLong,
    IdentityCharacter,
    IdentityReserved,
    RootEmpty,
    RootNotAbsolute,
    RootTooLong,
    RootTraversal,
    RootCharacter,
    DirectoryTooLong,
    OccupiedByFile,
    Undeterminable,
    CreateFailed,
    NotWritable,
};

const char* describe_fault(PathFault fault);

// A resolved, validated, existing, writable set of directories. It can only be
// produced by PathResolver, so holding one is proof that the checks ran.
class PathLayout {
public:
    PathLayout() = default;

    bool is_resolved() const;

    // Throws std::logic_error when the layout was never resolved. That is a
    // programming error, not a runtime condition: a resolution failure is
    // reported through PathResolution and must be handled there.
    const std::string& directory(PathRole role) const;
    std::string database_file() const;

    std::string describe() const;

private:
    friend class PathResolver;

    std::array<std::string, kPathRoleCount> directories_{};
    bool resolved_{false};
};

// The result shape used across the codebase in place of std::expected, which
// the verification toolchain does not ship. See
// docs/adr/0002-error-propagation-policy.md.
struct PathResolution {
    bool ok{false};
    PathLayout layout{};
    PathFault fault{PathFault::None};
    std::string message{};
    std::string offending_path{};

    // Non-fatal findings, chiefly a per-account cache that could not be used
    // and was replaced by one inside the data directory. Surfaced rather than
    // swallowed, and expected to reach the log at startup.
    std::vector<std::string> warnings{};
};

// Validation vocabulary, exposed because it is worth testing directly and
// because callers occasionally need to check a candidate before offering it.
std::string normalize_path(const std::string& raw);
bool is_absolute_path(const std::string& normalized);
bool contains_traversal(const std::string& normalized);
bool is_acceptable_identity_name(const std::string& name);

class PathResolver {
public:
    // The probe is borrowed, never owned: it outlives the resolver, which is a
    // short-lived object created at startup and discarded once the layout is
    // in hand.
    explicit PathResolver(DirectoryProbe& probe);

    PathResolution resolve(const PathEnvironment& environment) const;

private:
    DirectoryProbe& probe_;
};

}  // namespace squiflow::platform
