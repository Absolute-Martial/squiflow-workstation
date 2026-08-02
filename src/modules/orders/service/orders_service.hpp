#pragma once

// The write side of orders: four handlers, one per operation on the wire.
//
// Note what is absent. No handler accepts a price. A line is priced by asking
// pricing at the moment it is added, and the answer is copied onto the line.
// If somebody needs a different number, they record an override against that
// line first, which costs them the right_rate_override right and forces them
// to give a reason. Letting an order carry a typed-in price would route around
// both, and the shop would lose the ability to answer "why was this 450".

#include <cstdint>
#include <functional>
#include <string>

#include "engine/identity/session.hpp"
#include "engine/storage/store.hpp"
#include "modules/context.hpp"
#include "modules/orders/data/repository.hpp"
#include "modules/orders/domain/order.hpp"
#include "modules/pricing/service/pricing_service.hpp"

namespace squiflow::modules::orders {

class OrdersService {
public:
    using Clock = std::function<std::int64_t()>;

    explicit OrdersService(Clock clock) : clock_{std::move(clock)} {}

    void create(engine::Transaction& transaction, const Call& call) const;
    void update(engine::Transaction& transaction, const Call& call) const;
    void add_line(engine::Transaction& transaction, const Call& call) const;
    void cancel(engine::Transaction& transaction, const Call& call) const;

private:
    Clock clock_;
};

// Decode a call's payload into fields, turning a malformed payload into a rule
// violation the caller can be shown rather than an exception type from the
// storage layer.
engine::Row read_fields(const Call& call);

// The signed-in person behind a call. A write with no session is a programming
// error in the gate above, not a rule the user broke.
const engine::Session& actor(const Call& call);

}  // namespace squiflow::modules::orders
