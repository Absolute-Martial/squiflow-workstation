#pragma once

// The standard-library implementation of the directory probe.
//
// Small on purpose: everything interesting about path resolution is tested
// against the fake, and this class exists so that the one remaining question,
// whether the standard library agrees with the fake about what a directory is,
// can be answered against a temporary directory in the same suite.

#include <string>

#include "platform/directory_probe.hpp"

namespace squiflow::platform {

class LocalDirectoryProbe final : public DirectoryProbe {
public:
    LocalDirectoryProbe() = default;

    DirectoryState inspect(const std::string& path) const override;
    bool create_directory_tree(const std::string& path) override;

    // Writes a uniquely named file, flushes it, then removes it. A permission
    // bit is an opinion; a completed write is a fact.
    bool can_write(const std::string& path) const override;
};

}  // namespace squiflow::platform
