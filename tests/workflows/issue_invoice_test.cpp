#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "engine/audit/audit_log.hpp"
#include "engine/records/payload.hpp"
#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "engine/sync/outbox.hpp"
#include "modules/orders/module.hpp"
#include "modules/agreements/module.hpp"
#include "modules/pricing/module.hpp"
#include "modules/receivables/data/repository.hpp"
#include "modules/receivables/domain/document_number_block.hpp"
#include "modules/receivables/module.hpp"
#include "modules/registry.hpp"
#include "support/check.hpp"
#include "workflows/issue_invoice.hpp"

using squiflow::testing::check;
using squiflow::testing::section;
namespace e = squiflow::engine;
namespace m = squiflow::modules;
namespace r = squiflow::modules::receivables;
namespace p = squiflow::protocol;
namespace w = squiflow::workflows;

namespace {

constexpr std::int64_t kNow = 1'900'000'000'000LL;
std::int64_t now() { return kNow; }

const std::string Person = "81000000000000000000000000000001";
const std::string Device = "81000000000000000000000000000002";
const std::string OtherDevice = "81000000000000000000000000000003";
const std::string Party = "82000000000000000000000000000001";
const std::string Invoice1 = "83000000000000000000000000000001";
const std::string Invoice2 = "83000000000000000000000000000002";
const std::string Invoice3 = "83000000000000000000000000000003";
const std::string Invoice4 = "83000000000000000000000000000004";

e::Session owner() {
    e::Session session;
    session.person = e::record_id_from_string(Person);
    session.device = e::record_id_from_string(Device);
    session.is_owner = true;
    session.rights.grant_all();
    return session;
}

e::Blob payload(const std::string& series = "INV-OWNER",
                std::int64_t count = 2, std::int64_t total = 3000) {
    e::Row row;
    row.set("series", e::Value::text(series));
    row.set("expected_line_count", e::Value::integer(count));
    row.set("expected_total_minor", e::Value::integer(total));
    return e::encode_payload(row);
}

r::DocumentNumberBlock block(const std::string& id, std::uint64_t first,
                             std::uint64_t last,
                             const std::string& device = Device,
                             const std::string& series = "INV-OWNER") {
    r::DocumentNumberBlock value;
    value.id = id;
    value.series = series;
    value.device_id = device;
    value.first_number = first;
    value.next_number = first;
    value.last_number = last;
    value.assigned_at = kNow - 100'000;
    value.assignment_reference = "server-reservation-" + id;
    return value;
}

struct Shop {
    m::Registry registry{now};
    std::unique_ptr<e::Database> database;

    Shop() {
        registry.add(m::pricing::make_module(now));
        registry.add(m::orders::make_module(now));
        registry.add(r::make_module(now));
        registry.add(m::agreements::make_module(now));
        registry.install_workflow(w::make_issue_invoice(now));
        e::MigrationRunner runner{now};
        registry.collect_migrations(runner);
        database = std::make_unique<e::Database>(
            std::make_unique<e::MemoryStore>(), std::move(runner));
        database->open();
    }

    void seed_draft(const std::string& id, bool with_lines = true,
                    std::int64_t due_at = 0,
                    const std::string& party = Party) {
        database->write([&](e::Transaction& transaction) {
            r::Invoice invoice;
            invoice.id = id;
            invoice.party_id = party;
            invoice.due_at = due_at;
            invoice.note = "Confirmed draft";
            invoice.created_at = kNow - 10'000;
            invoice.created_by = Person;
            r::data::save_invoice(transaction, invoice);
            if (!with_lines) return;

            r::InvoiceLine first;
            first.id = id.substr(0, 30) + id.substr(31) + "a";
            first.invoice_id = id;
            first.description = "Frozen banner \xE2\x9C\x93";
            first.quantity_scaled = 1000;
            first.rate_minor = 500;
            first.amount_minor = 500;
            first.added_at = kNow - 9000;
            first.added_by = Person;
            r::data::save_invoice_line(transaction, first);

            r::InvoiceLine second;
            second.id = id.substr(0, 30) + id.substr(31) + "b";
            second.invoice_id = id;
            second.position = 1;
            second.description = std::string("Manual large proof ") +
                                 std::string(4096, 'x');
            second.quantity_scaled = 2000;
            second.rate_minor = 1250;
            second.amount_minor = 2500;
            second.rate_origin = e::RateOrigin::ManualOverride;
            second.rate_reason = "Customer-approved fixed rate";
            second.added_at = kNow - 8000;
            second.added_by = Person;
            r::data::save_invoice_line(transaction, second);
        });
    }

    void seed_block(const r::DocumentNumberBlock& number_block) {
        database->write([&](e::Transaction& transaction) {
            r::data::save_number_block(transaction, number_block);
        });
    }

    void seed_credit(std::int32_t period_days) {
        database->write([&](e::Transaction& transaction) {
            r::CreditAccount account;
            account.id = Party;
            account.party_id = Party;
            account.credit_limit_minor = 100'000;
            account.credit_period_days = period_days;
            account.cycle_day = 15;
            account.updated_at = kNow - 20'000;
            account.updated_by = Person;
            r::data::save_credit_account(transaction, account);
        });
    }

