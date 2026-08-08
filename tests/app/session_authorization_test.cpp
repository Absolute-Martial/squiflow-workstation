#include "app/session_authorization.hpp"

#include "support/check.hpp"

#include <stdexcept>
#include <utility>

namespace {
using namespace squiflow;

app::RequestContext context(engine::RightsSet rights, std::uint64_t generation,
                           engine::PersonId user) {
    auto created = app::RequestContext::create(app::TenantId{{1, 2}}, user, std::move(rights),
                                               "session-authorization-test", generation);
    if (!created) {
        throw std::logic_error("context creation failed");
    }
    return std::move(created).value();
}

engine::Session live_session(engine::PersonId person, engine::RightsSet rights) {
    engine::Session session;
    session.person = person;
    session.device = {9, 9};
    session.display_name = "Owner";
    session.is_owner = true;
    session.rights = std::move(rights);
    return session;
}

}  // namespace

int main() {
    namespace t = squiflow::testing;
    using namespace squiflow;

    const engine::PersonId owner_id{3, 4};
    const engine::PersonId other_id{5, 6};

    engine::RightsSet rights;
    rights.grant(protocol::RightId::right_party_read);

    t::section("a session that is not signed in refuses every context");
    {
        engine::Session empty;
        auto refused = app::authorize_session(context(rights, 7, owner_id), empty, 0);
        t::check(!refused && refused.error().code == app::DomainErrorCode::Unauthorized,
                 "an empty live session with generation zero is unauthorized");
    }

    t::section("a live session with generation zero is refused even if signed in");
    {
        auto refused = app::authorize_session(context(rights, 7, owner_id),
                                              live_session(owner_id, rights), 0);
        t::check(!refused && refused.error().code == app::DomainErrorCode::Unauthorized,
                 "generation zero is always unauthorized");
    }

    t::section("a stale generation, a different user, or an escalated right are refused");
    {
        const engine::Session live = live_session(owner_id, rights);
        auto stale = app::authorize_session(context(rights, 6, owner_id), live, 7);
        t::check(!stale && stale.error().code == app::DomainErrorCode::Unauthorized,
                 "a context captured under an earlier generation is unauthorized");

        auto wrong_user = app::authorize_session(context(rights, 7, other_id), live, 7);
        t::check(!wrong_user && wrong_user.error().code == app::DomainErrorCode::Unauthorized,
                 "a context built for a different user is unauthorized");

        engine::RightsSet elevated = rights;
        elevated.grant(protocol::RightId::right_party_write);
        auto escalated = app::authorize_session(context(elevated, 7, owner_id), live, 7);
        t::check(!escalated && escalated.error().code == app::DomainErrorCode::Unauthorized,
                 "a context claiming a right the live session does not have is unauthorized");
    }

    t::section("a context that matches the live session exactly is authorized");
    {
        const engine::Session live = live_session(owner_id, rights);
        auto ok = app::authorize_session(context(rights, 7, owner_id), live, 7);
        t::check(bool(ok), "matching session, generation, user and rights succeeds");

        auto fewer_rights = app::authorize_session(context({}, 7, owner_id), live, 7);
        t::check(bool(fewer_rights),
                 "a context that claims fewer rights than the live session still succeeds");
    }

    return t::report();
}
