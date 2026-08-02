#include "modules/agreements/domain/consumption.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

#include "modules/context.hpp"

namespace squiflow::modules::agreements {
namespace {
bool blank(const std::string& value) noexcept {
    return std::all_of(value.begin(), value.end(), [](char c) {
        return static_cast<unsigned char>(c) <= static_cast<unsigned char>(' ');
    });
}
std::uint64_t hash(const std::string& input, std::uint64_t value) noexcept {
    for (char c : input) {
        value ^= static_cast<std::uint64_t>(static_cast<unsigned char>(c));
        value *= 1099511628211ULL;
    }
    return value;
}
}

std::string consumption_id(const std::string& invoice_line_id,
                           const std::string& agreement_line_id) {
    const std::string input = invoice_line_id + ":" + agreement_line_id;
    const std::uint64_t high = hash(input, 14695981039346656037ULL);
    const std::uint64_t low = hash(input, 1099511628211ULL);
    static constexpr char hex[] = "0123456789abcdef";
    std::string result(32U, '0');
    auto write = [&](std::uint64_t value, std::size_t offset) {
        for (std::size_t i = 0; i < 16U; ++i) {
            const std::size_t shift = (15U - i) * 4U;
            result[offset + i] = hex[static_cast<std::size_t>((value >> shift) & 0x0fU)];
        }
    };
    write(high, 0U); write(low, 16U);
    if (high == 0U && low == 0U) result.back() = '1';
    return result;
}

void validate(const AgreementConsumption& value) {
    if (blank(value.id) || blank(value.agreement_id) ||
        blank(value.agreement_line_id) || blank(value.invoice_id) ||
        blank(value.invoice_line_id)) {
        throw RuleViolation("Agreement consumption must retain every source identity.");
    }
    if (value.quantity_scaled <= 0 || value.consumed_at <= 0 ||
        blank(value.consumed_by)) {
        throw RuleViolation("Agreement consumption must retain positive quantity and creation evidence.");
    }
    switch (value.state) {
        case ConsumptionState::Active:
            if (value.released_at != 0 || !blank(value.released_by) ||
                !blank(value.release_reason)) {
                throw RuleViolation("Active agreement consumption cannot carry release evidence.");
            }
            break;
        case ConsumptionState::Released:
            if (value.released_at < value.consumed_at || blank(value.released_by) ||
                blank(value.release_reason)) {
                throw RuleViolation("Released agreement consumption needs complete ordered evidence.");
            }
            break;
        default:
            throw RuleViolation("That agreement consumption state is not understood.");
    }
}

engine::Row to_row(const AgreementConsumption& value) {
    engine::Row row;
    row.set("id", engine::Value::text(value.id));
    row.set("agreement_id", engine::Value::text(value.agreement_id));
    row.set("agreement_line_id", engine::Value::text(value.agreement_line_id));
    row.set("invoice_id", engine::Value::text(value.invoice_id));
    row.set("invoice_line_id", engine::Value::text(value.invoice_line_id));
    row.set("quantity_scaled", engine::Value::integer(value.quantity_scaled));
    row.set("state", engine::Value::integer(static_cast<std::int64_t>(value.state)));
    row.set("consumed_at", engine::Value::integer(value.consumed_at));
    row.set("consumed_by", engine::Value::text(value.consumed_by));
    row.set("released_at", engine::Value::integer(value.released_at));
    row.set("released_by", engine::Value::text(value.released_by));
    row.set("release_reason", engine::Value::text(value.release_reason));
    return row;
}

AgreementConsumption consumption_from_row(const engine::Row& row) {
    AgreementConsumption value;
    value.id = row.get("id").text_or({});
    value.agreement_id = row.get("agreement_id").text_or({});
    value.agreement_line_id = row.get("agreement_line_id").text_or({});
    value.invoice_id = row.get("invoice_id").text_or({});
    value.invoice_line_id = row.get("invoice_line_id").text_or({});
    value.quantity_scaled = row.get("quantity_scaled").integer_or(0);
    const std::int64_t state = row.get("state").integer_or(-1);
    value.state = state == 0 ? ConsumptionState::Active
                            : state == 1 ? ConsumptionState::Released
                                         : static_cast<ConsumptionState>(255);
    value.consumed_at = row.get("consumed_at").integer_or(0);
    value.consumed_by = row.get("consumed_by").text_or({});
    value.released_at = row.get("released_at").integer_or(0);
    value.released_by = row.get("released_by").text_or({});
    value.release_reason = row.get("release_reason").text_or({});
    return value;
}

}  // namespace squiflow::modules::agreements
