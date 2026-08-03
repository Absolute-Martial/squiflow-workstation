#pragma once

#include <optional>
#include <string>

namespace squiflow::app {

enum class DomainErrorCode {
    NotFound,
    Unauthorized,
    ValidationFailed,
    Conflict,
    Offline,
    Timeout,
    Cancelled,
    InvalidContext,
};

struct DomainError final {
    DomainErrorCode code{DomainErrorCode::ValidationFailed};
    std::string message_key{};
    std::optional<std::string> field{};

    friend bool operator==(const DomainError&, const DomainError&) = default;
};

constexpr bool is_retryable(DomainErrorCode code) noexcept {
    return code == DomainErrorCode::Offline || code == DomainErrorCode::Timeout;
}

}  // namespace squiflow::app
