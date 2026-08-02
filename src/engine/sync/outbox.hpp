#pragma once

// The outbox: local changes waiting to reach the server.
//
// Every change a person makes is written locally and queued here, then sent
// when there is a connection. That is what makes the shop usable when the
// line is down, and it is also where the two worst failures in the whole
// system live:
//
//   A change sent twice.    A retry after a timeout charges a customer twice.
//                           Prevented by a client-generated idempotency key
//                           that survives the retry, so the server can
//                           recognise the second copy as the same change.
//
//   A change sent in the    A payment arriving before the invoice it pays
//   wrong order.            makes no sense on the server. Entries for the
//                           same record are sent strictly in the order they
//                           were made, and a stuck one blocks that record -
//                           only that record - until a person deals with it.
//
// Enqueuing is not a separate step from making the change. Both happen in the
// caller's transaction, because a change written without its outbox entry is
// a change that silently never syncs.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <squiflow/protocol/module_id.hpp>
#include <squiflow/protocol/operation_table.hpp>

#include "engine/storage/store.hpp"

namespace squiflow::engine {

enum class OutboxState : std::uint8_t {
    // Waiting to be sent, or waiting for its backoff to expire.
    Pending,
    // Handed to the sync orchestrator. Whether the server saw it is unknown.
    InFlight,
    // The server confirmed receipt but has not yet assigned a sequence.
    Acknowledged,
    // Applied on the server, with a sequence number. Prunable.
    Applied,
    // The server refused it because someone else changed the same record.
    // Never retried automatically; a person decides.
    Conflicted,
    // Retried until it was clear that retrying is not the answer. Needs
    // attention rather than patience.
    Failed,
};

std::string_view to_string(OutboxState state) noexcept;

struct OutboxEntry {
    // Generated on this device before the first attempt, and never changed by
    // a retry. This is the whole defence against a double charge.
    std::string idempotency_key{};

    protocol::OperationId operation{};
    std::string record_id{};
    Blob payload{};

    OutboxState state{OutboxState::Pending};
    std::int64_t position{0};
    std::int64_t created_at{0};
    std::int64_t updated_at{0};
    std::int64_t due_at{0};
    std::int32_t attempts{0};
    std::int64_t server_sequence{0};
    std::string last_error{};

    protocol::ModuleId module() const noexcept;
};

enum class EnqueueResult : std::uint8_t {
    Enqueued,
    // The same idempotency key is already queued. Enqueuing is itself
    // idempotent, so a repeated attempt is not an error.
    AlreadyQueued,
};

struct OutboxCounts {
    std::size_t pending{0};
    std::size_t in_flight{0};
    std::size_t acknowledged{0};
    std::size_t applied{0};
    std::size_t conflicted{0};
    std::size_t failed{0};

    std::size_t total() const noexcept;
    // Everything that still has to reach the server, or that a person still
    // has to look at. The updater refuses to install while this is not zero.
    std::size_t unfinished() const noexcept;
};

class Outbox {
public:
    using Clock = std::function<std::int64_t()>;

    // Fifty to a hundred per batch. Small enough that a weak line finishes a
    // batch before it drops, large enough that a day of offline work does not
    // take a thousand round trips.
    static constexpr std::size_t kMinimumBatch = 1;
    static constexpr std::size_t kDefaultBatch = 50;
    static constexpr std::size_t kMaximumBatch = 100;

    // 5s, 10s, 20s, 40s ... capped at five minutes.
    static constexpr std::int64_t kFirstBackoffMs = 5000;
    static constexpr std::int64_t kMaximumBackoffMs = 300000;

    // After this many attempts, waiting longer is not going to help.
    static constexpr std::int32_t kAttemptsBeforeGivingUp = 10;

    static const std::string& table_name();

    // Called by a migration. Idempotent.
    static void define(Store& store);

    explicit Outbox(Clock clock);

    // --- writes, all inside the caller's transaction ---

    EnqueueResult enqueue(Transaction& transaction, const OutboxEntry& entry) const;

    // Takes the next batch that is due and marks it in flight. Returns them in
    // the order they were made.
    std::vector<OutboxEntry> claim(Transaction& transaction,
                                   std::size_t batch = kDefaultBatch) const;

    void acknowledge(Transaction& transaction, const std::string& key) const;
    void mark_applied(Transaction& transaction, const std::string& key,
                      std::int64_t server_sequence) const;
    void retry_later(Transaction& transaction, const std::string& key,
                     const std::string& error) const;
    void mark_conflicted(Transaction& transaction, const std::string& key,
                         const std::string& reason) const;
    void mark_failed(Transaction& transaction, const std::string& key,
                     const std::string& error) const;

    // Send it again immediately rather than after a backoff. Used when a
    // conflict was resolved in favour of this device: the change is still
    // wanted, and it is not the line that was at fault, so the attempt is not
    // counted against the give-up limit.
    void resend(Transaction& transaction, const std::string& key,
                const std::string& reason) const;

    // Abandon it. Only for a change that has been dealt with some other way -
    // a conflict resolved in favour of the server, with the losing version
    // kept in the conflict log. Never a way to make an inconvenient entry go
    // away: an entry that has never left the device cannot be discarded.
    void discard(Transaction& transaction, const std::string& key) const;

    // At startup, anything still in flight was interrupted by a crash or a
    // power cut, and there is no way to know whether the server saw it.
    // Sending it again is safe precisely because of the idempotency key, so
    // these go back to pending rather than being guessed about.
    std::size_t recover(Transaction& transaction) const;

    std::size_t prune_applied(Transaction& transaction, std::int64_t applied_before) const;

    // --- reads ---

    std::optional<OutboxEntry> get(const Store& store, const std::string& key) const;
    std::vector<OutboxEntry> in_state(const Store& store, OutboxState state) const;
    std::vector<OutboxEntry> for_record(const Store& store, const std::string& record_id) const;
    OutboxCounts counts(const Store& store) const;
    bool drained(const Store& store) const;

private:
    OutboxEntry require(const Transaction& transaction, const std::string& key) const;
    void store_state(Transaction& transaction, OutboxEntry& entry, OutboxState state) const;
    std::int64_t next_position(const Transaction& transaction) const;

    Clock clock_;
};

std::int64_t backoff_for_attempt(std::int32_t attempts) noexcept;

}  // namespace squiflow::engine
