#pragma once

// The only seam between path resolution and a real disk.
//
// Path resolution has to ask questions that only a filesystem can answer:
// does this exist, is it a directory or a file sitting where a directory
// belongs, and can this account actually write into it. Those three answers
// decide whether the application is allowed to start, so they are the three
// methods here and nothing else.
//
// Writability is proven by writing. An access mask can say yes while a quota,
// a network share, or a group policy says no, and the place to discover that
// is startup rather than the first invoice of the day.
//
// See docs/adr/0005-directory-probe-seam.md.

#include <cstdint>
#include <string>

namespace squiflow::platform {

// What is at a path right now. Unknown is not Missing: it means the question
// could not be answered, usually because a parent denied traversal, and a
// resolver must treat it as a refusal rather than as an invitation to create.
enum class DirectoryState : std::uint8_t {
    Missing,
    Directory,
    Blocked,
    Unknown,
};

const char* describe_directory_state(DirectoryState state);

class DirectoryProbe {
public:
    virtual ~DirectoryProbe() = default;

    DirectoryProbe(const DirectoryProbe&) = delete;
    DirectoryProbe& operator=(const DirectoryProbe&) = delete;
    DirectoryProbe(DirectoryProbe&&) = delete;
    DirectoryProbe& operator=(DirectoryProbe&&) = delete;

    // Never throws: a filesystem error is an answer, not an exception.
    virtual DirectoryState inspect(const std::string& path) const = 0;

    // Creates the path and every missing parent. Returns false if the tree
    // does not exist as a directory afterwards, for any reason.
    virtual bool create_directory_tree(const std::string& path) = 0;

    // True only if a file was actually created, written, and removed.
    virtual bool can_write(const std::string& path) const = 0;

protected:
    DirectoryProbe() = default;
};

}  // namespace squiflow::platform
