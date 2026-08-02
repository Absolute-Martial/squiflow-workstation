#pragma once

// The write side of agreements: four handlers, one per operation on the wire.
//
// Every one of them is online-only, and closing and reopening are stronger
// still - they require a live connection rather than merely queueing. That is
// not caution for its own sake. A closed agreement changes what every later
// invoice is allowed to charge, so a device that has been offline for a day
// must not be able to close one retroactively and reprice work that has
// already been quoted against it.
//
// As in quotations, the protocol declares no line-level operation, so agreed
// rates arrive with the document as indexed payload fields - line_count, then
// line.0.id, line.0.product_id and so on - and the whole set is written in one
// transaction or none of it is.

#include <cstdint>
#include <functional>

#include "engine/storage/store.hpp"
#include "modules/context.hpp"

namespace squiflow::modules::agreements {

// A damaged or hostile payload must not be able to make this process allocate
// an arbitrary number of lines before any rule is checked. No real agreement
// approaches this; a corrupted length field does.
inline constexpr std::int64_t kMaxLines = 500;

// How long before its end date an agreement starts asking for attention.
// Thirty days, in milliseconds. Written once here rather than at each call
// site, because a warning window that differs between two screens is a warning
// nobody trusts.
inline constexpr std::int64_t kExpiryWindowMs = 30LL * 24 * 60 * 60 * 1000;

class AgreementsService {
public:
    using Clock = std::function<std::int64_t()>;

    explicit AgreementsService(Clock clock) : clock_{std::move(clock)} {}

    void create(engine::Transaction& transaction, const Call& call) const;
    void update(engine::Transaction& transaction, const Call& call) const;
    void close(engine::Transaction& transaction, const Call& call) const;
    void reopen(engine::Transaction& transaction, const Call& call) const;

private:
    Clock clock_;
};

}  // namespace squiflow::modules::agreements
