#pragma once

#include <string>

#include "engine/records/identity.hpp"
#include "engine/records/money.hpp"
#include "engine/records/quantity.hpp"

namespace squiflow::engine {

// A line as it was agreed, frozen.
//
// The product name is copied, not referenced. If the shop renames a product
// next year, last year's invoice must still read the way the customer's copy
// reads. Same for the rate: a rate change is never allowed to reach backwards
// into a document that has been issued.
struct LineSnapshot {
    RecordId product;        // may be invalid: off-catalog lines are allowed
    std::string description; // as printed
    Quantity quantity;
    Money rate;
    Money amount;            // quantity x rate, computed once and stored

    // Why this rate rather than the catalog rate. Blank for an ordinary sale.
    // Filled in for an agreement rate or a manual override, so that a question
    // months later has an answer.
    std::string rate_reason;
};

// Where a rate came from. Recorded on every line, because "why was this priced
// like that" is the most common question a shop gets asked.
enum class RateOrigin : std::uint8_t {
    CatalogDefault,
    PartySpecific,
    Agreement,
    ManualOverride,
    OffCatalog,
};

std::string_view to_string(RateOrigin origin) noexcept;

}  // namespace squiflow::engine
