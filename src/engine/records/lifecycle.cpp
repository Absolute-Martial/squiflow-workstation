#include "engine/records/lifecycle.hpp"

namespace squiflow::engine {

std::string_view to_string(DocumentState state) noexcept {
    switch (state) {
        case DocumentState::Draft:
            return "draft";
        case DocumentState::Issued:
            return "issued";
        case DocumentState::Cancelled:
            return "cancelled";
        case DocumentState::Replaced:
            return "replaced";
        case DocumentState::Discarded:
            return "discarded";
    }
    return "?";
}

bool transition_allowed(DocumentState from, DocumentState to) noexcept {
    switch (from) {
        case DocumentState::Draft:
            return to == DocumentState::Issued || to == DocumentState::Discarded;
        case DocumentState::Issued:
            return to == DocumentState::Cancelled;
        case DocumentState::Cancelled:
            return to == DocumentState::Replaced;
        case DocumentState::Replaced:
        case DocumentState::Discarded:
            return false;
    }
    return false;
}

TransitionResult apply_transition(DocumentState from, DocumentState to) noexcept {
    if (from == to) {
        return {false, from, "already in that state"};
    }
    if (!transition_allowed(from, to)) {
        if (from == DocumentState::Issued && to == DocumentState::Draft) {
            return {false, from,
                    "an issued document cannot be edited; cancel and reissue"};
        }
        if (is_final(from)) {
            return {false, from, "this document is final"};
        }
        return {false, from, "that change is not allowed"};
    }
    return {true, to, {}};
}

}  // namespace squiflow::engine
