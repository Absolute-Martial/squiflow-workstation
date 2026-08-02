#include "modules/receivables/domain/document_number_block.hpp"

#include <algorithm>
#include <limits>

#include "engine/records/identity.hpp"
#include "modules/context.hpp"

namespace squiflow::modules::receivables {
namespace {

bool blank(const std::string& text) noexcept {
    return std::all_of(text.begin(), text.end(), [](const char c) {
        return static_cast<unsigned char>(c) <= static_cast<unsigned char>(' ');
    });
}

bool storable(const std::uint64_t number) noexcept {
    return number > 0 &&
           number <= static_cast<std::uint64_t>(
                         std::numeric_limits<std::int64_t>::max());
}

NumberedDocumentKind kind_from(const engine::Row& row) noexcept {
    const std::int64_t stored = row.get("document_kind").integer_or(-1);
    return stored == 0 ? NumberedDocumentKind::Invoice
                       : static_cast<NumberedDocumentKind>(255);
}

std::uint64_t number_from(const engine::Row& row, const char* field) noexcept {
    const std::int64_t stored = row.get(field).integer_or(0);
    return stored > 0 ? static_cast<std::uint64_t>(stored) : 0;
}

}  // namespace

void validate(const DocumentNumberBlock& block) {
    if (blank(block.id)) {
        throw RuleViolation("A reserved number block must have an identity.");
    }
    if (block.kind != NumberedDocumentKind::Invoice) {
        throw RuleViolation("That reserved number block has an unknown document kind.");
    }
    if (blank(block.series)) {
        throw RuleViolation("A reserved number block must name its visible series.");
    }
    if (!engine::record_id_from_string(block.device_id).is_valid()) {
        throw RuleViolation("A reserved number block must belong to a valid device.");
    }
    if (!storable(block.first_number) || !storable(block.next_number) ||
        !storable(block.last_number) || block.first_number > block.next_number ||
        block.next_number > block.last_number) {
        throw RuleViolation("A reserved number block has an unusable numeric range.");
    }
    if (block.exhausted && block.next_number != block.last_number) {
        throw RuleViolation("An exhausted number block contradicts its next number.");
    }
    if (block.assigned_at <= 0 || blank(block.assignment_reference)) {
        throw RuleViolation("A reserved number block must retain its assignment evidence.");
    }
}

std::optional<std::uint64_t> allocate(DocumentNumberBlock& block) noexcept {
    try {
        validate(block);
    } catch (...) {
        return std::nullopt;
    }
    if (block.exhausted) return std::nullopt;

    const std::uint64_t allocated = block.next_number;
    if (block.next_number == block.last_number) {
        block.exhausted = true;
    } else {
        ++block.next_number;
    }
    return allocated;
}

bool overlaps(const DocumentNumberBlock& left,
              const DocumentNumberBlock& right) noexcept {
    return left.kind == right.kind && left.series == right.series &&
           left.first_number <= right.last_number &&
           right.first_number <= left.last_number;
}

engine::Row to_row(const DocumentNumberBlock& block) {
    engine::Row row;
    row.set("id", engine::Value::text(block.id));
    row.set("document_kind",
            engine::Value::integer(static_cast<std::int64_t>(block.kind)));
    row.set("series", engine::Value::text(block.series));
    row.set("device_id", engine::Value::text(block.device_id));
    row.set("first_number",
            engine::Value::integer(static_cast<std::int64_t>(block.first_number)));
    row.set("next_number",
            engine::Value::integer(static_cast<std::int64_t>(block.next_number)));
    row.set("last_number",
            engine::Value::integer(static_cast<std::int64_t>(block.last_number)));
    row.set("exhausted", engine::Value::boolean(block.exhausted));
    row.set("assigned_at", engine::Value::integer(block.assigned_at));
    row.set("assignment_reference",
            engine::Value::text(block.assignment_reference));
    return row;
}

DocumentNumberBlock document_number_block_from_row(const engine::Row& row) {
    DocumentNumberBlock block;
    block.id = row.get("id").text_or({});
    block.kind = kind_from(row);
    block.series = row.get("series").text_or({});
    block.device_id = row.get("device_id").text_or({});
    block.first_number = number_from(row, "first_number");
    block.next_number = number_from(row, "next_number");
    block.last_number = number_from(row, "last_number");
    block.exhausted = row.get("exhausted").boolean_or(false);
    block.assigned_at = row.get("assigned_at").integer_or(0);
    block.assignment_reference = row.get("assignment_reference").text_or({});
    return block;
}

}  // namespace squiflow::modules::receivables
