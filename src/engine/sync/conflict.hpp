#pragma once

// What happens when the same record was changed in two places.
//
// With two devices this is rare, but rare is not never: the staff device edits
// an invoice while offline, the owner edits the same invoice on the server,
// and later the two meet. The decision was made once, by the shopkeeper, and
// it is written here rather than argued about per screen:
//
//   The owner's version wins. The losing version is never thrown away.
//
// The second half matters as much as the first. A resolution that discards
// work silently is how a person stops trusting the machine, so the loser is
// kept, attributed and readable, and a person can copy anything out of it.
//
// When neither side is the owner, the server's version wins, because the
// server is the system of record and two devices cannot agree on the time.

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <squiflow/protocol/module_id.hpp>

#include "engine/storage/store.hpp"
#include "engine/sync/outbox.hpp"

namespace squiflow::engine {

enum class ConflictOutcome : std::uint8_t {
    // The version already on the server stands; the local change is retained
    // as superseded and its outbox entry is dropped.
    RemoteWins,
    // The local change stands and is sent again, this time on top of the
    // remote version.
    LocalWins,
    // Neither side may be discarded automatically. A person looks at it.
    NeedsAPerson,
};

std::string_view to_string(ConflictOutcome outcome) noexcept;

// Everything the rule is allowed to look at. Deliberately small: a decision
// that depends on the contents of the record is a decision a person should be
// making.
struct ConflictFacts {
    protocol::ModuleId module{};
    std::string record_id{};
    bool local_change_by_owner{false};
    bool remote_change_by_owner{false};
    // A record that has been issued - an invoice, a quotation - is not
    // something either side may quietly overwrite.
    bool record_is_final{false};
    std::int64_t remote_sequence{0};
};

struct ConflictDecision {
    ConflictOutcome outcome{ConflictOutcome::NeedsAPerson};
    std::string reason{};
};

// A pure function of the facts. No database, no clock, no connection: the
// rule can be read and tested on its own, which is the point of writing it
// down once.
ConflictDecision decide_conflict(const ConflictFacts& facts) noexcept;

struct SupersededVersion {
    std::string id{};
    protocol::ModuleId module{};
    std::string record_id{};
    protocol::OperationId operation{};
    Blob payload{};
    std::string reason{};
    std::int64_t recorded_at{0};
    bool seen_by_a_person{false};
};

// The record of what lost, and the queue of things a person still has to look
// at.
class ConflictLog {
public:
    using Clock = std::function<std::int64_t()>;

    static const std::string& table_name();
    static void define(Store& store);

    explicit ConflictLog(Clock clock);

    // Applies the decision to the outbox entry and keeps whatever loses.
    // Returns the decision so the caller does not have to make it twice.
    ConflictDecision resolve(Transaction& transaction, const Outbox& outbox,
                             const OutboxEntry& entry, const ConflictFacts& facts) const;

    void retain(Transaction& transaction, const OutboxEntry& entry,
                const std::string& reason) const;

    void mark_seen(Transaction& transaction, const std::string& id) const;

    std::vector<SupersededVersion> for_record(const Store& store,
                                              const std::string& record_id) const;
    std::vector<SupersededVersion> needing_attention(const Store& store) const;
    std::size_t count(const Store& store) const;

private:
    Clock clock_;
};

}  // namespace squiflow::engine
