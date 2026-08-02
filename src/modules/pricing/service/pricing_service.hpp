#pragma once

// The write side of pricing: four handlers, one per operation on the wire.
//
// The read side is deliberately not a method on this class. Resolving a price
// is a pure question asked of stored rows, and orders must be able to ask it
// from inside its own open transaction, before anything is committed. A member
// function would force a caller that only wants to read to hold a service and a
// clock it has no use for, so resolution is exposed as free templates over the
// reader instead.

#include <cstdint>
#include <functional>
#include <string>

#include "engine/identity/session.hpp"
#include "engine/storage/store.hpp"
#include "modules/context.hpp"
#include "modules/pricing/data/repository.hpp"
#include "modules/pricing/domain/rate.hpp"

namespace squiflow::modules::pricing {

// What a line actually costs, and why.
//
// overridden is separate from source because an override does not replace the
// reason a price existed; it sits on top of it. The counter needs to be able to
// say "the standard price is 500, changed to 450 because the customer supplied
// their own paper".
struct EffectivePrice {
    bool found{false};
    std::int64_t amount_minor{0};
    RateSource source{RateSource::None};
    bool overridden{false};
    std::string reason{};
};

// Resolve the rate for a product at a moment, party rate first, then catch-all,
// then the product's standard price. Nothing found stays nothing found; it does
// not become zero.
template <typename Reader>
ResolvedRate resolve_rate(const Reader& reader,
                          const std::string& product_id,
                          const std::string& party_id,
                          std::int64_t at) {
    if (product_id.empty()) {
        return {};
    }
    const std::vector<Rate> candidates = data::rates_for_product(reader, product_id);
    const ResolvedRate chosen = choose_rate(candidates, party_id, at);
    if (chosen.found()) {
        return chosen;
    }
    const auto fallback = data::find_default_rate(reader, product_id);
    if (!fallback) {
        return {};
    }
    return with_default(chosen, *fallback);
}

// What one line costs, override included.
//
// An override stands on its own: a line can be priced by hand even when the
// product has no rate at all, which is how a one-off job with no catalogue
// entry gets billed. So an override reports found even when resolution found
// nothing, and the resolved source is still carried for the explanation.
template <typename Reader>
EffectivePrice effective_price(const Reader& reader,
                               const std::string& line_id,
                               const std::string& product_id,
                               const std::string& party_id,
                               std::int64_t at) {
    const ResolvedRate resolved = resolve_rate(reader, product_id, party_id, at);

    EffectivePrice price;
    price.source = resolved.source;
    price.found = resolved.found();
    price.amount_minor = resolved.amount_minor;

    if (!line_id.empty()) {
        const auto override_ = data::latest_override_for_line(reader, line_id);
        if (override_) {
            price.found = true;
            price.overridden = true;
            price.amount_minor = override_->overridden_minor;
            price.reason = override_->reason;
        }
    }

    return price;
}

class PricingService {
public:
    using Clock = std::function<std::int64_t()>;

    explicit PricingService(Clock clock) : clock_{std::move(clock)} {}

    void set_rate(engine::Transaction& transaction, const Call& call) const;
    void remove_rate(engine::Transaction& transaction, const Call& call) const;
    void override_rate(engine::Transaction& transaction, const Call& call) const;
    void set_default_rate(engine::Transaction& transaction, const Call& call) const;

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

}  // namespace squiflow::modules::pricing
