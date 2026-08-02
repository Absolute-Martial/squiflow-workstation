#pragma once

// Log storage on a real disk, using the standard library only.
//
// The directory is fixed at construction and every operation is confined to
// it: a name containing a separator, a drive, or a parent reference is
// refused rather than resolved. Nothing above this class can be tricked into
// writing outside the logs directory by a configured file name.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "platform/log_storage.hpp"

namespace squiflow::platform {

// True only for a plain file name: no separator, no colon, no dot-only name,
// no control character, and short enough to survive on Windows.
bool is_plain_file_name(std::string_view name);

class LocalLogStorage final : public LogStorage {
public:
    explicit LocalLogStorage(std::string directory);

    const std::string& directory() const { return directory_; }

    bool append(const std::string& name, std::string_view bytes) override;
    std::optional<std::uint64_t> size_of(const std::string& name) const override;
    bool exists(const std::string& name) const override;
    bool rename(const std::string& from, const std::string& to) override;
    bool remove(const std::string& name) override;
    void flush() override;

private:
    std::string full_path(const std::string& name) const;

    std::string directory_;
};

}  // namespace squiflow::platform
