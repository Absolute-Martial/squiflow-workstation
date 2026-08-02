#pragma once

// Server-reserved final-number ranges retained on each workstation device.
//
// A caller may choose a series, but never a final number.  Issuance consumes
// one number from a block assigned to the current device in the same database
// transaction as the document it numbers.  Exhausted blocks are permanent
// evidence: they are never reset or overwritten with a new range.

#include <cstdint>
#include <optional>
#include <string>

#include "engine/storage/store.hpp"

namespace squiflow::modules::receivables {

enum class NumberedDocumentKind : std::uint8_t {
    Invoice = 0
};

struct DocumentNumberBlock {
    std::string id{};
    NumberedDocumentKind kind{NumberedDocumentKind::Invoice};
    std::string series{};
    std::string device_id{};

    std::uint64_t first_number{0};
    std::uint64_t next_number{0};
    std::uint64_t last_number{0};
    bool exhausted{false};

    std::int64_t assigned_at{0};
    std::string assignment_reference{};
};

void validate(const DocumentNumberBlock& block);

// Advances the retained block and returns the number consumed.  No state is
// changed when the block is exhausted or malformed.
std::optional<std::uint64_t> allocate(DocumentNumberBlock& block) noexcept;

bool overlaps(const DocumentNumberBlock& left,
              const DocumentNumberBlock& right) noexcept;

engine::Row to_row(const DocumentNumberBlock& block);
DocumentNumberBlock document_number_block_from_row(const engine::Row& row);

}  // namespace squiflow::modules::receivables
