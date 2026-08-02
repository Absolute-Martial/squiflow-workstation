#include "platform/paths.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace squiflow::platform {
namespace {

constexpr char kSeparator = '/';

// Names that Windows still treats as devices whatever the extension, in any
// directory. A folder called NUL cannot be created, and the failure surfaces
// as a bare access error, so the name is refused here where the message can
// say why.
const char* const kReservedDeviceNames[] = {
    "CON", "PRN", "AUX", "NUL",
    "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
    "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
};

// Characters Windows refuses inside a name. The colon is legal only as the
// second character of a drive specification, which is handled by the root
// check rather than here.
const char* const kForbiddenNameCharacters = "<>:\"|?*/\\";

bool is_control_character(char value) {
    const unsigned char raw = static_cast<unsigned char>(value);
    return raw < 0x20U || raw == 0x7FU;
}

bool is_ascii_letter(char value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

bool is_ascii_digit(char value) {
    return value >= '0' && value <= '9';
}

char to_upper(char value) {
    return (value >= 'a' && value <= 'z')
               ? static_cast<char>(value - ('a' - 'A'))
               : value;
}

std::string upper_case(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (const char value : text) {
        result.push_back(to_upper(value));
    }
    return result;
}

std::vector<std::string> segments_of(const std::string& path) {
    std::vector<std::string> parts;
    std::string current;
    for (const char value : path) {
        if (value == kSeparator) {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(value);
    }
    if (!current.empty()) {
        parts.push_back(current);
    }
    return parts;
}

std::string join(const std::string& base, const std::string& leaf) {
    if (base.empty()) {
        return leaf;
    }
    if (base.back() == kSeparator) {
        return base + leaf;
    }
    return base + kSeparator + leaf;
}

struct Failure {
    PathFault fault{PathFault::None};
    std::string message{};
};

Failure check_identity_name(const std::string& name, const char* label) {
    if (name.empty()) {
        return {PathFault::IdentityEmpty,
                std::string(label) + " name is empty"};
    }
    if (name.size() > kMaxIdentityNameLength) {
        return {PathFault::IdentityTooLong,
                std::string(label) + " name is longer than "
                    + std::to_string(kMaxIdentityNameLength) + " characters"};
    }
    if (!is_acceptable_identity_name(name)) {
        return {PathFault::IdentityCharacter,
                std::string(label)
                    + " name may contain only letters, digits, spaces, "
                      "hyphens and underscores, and may not begin or end "
                      "with a space"};
    }

    const std::string upper = upper_case(name);
    for (const char* reserved : kReservedDeviceNames) {
        if (upper == reserved) {
            return {PathFault::IdentityReserved,
                    std::string(label) + " name '" + name
                        + "' is a reserved Windows device name"};
        }
    }
    return {};
}

Failure check_root(const std::string& normalized, const char* label) {
    if (normalized.empty()) {
        return {PathFault::RootEmpty, std::string(label) + " is empty"};
    }
    if (normalized.size() > kMaxRootLength) {
        return {PathFault::RootTooLong,
                std::string(label) + " is longer than "
                    + std::to_string(kMaxRootLength) + " characters"};
    }
    if (!is_absolute_path(normalized)) {
        return {PathFault::RootNotAbsolute,
                std::string(label)
                    + " is not an absolute path; a relative location depends "
                      "on the working directory, which the application does "
                      "not control"};
    }
    if (contains_traversal(normalized)) {
        return {PathFault::RootTraversal,
                std::string(label) + " contains a '.' or '..' segment"};
    }
    for (const char value : normalized) {
        if (is_control_character(value)) {
            return {PathFault::RootCharacter,
                    std::string(label) + " contains a control character"};
        }
    }

    // The colon is legitimate only in a drive specification. Anywhere else it
    // names an alternate data stream, which is not a directory.
    const std::string::size_type first = normalized.find(':');
    if (first != std::string::npos && first != 1) {
        return {PathFault::RootCharacter,
                std::string(label) + " contains a colon outside a drive "
                                     "specification"};
    }
    if (first == 1 && normalized.find(':', 2) != std::string::npos) {
        return {PathFault::RootCharacter,
                std::string(label) + " contains more than one colon"};
    }
    return {};
}

// Existence, kind, creation, and a real write, in that order. Any answer other
// than a writable directory stops startup: the alternative is an application
// that runs and loses work later.
Failure prepare_directory(DirectoryProbe& probe, const std::string& path) {
    if (path.size() > kMaxDirectoryLength) {
        return {PathFault::DirectoryTooLong,
                "the composed directory is longer than "
                    + std::to_string(kMaxDirectoryLength)
                    + " characters, which leaves no room for file names "
                      "within the Windows path limit"};
    }

    switch (probe.inspect(path)) {
    case DirectoryState::Blocked:
        return {PathFault::OccupiedByFile,
                "a file already occupies this name"};
    case DirectoryState::Unknown:
        return {PathFault::Undeterminable,
                "the location could not be examined; a parent directory "
                "denied access"};
    case DirectoryState::Missing:
        if (!probe.create_directory_tree(path)
            || probe.inspect(path) != DirectoryState::Directory) {
            return {PathFault::CreateFailed,
                    "the directory could not be created"};
        }
        break;
    case DirectoryState::Directory:
        break;
    }

    if (!probe.can_write(path)) {
        return {PathFault::NotWritable,
                "the directory exists but this account cannot write into it"};
    }
    return {};
}

void fail(PathResolution& result, const Failure& failure,
          const std::string& path) {
    result.ok = false;
    result.fault = failure.fault;
    result.message = failure.message;
    result.offending_path = path;
}

}  // namespace

const char* describe_directory_state(DirectoryState state) {
    switch (state) {
    case DirectoryState::Missing:
        return "missing";
    case DirectoryState::Directory:
        return "directory";
    case DirectoryState::Blocked:
        return "occupied by something that is not a directory";
    case DirectoryState::Unknown:
        return "undeterminable";
    }
    return "undeterminable";
}

const char* role_name(PathRole role) {
    switch (role) {
    case PathRole::Data:
        return "data";
    case PathRole::Logs:
        return "logs";
    case PathRole::Backups:
        return "backups";
    case PathRole::Crash:
        return "crash";
    case PathRole::Secrets:
        return "secrets";
    case PathRole::Cache:
        return "cache";
    }
    return "unknown";
}

const char* describe_fault(PathFault fault) {
    switch (fault) {
    case PathFault::None:
        return "no fault";
    case PathFault::IdentityEmpty:
        return "an identity name is empty";
    case PathFault::IdentityTooLong:
        return "an identity name is too long";
    case PathFault::IdentityCharacter:
        return "an identity name contains an unusable character";
    case PathFault::IdentityReserved:
        return "an identity name is a reserved device name";
    case PathFault::RootEmpty:
        return "a root location is empty";
    case PathFault::RootNotAbsolute:
        return "a root location is not absolute";
    case PathFault::RootTooLong:
        return "a root location is too long";
    case PathFault::RootTraversal:
        return "a root location contains a traversal segment";
    case PathFault::RootCharacter:
        return "a root location contains an unusable character";
    case PathFault::DirectoryTooLong:
        return "a composed directory exceeds the usable path length";
    case PathFault::OccupiedByFile:
        return "a file occupies the name of a required directory";
    case PathFault::Undeterminable:
        return "a required directory could not be examined";
    case PathFault::CreateFailed:
        return "a required directory could not be created";
    case PathFault::NotWritable:
        return "a required directory is not writable";
    }
    return "unknown fault";
}

std::string normalize_path(const std::string& raw) {
    std::string forward;
    forward.reserve(raw.size());
    for (const char value : raw) {
        forward.push_back(value == '\\' ? kSeparator : value);
    }

    const bool network_share =
        forward.size() >= 2 && forward[0] == kSeparator && forward[1] == kSeparator;
    const std::string body = network_share ? forward.substr(2) : forward;

    std::string collapsed;
    collapsed.reserve(body.size());
    for (const char value : body) {
        if (value == kSeparator && !collapsed.empty()
            && collapsed.back() == kSeparator) {
            continue;
        }
        collapsed.push_back(value);
    }
    while (collapsed.size() > 1 && collapsed.back() == kSeparator) {
        collapsed.pop_back();
    }

    return network_share ? "//" + collapsed : collapsed;
}

bool is_absolute_path(const std::string& normalized) {
    if (normalized.size() >= 2 && normalized[0] == kSeparator
        && normalized[1] == kSeparator) {
        // A share needs both a server and a share name; '//server' alone is
        // not a place anything can be written to.
        const std::string rest = normalized.substr(2);
        const std::string::size_type slash = rest.find(kSeparator);
        return slash != std::string::npos && slash > 0 && slash + 1 < rest.size();
    }
    if (!normalized.empty() && normalized[0] == kSeparator) {
        return true;
    }
    return normalized.size() >= 3 && is_ascii_letter(normalized[0])
           && normalized[1] == ':' && normalized[2] == kSeparator;
}

bool contains_traversal(const std::string& normalized) {
    for (const std::string& part : segments_of(normalized)) {
        if (part == "." || part == "..") {
            return true;
        }
    }
    return false;
}

bool is_acceptable_identity_name(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    if (name.front() == ' ' || name.back() == ' ' || name.back() == '.') {
        return false;
    }
    for (const char value : name) {
        if (is_control_character(value)) {
            return false;
        }
        const bool ordinary = is_ascii_letter(value) || is_ascii_digit(value)
                              || value == ' ' || value == '-' || value == '_';
        if (!ordinary) {
            return false;
        }
        if (std::string(kForbiddenNameCharacters).find(value)
            != std::string::npos) {
            return false;
        }
    }
    return true;
}

bool PathLayout::is_resolved() const {
    return resolved_;
}

const std::string& PathLayout::directory(PathRole role) const {
    if (!resolved_) {
        throw std::logic_error(
            "PathLayout::directory called on a layout that was never "
            "resolved; check PathResolution::ok first");
    }
    return directories_[static_cast<std::size_t>(role)];
}

std::string PathLayout::database_file() const {
    return join(directory(PathRole::Data), kDatabaseFileName);
}

std::string PathLayout::describe() const {
    if (!resolved_) {
        return "paths: unresolved";
    }
    std::string text;
    for (std::size_t index = 0; index < kPathRoleCount; ++index) {
        const PathRole role = static_cast<PathRole>(index);
        text += role_name(role);
        text += ": ";
        text += directories_[index];
        text += "\n";
    }
    text += "database: ";
    text += database_file();
    return text;
}

PathResolver::PathResolver(DirectoryProbe& probe) : probe_(probe) {}

PathResolution PathResolver::resolve(const PathEnvironment& environment) const {
    PathResolution result;

    const Failure organization =
        check_identity_name(environment.identity.organization, "organization");
    if (organization.fault != PathFault::None) {
        fail(result, organization, environment.identity.organization);
        return result;
    }
    const Failure application =
        check_identity_name(environment.identity.application, "application");
    if (application.fault != PathFault::None) {
        fail(result, application, environment.identity.application);
        return result;
    }

    const std::string shared_root = normalize_path(environment.shared_data_root);
    const Failure shared = check_root(shared_root, "the shared data root");
    if (shared.fault != PathFault::None) {
        fail(result, shared, shared_root);
        return result;
    }

    const std::string data = join(join(shared_root,
                                       environment.identity.organization),
                                  environment.identity.application);

    struct Planned {
        PathRole role;
        std::string path;
    };

    const Planned planned[] = {
        {PathRole::Data, data},
        {PathRole::Logs, join(data, "logs")},
        {PathRole::Backups, join(data, "backups")},
        {PathRole::Crash, join(data, "crash")},
        {PathRole::Secrets, join(data, "secrets")},
    };

    for (const Planned& entry : planned) {
        const Failure failure = prepare_directory(probe_, entry.path);
        if (failure.fault != PathFault::None) {
            fail(result, {failure.fault,
                          std::string(role_name(entry.role)) + " directory: "
                              + failure.message},
                 entry.path);
            return result;
        }
        result.layout.directories_[static_cast<std::size_t>(entry.role)] =
            entry.path;
    }

    // The cache is the one location allowed to disappoint. It holds nothing
    // that cannot be rebuilt, so an unusable per-account location becomes a
    // warning and a directory inside the data root, not a refusal to start.
    const std::string cache_root = normalize_path(environment.user_cache_root);
    const Failure cache_root_check = check_root(cache_root, "the cache root");
    std::string cache;
    if (cache_root_check.fault == PathFault::None) {
        cache = join(join(join(cache_root, environment.identity.organization),
                          environment.identity.application),
                     "cache");
        const Failure failure = prepare_directory(probe_, cache);
        if (failure.fault != PathFault::None) {
            result.warnings.push_back("the per-account cache at '" + cache
                                      + "' is unusable (" + failure.message
                                      + "); using a cache inside the data "
                                        "directory instead");
            cache.clear();
        }
    } else {
        result.warnings.push_back("the per-account cache root is unusable ("
                                  + cache_root_check.message
                                  + "); using a cache inside the data "
                                    "directory instead");
    }

    if (cache.empty()) {
        cache = join(data, "cache");
        const Failure failure = prepare_directory(probe_, cache);
        if (failure.fault != PathFault::None) {
            fail(result, {failure.fault,
                          std::string("cache directory: ") + failure.message},
                 cache);
            return result;
        }
    }
    result.layout.directories_[static_cast<std::size_t>(PathRole::Cache)] = cache;

    result.layout.resolved_ = true;
    result.ok = true;
    return result;
}

}  // namespace squiflow::platform
