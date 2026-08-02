#pragma once

// Reads are templates over the reader so that the same function serves a plain
// Store and an open Transaction. A handler mid-write must see its own
// uncommitted rows; a screen reading prices must not need a transaction to do
// it. Both call these.

#include <optional>
#include <string>
#include <vector>

#include "engine/storage/store.hpp"
#include "modules/pricing/data/tables.hpp"
#include "modules/pricing/domain/rate.hpp"

namespace squiflow::modules::pricing::data {

template <typename Reader>
std::optional<Rate> find_rate(const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kRate, id);
    if (!row) {
        return std::nullopt;
    }
    return rate_from_row(*row);
}

template <typename Reader>
std::optional<DefaultRate> find_default_rate(const Reader& reader,
                                            const std::string& product_id) {
    const auto row = reader.find(tables::kDefaultRate, product_id);
    if (!row) {
        return std::nullopt;
    }
    return default_rate_from_row(*row);
}

template <typename Reader>
std::optional<RateOverride> find_override(const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kRateOverride, id);
    if (!row) {
        return std::nullopt;
    }
    return override_from_row(*row);
}

// Every rate recorded against a product, whatever party or window it names.
// This is deliberately unfiltered: choosing between them is choose_rate's job,
// and doing the filtering here in a query would put half the resolution rules
// somewhere no test can reach without a database.
template <typename Reader>
std::vector<Rate> rates_for_product(const Reader& reader, const std::string& product_id) {
    engine::Query query{tables::kRate};
    query.where_equals("product_id", engine::Value::text(product_id));
    query.order_by("id");
    std::vector<Rate> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(rate_from_row(row));
    }
    return result;
}

// Every override recorded against one line, oldest first. Ordered by when it
// was applied and then by id, so the sequence is the same on every device even
// when two overrides share a timestamp.
template <typename Reader>
std::vector<RateOverride> overrides_for_line(const Reader& reader,
                                             const std::string& line_id) {
    engine::Query query{tables::kRateOverride};
    query.where_equals("line_id", engine::Value::text(line_id));
    query.order_by("applied_at");
    query.order_by("id");
    std::vector<RateOverride> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(override_from_row(row));
    }
    return result;
}

// The override that currently governs a line, which is the last one applied.
// Earlier overrides are kept but no longer decide the price.
template <typename Reader>
std::optional<RateOverride> latest_override_for_line(const Reader& reader,
                                                    const std::string& line_id) {
    const std::vector<RateOverride> all = overrides_for_line(reader, line_id);
    if (all.empty()) {
        return std::nullopt;
    }
    return all.back();
}

// Writes. A transaction is required, and the caller cannot open one, so a
// write outside the gate above is not expressible here.
void save_rate(engine::Transaction& transaction, const Rate& rate);
void save_default_rate(engine::Transaction& transaction, const DefaultRate& rate);
void save_override(engine::Transaction& transaction, const RateOverride& override_);

// True when the rate existed and was removed. False when it was not there,
// which is not an error: the caller wanted it gone and it is gone.
bool remove_rate(engine::Transaction& transaction, const std::string& id);

}  // namespace squiflow::modules::pricing::data
