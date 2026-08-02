#pragma once

// What a handler is given, and what it may hand back.
//
// A handler receives a transaction or a store, never the database. It cannot
// open a transaction of its own, cannot start a second one inside the first,
// and cannot commit early: the gate above it owns all of that. This is the
// single writer rule expressed as a type rather than as a paragraph in a
// document that nobody reads twice.

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#include <squiflow/protocol/operation_table.hpp>

#include "engine/identity/capability.hpp"
#include "engine/storage/store.hpp"

namespace squiflow::modules {

// A refusal by the module itself, in words for the person in front of the
// screen: "that username is taken", "the owner cannot be disabled".
//
// Thrown, not returned, for one reason: it is thrown from inside a
// transaction, and throwing is what rolls that transaction back. A refusal
// returned as a value would leave the caller free to ignore it and carry on
// writing. Everything half-written disappears, and the registry turns it into
// an ordinary refused Outcome for the caller.
class RuleViolation : public std::runtime_error {
public:
    explicit RuleViolation(const std::string& message) : std::runtime_error(message) {}
};

// One request to do one thing.
struct Call {
    protocol::OperationId operation{};

    // The record being acted on. Empty for an operation that creates one and
    // for a query. Required for anything that will be synchronised, because
    // ordering is per record and a change with no record cannot be ordered.
    std::string record_id{};

    // The encoded arguments. The framework never looks inside; only the
    // module's own handler and the server's matching handler know the shape.
    engine::Blob payload{};

    // Generated on this device before the first attempt. Required for a
    // synchronised change and refused for anything else, so that the field
    // cannot drift into meaning "a request id".
    std::string idempotency_key{};

    // Who asked, and from which device. Filled in by the registry from the
    // session it already checked the rules against; anything a caller puts
    // here is overwritten.
    //
    // It is not optional in practice: almost every record carries who made it
    // and who last changed it, and a module that had to be *passed* the actor
    // separately would eventually be passed a different one from the one the
    // rules were checked against. Never null inside a handler.
    const engine::Session* actor{nullptr};
};

struct Outcome {
    bool ok{false};

    // Why it was refused, when it was refused by the rules rather than by the
    // handler. None with ok false means the handler itself refused.
    engine::DenialReason reason{engine::DenialReason::None};

    // Written for the person, not for a log file.
    std::string error{};

    // Rows, for a read. A write returns none.
    std::vector<engine::Row> rows{};

    // True when this change was placed in the outbox to be sent. False for a
    // local-only change, and false for a repeat of one already queued.
    bool queued{false};

    // True when this exact change had already been made on this device, found
    // by its idempotency key, and so was not made again. Not an error: a
    // retried request is supposed to be harmless, which is the entire reason
    // the key exists.
    bool replayed{false};
};

// A write handler runs inside the caller's transaction. Throwing StoreError
// rolls the whole thing back, including the outbox entry, which is the
// behaviour we want: a change that was not made must not be sent.
using WriteHandler = std::function<void(engine::Transaction&, const Call&)>;

// A read handler gets the store, not a transaction, and cannot write with it.
using ReadHandler = std::function<std::vector<engine::Row>(const engine::Store&, const Call&)>;

}  // namespace squiflow::modules
