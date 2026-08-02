#pragma once

// The writer gate.
//
// SQLite in write-ahead mode allows exactly one writer. Fourteen background
// services and an interface all want to write, and the naive answer - let
// them try, and retry on SQLITE_BUSY - is the trapdoor: retries are unfair,
// so a busy service can starve the person standing at the counter, and the
// failure only appears under load, which is to say at the shop rather than
// here.
//
// So writes queue instead of colliding. The queue is a ticket, taken under a
// lock and served in order, which makes it first-come-first-served with no
// possibility of starvation. A writer that gives up releases its ticket and
// the queue skips it.
//
// Readers do not take a ticket. They share a lock with each other and are
// excluded only for the moment a write is actually applying, so a slow write
// never blocks a list from drawing.

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <set>
#include <shared_mutex>

#include "engine/storage/store.hpp"

namespace squiflow::engine {

class Writer {
public:
    // A unit of work runs inside a transaction that is already open. It does
    // not commit: the gate commits when the work returns, and rolls back if it
    // throws. A caller cannot forget to do either.
    using Work = std::function<void(Transaction&)>;
    using Read = std::function<void(const Store&)>;

    struct Statistics {
        std::uint64_t completed{0};
        std::uint64_t failed{0};
        std::uint64_t abandoned{0};
        std::uint64_t peak_waiting{0};
        std::uint64_t served{0};
    };

    explicit Writer(Store& store);

    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;
    Writer(Writer&&) = delete;
    Writer& operator=(Writer&&) = delete;
    ~Writer() = default;

    // Waits for its turn, then applies the work and commits. If the work
    // throws, the transaction is rolled back, the queue moves on, and the
    // exception reaches the caller unchanged.
    void write(const Work& work);

    // The same, but gives up if its turn has not arrived within the patience
    // given. Returns false without having written anything. Used by
    // background work that would rather be skipped than delay a person.
    bool write_within(const Work& work, std::chrono::milliseconds patience);

    // Reads share with other reads and are excluded only while a write is
    // applying.
    void read(const Read& reader) const;

    std::uint64_t waiting() const;
    Statistics statistics() const;

private:
    void finish_turn();
    void skip_abandoned();

    Store& store_;

    mutable std::mutex queue_mutex_;
    std::condition_variable turn_changed_;
    std::uint64_t next_ticket_{0};
    std::uint64_t serving_{0};
    std::uint64_t waiting_{0};
    std::set<std::uint64_t> abandoned_{};
    Statistics statistics_{};

    mutable std::shared_mutex data_mutex_;
};

}  // namespace squiflow::engine
