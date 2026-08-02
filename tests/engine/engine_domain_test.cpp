// The domain half of the engine. No database, no Qt, no platform calls, so it
// compiles and runs anywhere.
//
// Everything checked here is something that would be a wrong number on a piece
// of paper handed to a customer.

#include "engine/identity/capability.hpp"
#include "engine/identity/rights_set.hpp"
#include "engine/records/approval.hpp"
#include "engine/records/identity.hpp"
#include "engine/records/lifecycle.hpp"
#include "engine/records/money.hpp"
#include "engine/records/numbering.hpp"
#include "engine/records/quantity.hpp"
#include "support/check.hpp"

#include <limits>
#include <string>
#include <string_view>

using namespace squiflow::engine;
using squiflow::testing::check;
using squiflow::testing::section;
namespace protocol = squiflow::protocol;

namespace {

RecordId make_id(std::uint64_t low) {
    return RecordId{1, low};
}

void test_identity() {
    section("record identity");

    const RecordId id{0x0123456789abcdefULL, 0xfedcba9876543210ULL};
    const std::string text = to_string(id);
    check(text == "0123456789abcdeffedcba9876543210", "identifier renders as hex");
    check(record_id_from_string(text) == id, "and parses back to the same value");

    check(!record_id_from_string("too short").is_valid(),
          "a malformed identifier is invalid, not a guess");
    check(!record_id_from_string("zzzz456789abcdeffedcba9876543210").is_valid(),
          "non-hex characters are rejected");
    check(!RecordId{}.is_valid(), "a zero identifier is never valid");
}

void test_quantity() {
    section("quantity");

    check(parse_quantity("5").value == Quantity::from_whole(5), "whole numbers");
    check(parse_quantity("2.5").value.scaled == 2500, "one decimal");
    check(parse_quantity("0.125").value.scaled == 125, "three decimals");
    check(parse_quantity("-3.25").value.scaled == -3250, "negatives");

    check(!parse_quantity("1.2345").ok,
          "four decimals is refused rather than rounded behind the person");
    check(!parse_quantity("").ok, "empty is refused");
    check(!parse_quantity("abc").ok, "nonsense is refused");

    check(format(Quantity::from_whole(5)) == "5", "trailing zeros are trimmed");
    check(format(Quantity{2500}) == "2.5", "one decimal prints as one");
    check(format(Quantity{125}) == "0.125", "three decimals survive");
    check(format(Quantity{-3250}) == "-3.25", "negatives print");

    const QuantityResult sum =
        quantity_add(Quantity{1500}, Quantity{2500});
    check(sum.ok && sum.value.scaled == 4000, "addition");

    const QuantityResult overflow = quantity_add(
        Quantity{std::numeric_limits<std::int64_t>::max()}, Quantity{1});
    check(!overflow.ok, "overflow is reported, never wrapped");
}

void test_money() {
    section("money");

    check(parse_money("1200").value.minor == 120000, "whole amounts");
    check(parse_money("1200.50").value.minor == 120050, "two decimals");
    check(parse_money("1,200.50").value.minor == 120050, "grouped input is accepted");
    check(!parse_money("10.005").ok, "three decimals is refused");

    check(format(Money{120050}) == "1,200.50", "formatting groups in threes");
    check(format(Money{5}) == "0.05", "small amounts keep both decimals");
    check(format(Money{-120050}) == "-1,200.50", "negatives");

    // 12.5 square feet at 40.00 is exactly 500.00.
    const MoneyResult exact =
        money_multiply(Money::from_units(40), Quantity{12500});
    check(exact.ok && exact.value.minor == 50000, "rate x quantity, exact");

    // 3 at 33.33 is 99.99, not 100.
    const MoneyResult pennies =
        money_multiply(Money{3333}, Quantity::from_whole(3));
    check(pennies.ok && pennies.value.minor == 9999, "no drift on repeated cents");

    // 0.5 of 0.05 is 0.025, which rounds away from zero to 0.03 the way a
    // person doing it by hand would.
    const MoneyResult rounded = money_multiply(Money{5}, Quantity{500});
    check(rounded.ok && rounded.value.minor == 3, "half rounds away from zero");

    const MoneyResult negative_rounded =
        money_multiply(Money{-5}, Quantity{500});
    check(negative_rounded.ok && negative_rounded.value.minor == -3,
          "and does the same on the negative side");

    const MoneyResult big = money_multiply(
        Money{std::numeric_limits<std::int64_t>::max()}, Quantity::from_whole(2));
    check(!big.ok, "overflow is reported, never wrapped into a wrong invoice");

    // A hundred lines of an awkward amount must still total exactly.
    Money running{0};
    bool ok = true;
    for (int i = 0; i < 100; ++i) {
        const MoneyResult step = money_add(running, Money{3333});
        ok = ok && step.ok;
        running = step.value;
    }
    check(ok && running.minor == 333300, "a hundred lines add up exactly");
}

void test_lifecycle() {
    section("document lifecycle");

    check(is_editable(DocumentState::Draft), "a draft is editable");
    check(!is_editable(DocumentState::Issued), "an issued document is not");

    check(transition_allowed(DocumentState::Draft, DocumentState::Issued),
          "draft to issued");
    check(transition_allowed(DocumentState::Draft, DocumentState::Discarded),
          "draft to discarded");
    check(transition_allowed(DocumentState::Issued, DocumentState::Cancelled),
          "issued to cancelled");
    check(transition_allowed(DocumentState::Cancelled, DocumentState::Replaced),
          "cancelled to replaced");

    check(!transition_allowed(DocumentState::Issued, DocumentState::Draft),
          "an issued document can never go back to draft");
    check(!transition_allowed(DocumentState::Cancelled, DocumentState::Issued),
          "a cancelled document is never re-issued in place");
    check(!transition_allowed(DocumentState::Discarded, DocumentState::Issued),
          "a discarded draft stays discarded");

    const TransitionResult refusal =
        apply_transition(DocumentState::Issued, DocumentState::Draft);
    check(!refusal.ok, "the refusal is reported");
    check(refusal.refusal.find("cancel and reissue") != std::string_view::npos,
          "and it tells the person what to do instead");

    check(!has_number(DocumentState::Draft), "drafts hold no number");
    check(!has_number(DocumentState::Discarded),
          "a discarded draft never took one, so it leaves no gap");
    check(has_number(DocumentState::Cancelled),
          "a cancelled document keeps its number; the number is burned");
}

void test_numbering() {
    section("numbering");

    NumberBlock block(1, 3);
    check(block.remaining() == 3, "a fresh block holds its whole range");
    check(block.allocate().value() == 1, "first");
    check(block.allocate().value() == 2, "second");
    check(block.allocate().value() == 3, "third");
    check(!block.allocate().has_value(),
          "an exhausted block refuses rather than inventing a number");
    check(block.exhausted(), "and says so");

    NumberBlock owner_block(1, 500);
    NumberBlock staff_block(501, 1000);
    check(owner_block.allocate().value() == 1, "one device starts at 1");
    check(staff_block.allocate().value() == 501,
          "the other starts at 501, so gaps in the sequence are normal");

    NumberBlock nearly(1, 10);
    for (int i = 0; i < 8; ++i) {
        (void)nearly.allocate();
    }
    check(nearly.low(5), "a block running low is detectable before it runs out");

    check(format_number("INV", 42, 6) == "INV-000042", "numbers are padded");
    check(format_number("", 42, 4) == "0042", "an empty series adds no dash");
}

void test_signatures_and_approvals() {
    section("signatures and approvals");

    check(signature_format_allowed("png"), "lossless formats are allowed");
    check(signature_format_allowed(".svg"), "a leading dot is tolerated");
    check(!signature_format_allowed("jpg"), "lossy formats are refused");
    check(!signature_format_allowed("AVIF"),
          "including the one bill photos use, and regardless of case");

    Approval approval;
    approval.subject = {protocol::ModuleId::agreements, make_id(7)};

    check(!approval_complete(approval, false).complete,
          "a pending approval is not an approval");

    approval.state = ApprovalState::Approved;
    approval.decided_by = make_id(1);
    approval.decided_at = Timestamp{1000};
    check(approval_complete(approval, false).complete,
          "decided, by someone, at a time");

    check(!approval_complete(approval, true).complete,
          "but not when a signature is required and missing");

    Signature signature;
    signature.id = make_id(9);
    signature.stroke_count = 12;
    approval.signature = signature;
    check(approval_complete(approval, true).complete, "with the signature, it is");

    approval.state = ApprovalState::Rejected;
    check(!approval_complete(approval, true).complete,
          "a rejection is never complete, whatever else is filled in");
}

void test_rights() {
    section("rights");

    RightsSet rights;
    check(rights.empty(), "a new person has nothing");

    rights.grant(protocol::RightId::right_order_write);
    check(rights.has(protocol::RightId::right_order_write), "granting works");
    check(!rights.has(protocol::RightId::right_invoice_cancel),
          "and grants nothing else");

    rights.revoke(protocol::RightId::right_order_write);
    check(!rights.has(protocol::RightId::right_order_write), "revoking works");

    rights.grant(protocol::RightId::right_party_read);
    rights.grant(protocol::RightId::right_party_write);
    rights.grant(protocol::RightId::right_job_write);
    check(rights.granted_in(protocol::ModuleId::parties).size() == 2,
          "rights can be listed per module");
    check(rights.count() == 3, "and counted");
}

Session owner_session() {
    Session session;
    session.person = make_id(1);
    session.device = make_id(100);
    session.display_name = "owner";
    session.is_owner = true;
    session.rights.grant_all();
    return session;
}

Session staff_session() {
    Session session;
    session.person = make_id(2);
    session.device = make_id(200);
    session.display_name = "staff";
    session.is_owner = false;
    session.rights.grant_all();  // rights are not what is being tested here
    return session;
}

void test_capability() {
    section("capability");

    const protocol::Activation all = protocol::resolve_activation({}).activation;

    const Session owner = owner_session();
    const Session staff = staff_session();

    check(may_run(protocol::OperationId::counter_sale, owner,
                  ConnectionState::Online, all)
              .allowed,
          "the owner can take a counter sale online");

    check(may_run(protocol::OperationId::counter_sale, staff,
                  ConnectionState::Offline, all)
              .allowed,
          "and staff can take one with the connection down: the counter never "
          "stops");

    check(may_run(protocol::OperationId::document_print, staff,
                  ConnectionState::Offline, all)
              .allowed,
          "staff can print offline");

    {
        const Decision decision = may_run(protocol::OperationId::party_create,
                                          staff, ConnectionState::Offline, all);
        check(!decision.allowed, "but staff cannot change records offline");
        check(decision.reason == DenialReason::ReadOnlyOffline,
              "and the reason says exactly that");
    }

    check(may_run(protocol::OperationId::party_create, owner,
                  ConnectionState::Offline, all)
              .allowed,
          "the owner still can");

    {
        const Decision decision = may_run(protocol::OperationId::right_grant,
                                          owner, ConnectionState::Offline, all);
        check(!decision.allowed,
              "nobody grants rights offline, owner included");
        check(decision.reason == DenialReason::RequiresConnection,
              "because it needs the server, not because of who they are");
    }

    {
        Session limited = owner;
        limited.rights.clear();
        limited.rights.grant(protocol::RightId::right_order_read);
        const Decision decision = may_run(protocol::OperationId::order_create,
                                          limited, ConnectionState::Online, all);
        check(!decision.allowed, "a missing right refuses");
        check(decision.reason == DenialReason::NoRight, "for the right reason");
    }

    {
        const protocol::Activation without_agreements =
            protocol::resolve_activation({protocol::ModuleId::agreements})
                .activation;
        const Decision decision =
            may_run(protocol::OperationId::agreement_create, owner,
                    ConnectionState::Online, without_agreements);
        check(!decision.allowed, "a switched-off module refuses");
        check(decision.reason == DenialReason::ModuleInactive,
              "and says the shop does not use it, rather than blaming "
              "permissions");
        check(decision.explanation.find("agreements") != std::string::npos,
              "naming the module");
    }

    {
        Session nobody;
        const Decision decision = may_run(protocol::OperationId::counter_sale,
                                          nobody, ConnectionState::Online, all);
        check(!decision.allowed, "nobody signed in means nothing runs");
        check(decision.reason == DenialReason::NotSignedIn, "stated plainly");
    }

    check(may_run(protocol::OperationId::counter_sale, owner,
                  ConnectionState::Metered, all)
              .allowed,
          "a metered connection changes how sync behaves, not what is allowed");
    check(may_run(protocol::OperationId::counter_sale, owner,
                  ConnectionState::Weak, all)
              .allowed,
          "and neither does a slow one");
}

void test_staff_exceptions_are_coherent() {
    section("staff offline exceptions");

    // Every operation on the staff exception list must itself be usable
    // offline. A contradiction here would be a rule that can never fire.
    for (const protocol::OperationInfo& info : protocol::all_operations()) {
        if (protocol::staff_offline_exception(info.id)) {
            check(info.offline == protocol::OfflineRule::OfflineAllowed,
                  std::string(info.name) +
                      " is a staff offline exception, so it must be offline-"
                      "allowed");
        }
    }
}

}  // namespace

int main() {
    test_identity();
    test_quantity();
    test_money();
    test_lifecycle();
    test_numbering();
    test_signatures_and_approvals();
    test_rights();
    test_capability();
    test_staff_exceptions_are_coherent();
    return squiflow::testing::report();
}
