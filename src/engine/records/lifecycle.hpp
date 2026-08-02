#pragma once

#include <cstdint>
#include <string_view>

namespace squiflow::engine {

// One lifecycle, shared by quotations, invoices and agreements, so those three
// cannot drift into three different ideas of what "cancelled" means.
enum class DocumentState : std::uint8_t {
    Draft,      // freely editable, no number taken, invisible to the customer
    Issued,     // an explicit human act; frozen from here on
    Cancelled,  // the number is burned and never reused
    Replaced,   // cancelled and superseded by a linked reissue
    Discarded,  // a draft thrown away; it never became anything
};

std::string_view to_string(DocumentState state) noexcept;

// The only permitted moves. Everything else is a bug, and this is the single
// place to look when arguing about whether something should be possible.
//
//   Draft     -> Issued | Discarded
//   Issued    -> Cancelled
//   Cancelled -> Replaced
//
// There is deliberately no path back to Draft. An issued document is evidence
// the customer may already be holding.
bool transition_allowed(DocumentState from, DocumentState to) noexcept;

// Editable only while a draft. This is what makes "correction means cancel and
// reissue" true rather than merely intended.
constexpr bool is_editable(DocumentState state) noexcept {
    return state == DocumentState::Draft;
}

// Terminal states hold still forever, so a screen can stop offering actions.
constexpr bool is_final(DocumentState state) noexcept {
    return state == DocumentState::Replaced || state == DocumentState::Discarded;
}

// Did this document ever take a number? Drafts and discarded drafts never do,
// which is why the numbering sequence has no gaps from abandoned work.
constexpr bool has_number(DocumentState state) noexcept {
    return state != DocumentState::Draft && state != DocumentState::Discarded;
}

struct TransitionResult {
    bool ok = false;
    DocumentState state = DocumentState::Draft;
    std::string_view refusal;
};

TransitionResult apply_transition(DocumentState from, DocumentState to) noexcept;

}  // namespace squiflow::engine
