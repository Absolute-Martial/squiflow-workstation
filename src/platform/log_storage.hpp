#pragma once

// The seam between rotation and a real disk.
//
// Rotation is arithmetic and bookkeeping: how big is this file, shift the
// generations, delete the oldest, stay under the budget. None of that needs a
// disk to be tested, and all of the failures worth testing - a rename that
// fails, a delete that is refused, a size that cannot be read, a volume that
// stops accepting bytes - are awkward to arrange with a real one.
//
// Names are plain file names. The implementation owns the directory, so no
// caller above this line ever composes a path.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace squiflow::platform {

class LogStorage {
public:
    virtual ~LogStorage() = default;

    LogStorage(const LogStorage&) = delete;
    LogStorage& operator=(const LogStorage&) = delete;
    LogStorage(LogStorage&&) = delete;
    LogStorage& operator=(LogStorage&&) = delete;

    // Appends bytes to a file, creating it if needed. Never truncates.
    virtual bool append(const std::string& name, std::string_view bytes) = 0;

    // The size in bytes, or nothing if the file does not exist or cannot be
    // measured. Nothing is not zero: an unmeasurable file must not be treated
    // as empty room inside the budget.
    virtual std::optional<std::uint64_t> size_of(
        const std::string& name) const = 0;

    virtual bool exists(const std::string& name) const = 0;

    virtual bool rename(const std::string& from, const std::string& to) = 0;

    // True if the file is gone afterwards, including when it never existed.
    virtual bool remove(const std::string& name) = 0;

    virtual void flush() = 0;

protected:
    LogStorage() = default;
};

}  // namespace squiflow::platform
