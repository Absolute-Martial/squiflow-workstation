#pragma once

// Where this device has read up to.
//
// The server assigns every applied change a sequence number. Pulling asks for
// "everything after N" rather than "everything changed since a timestamp",
// because two machines never agree on the time and a clock that goes backwards
// would silently skip a day of work.
//
// One cursor per module, not one for the whole shop. A module that is not
// active pulls nothing, and a module whose pull fails does not hold back the
// others.

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <squiflow/protocol/module_id.hpp>

#include "engine/storage/store.hpp"

namespace squiflow::engine {

struct CursorPosition {
    protocol::ModuleId module{};
    // The highest sequence this device has applied. Zero means nothing has
    // ever been pulled, which is how a new device is recognised.
    std::int64_t sequence{0};
    std::int64_t last_attempt_at{0};
    std::int64_t last_success_at{0};
    std::int32_t consecutive_failures{0};
    std::string last_error{};

    bool never_pulled() const noexcept;
};

class Cursor {
public:
    using Clock = std::function<std::int64_t()>;

    static const std::string& table_name();
    static void define(Store& store);

    explicit Cursor(Clock clock);

    CursorPosition position(const Store& store, protocol::ModuleId module) const;
    std::vector<CursorPosition> all(const Store& store) const;

    // Moves forward after a pulled batch has been applied. Advancing and
    // applying are the same transaction: a cursor moved without the rows is a
    // batch lost for good, and a batch applied without the cursor is one
    // applied twice.
    void advance(Transaction& transaction, protocol::ModuleId module,
                 std::int64_t sequence) const;

    void record_failure(Transaction& transaction, protocol::ModuleId module,
                        const std::string& error) const;

    // Back to zero, for a device that has been away long enough that the
    // server no longer holds the intervening changes. The next pull is a full
    // one.
    void reset(Transaction& transaction, protocol::ModuleId module) const;

    // Modules that have never pulled anything. A new device needs a full
    // first pull for each of these.
    std::vector<protocol::ModuleId> never_pulled(const Store& store) const;

private:
    CursorPosition load(const Transaction& transaction, protocol::ModuleId module) const;
    void save(Transaction& transaction, const CursorPosition& position) const;

    Clock clock_;
};

}  // namespace squiflow::engine
