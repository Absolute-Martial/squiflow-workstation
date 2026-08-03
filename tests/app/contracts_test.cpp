#include "app/contracts/request_context.hpp"
#include "app/contracts/result.hpp"
#include "support/check.hpp"

#include <memory>
#include <stdexcept>
#include <string>

using squiflow::app::DomainErrorCode;
using squiflow::app::RequestContext;
using squiflow::app::Result;
using squiflow::app::TenantId;

int main() {
    namespace t = squiflow::testing;
    t::section("explicit result boundary");
    auto value = Result<int, std::string>::success(42);
    t::check(value.has_value() && value.value() == 42,
             "success stores its value");
    auto failure = Result<int, std::string>::failure("not-found");
    t::check(!failure && failure.error() == "not-found",
             "failure stores its error");
    auto same_types = Result<std::string, std::string>::failure("failure");
    t::check(!same_types && same_types.error() == "failure",
             "equal value and error types stay distinct");
    auto move_only = Result<std::unique_ptr<int>, std::string>::success(
        std::make_unique<int>(7));
    t::check(**move_only.value_if() == 7, "move-only values are supported");
    auto no_value = Result<void, std::string>::success();
    t::check(no_value.has_value(), "void success is explicit");
    bool misuse_threw = false;
    try { (void)failure.value(); } catch (const std::logic_error&) {
        misuse_threw = true;
    }
    t::check(misuse_threw, "unchecked result misuse is a programming error");

    t::section("immutable request context");
    squiflow::engine::RightsSet rights;
    const auto right = static_cast<squiflow::protocol::RightId>(0);
    rights.grant(right);
    const TenantId tenant{{1, 2}};
    const squiflow::engine::PersonId user{3, 4};
    std::string correlation(128, 'a');
    auto context = RequestContext::create(tenant, user, rights, correlation, 9);
    t::check(context.has_value(), "maximum correlation boundary is accepted");
    correlation[0] = 'z';
    rights.revoke(right);
    t::check(context.value().correlation_id().front() == 'a',
             "context owns correlation storage");
    t::check(context.value().permissions().has(right),
             "context owns an immutable permission snapshot");
    t::check(context.value().session_generation() == 9,
             "session generation is retained");

    t::section("malformed context is rejected");
    const auto invalid_tenant = RequestContext::create(
        TenantId{}, user, rights, "request-1", 1);
    t::check(!invalid_tenant &&
             invalid_tenant.error().code == DomainErrorCode::InvalidContext,
             "invalid tenant is rejected");
    t::check(!RequestContext::create(tenant, {}, rights, "request-1", 1),
             "invalid user is rejected");
    t::check(!RequestContext::create(tenant, user, rights, "", 1),
             "empty correlation is rejected");
    t::check(!RequestContext::create(
                 tenant, user, rights, std::string(129, 'a'), 1),
             "oversized correlation is rejected");
    t::check(!RequestContext::create(tenant, user, rights, "bad value", 1),
             "unsafe correlation characters are rejected");
    t::check(!RequestContext::create(tenant, user, rights, "request-1", 0),
             "zero session generation is rejected");

    return t::report();
}
