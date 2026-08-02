#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "engine/records/identity.hpp"
#include "engine/records/reference.hpp"
#include "engine/records/signature.hpp"

namespace squiflow::engine {

// An approval is a mechanism, not a module.
//
// It was a module in an earlier draft and had to be demoted: it owns no
// business entity of its own, and every module needing an approval would have
// had to depend on it, which breaks the one-way dependency rule everywhere at
// once. An approval belongs to the thing being approved.
enum class ApprovalState : std::uint8_t {
    Pending,
    Approved,
    Rejected,
    Withdrawn,
};

std::string_view to_string(ApprovalState state) noexcept;

struct Approval {
    RecordId id;
    Reference subject;
    ApprovalState state = ApprovalState::Pending;
    PersonId decided_by;
    Timestamp decided_at;
    std::string note;
    std::optional<Signature> signature;

    // A remote backend may deliver the request by email. Transport acceptance
    // never means approval: a valid signed or explicitly recorded customer
    // decision is still required.
    bool sent_for_approval = false;
    Timestamp sent_at;
};

struct ApprovalCheck {
    bool complete = false;
    std::string_view missing;
};

// Whether an approval is finished. When a signature is required, an approval
// without one is not an approval, however many notes it carries.
ApprovalCheck approval_complete(const Approval& approval,
                                bool signature_required) noexcept;

}  // namespace squiflow::engine
