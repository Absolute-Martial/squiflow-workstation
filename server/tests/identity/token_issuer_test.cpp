#include "identity/token_issuer.hpp"
#include "identity/token_store.hpp"

#include "support/check.hpp"

#include <chrono>
#include <thread>
#include <vector>

namespace t = squiflow::testing;
using squiflow::engine::DeviceId;
using squiflow::engine::PersonId;
using squiflow::engine::Timestamp;
using namespace squiflow::server::identity;

namespace {

PersonId make_person(std::uint64_t value) { return PersonId{0, value}; }
DeviceId make_device(std::uint64_t value) { return DeviceId{0, value}; }

// A clock the test controls exactly, so expiry can be tested without a real
// sleep.
class FakeClock {
public:
    explicit FakeClock(std::int64_t start_ms) : now_ms_(start_ms) {}
    Timestamp operator()() const { return Timestamp{now_ms_}; }
    void advance(std::chrono::seconds by) { now_ms_ += by.count() * 1000; }

private:
    std::int64_t now_ms_;
};

}  // namespace

int main() {
    t::section("normal issue, validate, and revoke cycle");
    {
        InMemoryTokenStore store;
        FakeClock clock(1'000'000);
        TokenIssuer issuer(store, std::chrono::seconds(3600), std::ref(clock));

        const auto person = make_person(1);
        const auto device = make_device(2);
        const auto issued = issuer.issue(person, device);
        t::check(!issued.bearer.empty(), "issuing returns a non-empty bearer string");
        t::check(issued.bearer.find('.') != std::string::npos,
                 "bearer string contains the id/secret separator");
        t::check(issued.bearer.find(squiflow::engine::to_string(person)) == std::string::npos,
                 "bearer string never contains the person id's own encoding");

        const auto valid = issuer.validate(issued.bearer);
        t::check(valid.ok, "a freshly issued token validates");
        t::check(valid.person == person, "validation returns the issuing person");
        t::check(valid.device == device, "validation returns the issuing device");

        issuer.revoke(issued.id);
        const auto after_revoke = issuer.validate(issued.bearer);
        t::check(!after_revoke.ok, "a revoked token no longer validates");
        t::check(after_revoke.fault == TokenFault::Revoked,
                 "a revoked token reports the revoked fault");
    }

    t::section("expired token rejected");
    {
        InMemoryTokenStore store;
        FakeClock clock(0);
        TokenIssuer issuer(store, std::chrono::seconds(60), std::ref(clock));
        const auto issued = issuer.issue(make_person(1), make_device(1));

        clock.advance(std::chrono::seconds(59));
        t::check(issuer.validate(issued.bearer).ok, "still valid one second before expiry");

        clock.advance(std::chrono::seconds(2));
        const auto expired = issuer.validate(issued.bearer);
        t::check(!expired.ok, "a token past its ttl is rejected");
        t::check(expired.fault == TokenFault::Expired, "expiry reports the expired fault");
    }

    t::section("revoked token rejected immediately, no staleness window");
    {
        InMemoryTokenStore store;
        TokenIssuer issuer(store, std::chrono::seconds(3600));
        const auto issued = issuer.issue(make_person(5), make_device(5));
        t::check(issuer.validate(issued.bearer).ok, "valid before revocation");
        issuer.revoke(issued.id);
        t::check(!issuer.validate(issued.bearer).ok,
                 "invalid on the very next validate call after revocation");
    }

    t::section("malformed and unknown tokens rejected without leaking why");
    {
        InMemoryTokenStore store;
        TokenIssuer issuer(store, std::chrono::seconds(3600));
        const auto issued = issuer.issue(make_person(1), make_device(1));

        const auto no_separator = issuer.validate("not-a-real-token");
        t::check(!no_separator.ok && no_separator.fault == TokenFault::Malformed,
                 "a token with no separator is malformed");

        const auto wrong_lengths = issuer.validate("abc.def");
        t::check(!wrong_lengths.ok && wrong_lengths.fault == TokenFault::Malformed,
                 "a token with the wrong id/secret lengths is malformed");

        const auto non_hex = issuer.validate(
            std::string(issued.id.value) + "." + std::string(64, 'z'));
        t::check(!non_hex.ok && non_hex.fault == TokenFault::Malformed,
                 "a non-hex secret of the right length is malformed");

        const auto unknown_id =
            issuer.validate(std::string(32, 'a') + "." + std::string(64, 'b'));
        t::check(!unknown_id.ok && unknown_id.fault == TokenFault::NotFound,
                 "a well-formed but unregistered id is not found");

        const auto wrong_secret =
            issuer.validate(issued.id.value + "." + std::string(64, 'a'));
        t::check(!wrong_secret.ok && wrong_secret.fault == TokenFault::NotFound,
                 "a wrong secret for a real id reports the same fault as unknown, not a hint");
    }

    t::section("concurrent revocation and validation of the same token");
    {
        InMemoryTokenStore store;
        TokenIssuer issuer(store, std::chrono::seconds(3600));
        const auto issued = issuer.issue(make_person(9), make_device(9));

        std::vector<std::thread> workers;
        for (int i = 0; i < 8; ++i) {
            workers.emplace_back([&issuer, &issued] {
                for (int j = 0; j < 200; ++j) {
                    (void)issuer.validate(issued.bearer);
                }
            });
        }
        workers.emplace_back([&issuer, &issued] { issuer.revoke(issued.id); });
        for (auto& worker : workers) {
            worker.join();
        }
        t::check(!issuer.validate(issued.bearer).ok,
                 "the token is revoked after concurrent access settles, without a crash");
    }

    t::section("device re-registration issues a new token, never un-revokes the old one");
    {
        InMemoryTokenStore store;
        TokenIssuer issuer(store, std::chrono::seconds(3600));
        const auto device = make_device(3);
        const auto first = issuer.issue(make_person(1), device);

        // Simulate re-registration: the old token is explicitly revoked and a
        // new one is issued for the same person/device.
        issuer.revoke(first.id);
        const auto second = issuer.issue(make_person(1), device);

        t::check(first.id.value != second.id.value,
                 "re-registration produces a distinct token id");
        t::check(!issuer.validate(first.bearer).ok,
                 "the old token stays revoked after re-registration");
        t::check(issuer.validate(second.bearer).ok, "the new token validates independently");
    }

    return t::report();
}
