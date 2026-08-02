#include "engine/records/approval.hpp"

#include "engine/records/snapshot.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace squiflow::engine {

std::string_view to_string(ApprovalState state) noexcept {
    switch (state) {
        case ApprovalState::Pending:
            return "pending";
        case ApprovalState::Approved:
            return "approved";
        case ApprovalState::Rejected:
            return "rejected";
        case ApprovalState::Withdrawn:
            return "withdrawn";
    }
    return "?";
}

std::string_view to_string(RateOrigin origin) noexcept {
    switch (origin) {
        case RateOrigin::CatalogDefault:
            return "catalog rate";
        case RateOrigin::PartySpecific:
            return "rate for this customer";
        case RateOrigin::Agreement:
            return "agreement rate";
        case RateOrigin::ManualOverride:
            return "changed by hand";
        case RateOrigin::OffCatalog:
            return "one-off line";
    }
    return "?";
}

bool signature_format_allowed(std::string_view extension) noexcept {
    // Lossy formats destroy thin strokes, which is all a signature is.
    static constexpr std::array<std::string_view, 5> kForbidden = {
        "jpg", "jpeg", "avif", "webp", "heic"};

    std::string lowered;
    lowered.reserve(extension.size());
    for (const char c : extension) {
        if (c == '.') {
            continue;
        }
        lowered.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(c))));
    }

    return std::find(kForbidden.begin(), kForbidden.end(), lowered) ==
           kForbidden.end();
}

ApprovalCheck approval_complete(const Approval& approval,
                                bool signature_required) noexcept {
    if (approval.state == ApprovalState::Pending) {
        return {false, "nobody has decided yet"};
    }
    if (approval.state != ApprovalState::Approved) {
        return {false, "this was not approved"};
    }
    if (!approval.decided_by.is_valid()) {
        return {false, "no person is recorded as deciding"};
    }
    if (!approval.decided_at.is_set()) {
        return {false, "no time is recorded"};
    }
    if (signature_required && !approval.signature.has_value()) {
        return {false, "a signature is required and none was captured"};
    }
    return {true, {}};
}

}  // namespace squiflow::engine