    m::Outcome issue(const std::string& invoice, const std::string& key,
                     const e::Blob& body = payload(),
                     const e::Session& session = owner(),
                     e::ConnectionState connection = e::ConnectionState::Online) {
        m::Call call;
        call.operation = p::OperationId::issue_invoice;
        call.record_id = invoice;
        call.idempotency_key = key;
        call.payload = body;
        return registry.run(*database, call, session, connection);
    }
};

std::size_t count(const Shop& shop, const std::string& table) {
    std::size_t result = 0;
    shop.database->read([&](const e::Store& store) {
        result = store.count(table);
    });
    return result;
}

}  // namespace

int main() {
    section("migration 23 and reserved block invariants");
    {
        Shop shop;
        check(r::tables::kNumberBlockMigration == 23,
              "reserved number blocks own migration 23");
        shop.database->read([](const e::Store& store) {
            check(store.has_table(r::tables::kNumberBlock),
                  "migration creates number-block table");
        });

        r::DocumentNumberBlock single = block("single", 7, 7);
        const auto seven = r::allocate(single);
        check(seven && *seven == 7 && single.exhausted,
              "single-number range allocates its boundary once");
        check(!r::allocate(single), "exhausted range cannot allocate twice");

        r::DocumentNumberBlock highest = block(
            "highest",
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()),
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()),
            Device, "INV-MAX");
        const auto maximum = r::allocate(highest);
        check(maximum &&
                  *maximum == static_cast<std::uint64_t>(
                                  std::numeric_limits<std::int64_t>::max()) &&
                  highest.exhausted,
              "largest storable final number has no increment overflow");

        bool malformed = false;
        try {
            r::DocumentNumberBlock bad = block("bad", 9, 8);
            r::validate(bad);
        } catch (const m::RuleViolation&) {
            malformed = true;
        }
        check(malformed, "reversed range fails closed");

        shop.seed_block(block("range-a", 100, 110));
        bool overlap_refused = false;
        try {
            shop.seed_block(block("range-b", 110, 120, OtherDevice));
        } catch (const m::RuleViolation&) {
            overlap_refused = true;
        }
        check(overlap_refused,
              "overlap is refused globally even across devices");
    }

    section("offline issue freezes evidence and replays without another number");
    {
        Shop shop;
        shop.seed_draft(Invoice1);
        shop.seed_credit(30);
        shop.seed_block(block("owner-range", 500, 501));
        const m::Outcome issued = shop.issue(
            Invoice1, "issue-one", payload(), owner(),
            e::ConnectionState::Offline);
        check(issued.ok && issued.queued && !issued.replayed,
              "owner can issue offline from a reserved block");
        shop.database->read([](const e::Store& store) {
            const auto invoice = r::data::find_invoice(store, Invoice1);
            check(invoice && invoice->state == e::DocumentState::Issued,
                  "draft becomes issued");
            check(invoice && invoice->number_series == "INV-OWNER" &&
                              invoice->number == 500,
                  "final number comes from device block");
            check(invoice && invoice->issued_at == kNow &&
                              invoice->issued_by == Person,
                  "issuer and issue time retained");
            check(invoice && invoice->due_at ==
                                  kNow + 30LL * 86'400'000LL,
                  "credit terms determine frozen due date");
            const auto lines = r::data::lines_for_invoice(store, Invoice1);
            check(lines.size() == 2 && lines[0].amount_minor == 500 &&
                              lines[1].amount_minor == 2500 &&
                              lines[1].rate_reason ==
                                  "Customer-approved fixed rate",
                  "line prices and provenance remain unchanged");
            const auto number_block =
                r::data::find_number_block(store, "owner-range");
            check(number_block && number_block->next_number == 501 &&
                                    !number_block->exhausted,
                  "successful issue persists exactly one consumption");
            check(store.find(e::AuditLog::table_name(), "issue-one").has_value(),
                  "workflow audit committed");
            check(store.find(e::Outbox::table_name(), "issue-one").has_value(),
                  "outbox change committed");
        });

        const m::Outcome replay = shop.issue(Invoice1, "issue-one");
        check(replay.ok && replay.replayed && !replay.queued,
              "same idempotency key replays before the handler");
        shop.database->read([](const e::Store& store) {
            const auto number_block =
                r::data::find_number_block(store, "owner-range");
            check(number_block && number_block->next_number == 501,
                  "replay consumes no second number");
        });
        check(!shop.issue(Invoice1, "different-key").ok,
              "different-key retry cannot reissue an issued invoice");
        check(count(shop, e::AuditLog::table_name()) == 1 &&
                  count(shop, e::Outbox::table_name()) == 1,
              "duplicate attempts add no audit or outbox evidence");
    }

    section("sequential blocks roll over and remain device scoped");
    {
        Shop shop;
        shop.seed_draft(Invoice1);
        shop.seed_draft(Invoice2);
        shop.seed_draft(Invoice3);
        shop.seed_block(block("other-device", 1, 5, OtherDevice));
        shop.seed_block(block("first", 10, 11));
        shop.seed_block(block("second", 12, 12));
        check(shop.issue(Invoice1, "roll-1").ok,
              "first number issues");
        check(shop.issue(Invoice2, "roll-2").ok,
              "last number in first block issues");
        check(shop.issue(Invoice3, "roll-3").ok,
              "next reserved block is selected");
        shop.database->read([](const e::Store& store) {
            const auto one = r::data::find_invoice(store, Invoice1);
            const auto two = r::data::find_invoice(store, Invoice2);
            const auto three = r::data::find_invoice(store, Invoice3);
            check(one && two && three && one->number == 10 &&
                      two->number == 11 && three->number == 12,
                  "numbers are unique and monotonically consumed");
            const auto first = r::data::find_number_block(store, "first");
            const auto second = r::data::find_number_block(store, "second");
            const auto other =
                r::data::find_number_block(store, "other-device");
            check(first && first->exhausted && second && second->exhausted,
                  "both owner blocks retain exhaustion evidence");
            check(other && other->next_number == 1 && !other->exhausted,
                  "another device's block is untouched");
        });
    }

    section("stale malformed and unauthorized requests fail before writes");
    {
        Shop stale;
        stale.seed_draft(Invoice1);
        stale.seed_block(block("stale", 20, 30));
        check(!stale.issue(Invoice1, "wrong-count", payload("INV-OWNER", 1, 3000)).ok,
              "stale line count is refused");
        check(!stale.issue(Invoice1, "wrong-total", payload("INV-OWNER", 2, 2999)).ok,
              "stale total is refused");
        e::Row unknown;
        unknown.set("series", e::Value::text("INV-OWNER"));
        unknown.set("expected_line_count", e::Value::integer(2));
        unknown.set("expected_total_minor", e::Value::integer(3000));
        unknown.set("number", e::Value::integer(999));
        check(!stale.issue(Invoice1, "caller-number",
                           e::encode_payload(unknown)).ok,
              "caller cannot supply a final number");
        check(!stale.issue(Invoice1, "damaged", e::Blob{1, 2, 3}).ok,
              "malformed payload fails closed");

        e::Session denied = owner();
        denied.rights.revoke(p::RightId::right_invoice_issue);
        check(!stale.issue(Invoice1, "denied", payload(), denied).ok,
              "invoice issue right is mandatory");
        check(count(stale, r::tables::kInvoice) == 1 &&
                  count(stale, e::AuditLog::table_name()) == 0 &&
                  count(stale, e::Outbox::table_name()) == 0,
              "all early refusals leave no side effects");
        stale.database->read([](const e::Store& store) {
            const auto number_block = r::data::find_number_block(store, "stale");
            const auto invoice = r::data::find_invoice(store, Invoice1);
            check(number_block && number_block->next_number == 20 &&
                      invoice && invoice->state == e::DocumentState::Draft,
                  "stale and denied attempts consume nothing");
        });
    }

    section("empty expired and unreserved drafts cannot issue");
    {
        Shop shop;
        shop.seed_draft(Invoice1, false);
        shop.seed_draft(Invoice2, true, kNow - 1);
        shop.seed_draft(Invoice3, true, 0, "");
        shop.seed_block(block("only-other-series", 40, 41,
                              Device, "INV-OTHER"));
        check(!shop.issue(Invoice1, "empty").ok,
              "empty draft is refused");
        check(!shop.issue(Invoice2, "past-due").ok,
              "due date before issue is refused");
        check(!shop.issue(Invoice3, "wrong-series").ok,
              "missing requested series block is refused");
        check(count(shop, e::AuditLog::table_name()) == 0 &&
                  count(shop, e::Outbox::table_name()) == 0,
              "business refusals create no workflow evidence");
    }

    section("duplicate final number rolls back persisted block consumption");
    {
        Shop shop;
        shop.seed_draft(Invoice1);
        shop.seed_draft(Invoice2);
        shop.seed_block(block("collision", 50, 51));
        shop.database->write([](e::Transaction& transaction) {
            auto existing = r::data::find_invoice(transaction, Invoice2);
            existing->state = e::DocumentState::Issued;
            existing->number_series = "INV-OWNER";
            existing->number = 50;
            existing->issued_at = kNow - 100;
            existing->issued_by = Person;
            r::data::save_invoice(transaction, *existing);
        });
        check(!shop.issue(Invoice1, "collision-attempt").ok,
              "damaged duplicate number is refused");
        shop.database->read([](const e::Store& store) {
            const auto number_block =
                r::data::find_number_block(store, "collision");
            const auto draft = r::data::find_invoice(store, Invoice1);
            check(number_block && number_block->next_number == 50 &&
                                    !number_block->exhausted,
                  "transaction rollback restores consumed number");
            check(draft && draft->state == e::DocumentState::Draft &&
                            draft->number == 0,
                  "transaction rollback restores invoice draft");
        });
        check(count(shop, e::AuditLog::table_name()) == 0 &&
                  count(shop, e::Outbox::table_name()) == 0,
              "rolled-back issue leaves no audit or outbox row");
    }

    return squiflow::testing::report();
}
