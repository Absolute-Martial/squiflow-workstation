#pragma once

#include "app/contracts/domain_error.hpp"
#include "app/contracts/result.hpp"
#include "engine/identity/rights_set.hpp"
#include "engine/records/identity.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace squiflow::app {

struct TenantId final {
    engine::RecordId value{};
    bool is_valid() const noexcept { return value.is_valid(); }
    friend bool operator==(const TenantId&, const TenantId&) = default;
};

class RequestContext final {
  public:
    static constexpr std::size_t kMaxCorrelationId = 128;

    static Result<RequestContext, DomainError> create(
        TenantId tenant,
        engine::PersonId user,
        engine::RightsSet permissions,
        std::string correlation_id,
        std::uint64_t session_generation) {
        if (!tenant.is_valid()) {
            return invalid("request_context.tenant_required", "tenant_id");
        }
        if (!user.is_valid()) {
            return invalid("request_context.user_required", "user_id");
        }
        if (correlation_id.empty() || correlation_id.size() > kMaxCorrelationId) {
            return invalid("request_context.correlation_invalid", "correlation_id");
        }
        for (const char value : correlation_id) {
            const bool alpha = (value >= 'a' && value <= 'z') ||
                               (value >= 'A' && value <= 'Z');
            const bool digit = value >= '0' && value <= '9';
            const bool punctuation = value == '-' || value == '_' ||
                                     value == '.' || value == ':';
            if (!alpha && !digit && !punctuation) {
                return invalid("request_context.correlation_invalid", "correlation_id");
            }
        }
        if (session_generation == 0) {
            return invalid("request_context.session_generation_required",
                           "session_generation");
        }
        return Result<RequestContext, DomainError>::success(RequestContext(
            tenant, user, std::move(permissions), std::move(correlation_id),
            session_generation));
    }

    const TenantId& tenant_id() const noexcept { return tenant_; }
    const engine::PersonId& user_id() const noexcept { return user_; }
    const engine::RightsSet& permissions() const noexcept { return permissions_; }
    std::string_view correlation_id() const noexcept { return correlation_id_; }
    std::uint64_t session_generation() const noexcept { return session_generation_; }

  private:
    RequestContext(TenantId tenant, engine::PersonId user,
                   engine::RightsSet permissions, std::string correlation_id,
                   std::uint64_t session_generation)
        : tenant_(tenant), user_(user), permissions_(std::move(permissions)),
          correlation_id_(std::move(correlation_id)),
          session_generation_(session_generation) {}

    static Result<RequestContext, DomainError> invalid(
        std::string message_key, std::string field) {
        return Result<RequestContext, DomainError>::failure(DomainError{
            DomainErrorCode::InvalidContext, std::move(message_key),
            std::move(field)});
    }

    TenantId tenant_;
    engine::PersonId user_;
    engine::RightsSet permissions_;
    std::string correlation_id_;
    std::uint64_t session_generation_;
};

}  // namespace squiflow::app
