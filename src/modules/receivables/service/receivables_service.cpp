#include "modules/receivables/service/receivables_service.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

#include "engine/identity/session.hpp"
#include "engine/records/identity.hpp"
#include "engine/records/payload.hpp"
#include "modules/parties/data/repository.hpp"
#include "modules/receivables/data/repository.hpp"

namespace squiflow::modules::receivables {
namespace {

bool blank(const std::string& text) noexcept {
    for (const char c : text) if (static_cast<unsigned char>(c) > ' ') return false;
    return true;
}

engine::Row fields(const Call& call) {
    try {
        return engine::decode_payload(call.payload);
    } catch (const engine::PayloadError&) {
        throw RuleViolation("This request could not be read. Please try it again.");
    }
}

const engine::Session& actor(const Call& call) {
    if (call.actor == nullptr) throw std::logic_error("receivables: request has no session");
    return *call.actor;
}

std::string required_text(const engine::Row& row, const char* name,
                          const char* complaint) {
    const std::string* value = row.get(name).as_text();
    if (value == nullptr || blank(*value)) throw RuleViolation(complaint);
    return *value;
}

std::string optional_text(const engine::Row& row, const char* name,
                          const char* complaint) {
    if (!row.has(name)) return {};
    const std::string* value = row.get(name).as_text();
    if (value == nullptr) throw RuleViolation(complaint);
    return *value;
}

std::int64_t required_number(const engine::Row& row, const char* name,
                             const char* complaint) {
    const auto value = row.get(name).as_integer();
    if (!value) throw RuleViolation(complaint);
    return *value;
}

std::string subject(const Call& call) {
    if (blank(call.record_id)) throw RuleViolation("This request does not identify its record.");
    return call.record_id;
}

template <typename Reader>
Invoice existing_invoice(const Reader& reader, const std::string& id) {
    const auto invoice = data::find_invoice(reader, id);
    if (!invoice) throw RuleViolation("That invoice is not on file.");
    validate(*invoice);
    return *invoice;
}

template <typename Reader>
Payment existing_payment(const Reader& reader, const std::string& id) {
    const auto payment = data::find_payment(reader, id);
    if (!payment) throw RuleViolation("That payment is not on file.");
    validate(*payment);
    return *payment;
}

engine::RateOrigin origin_from(std::int64_t value) {
    switch (value) {
        case 0: return engine::RateOrigin::CatalogDefault;
        case 1: return engine::RateOrigin::PartySpecific;
        case 2: return engine::RateOrigin::Agreement;
        case 3: return engine::RateOrigin::ManualOverride;
        case 4: return engine::RateOrigin::OffCatalog;
        default: throw RuleViolation("That price origin is not understood.");
    }
}

std::string invoice_reference(const Invoice& invoice) {
    return invoice.number_series + "-" + std::to_string(invoice.number);
}

StatementEntry event(const std::string& statement_id, StatementEntryKind kind,
                     const std::string& source, std::int64_t when,
                     const std::string& reference, const std::string& description,
                     std::int64_t amount) {
    StatementEntry entry;
    entry.id = statement_id + ":" + std::to_string(static_cast<unsigned>(kind)) + ":" + source;
    entry.statement_id = statement_id;
    entry.kind = kind;
    entry.source_id = source;
    entry.occurred_at = when;
    entry.reference = reference;
    entry.description = description;
    entry.amount_minor = amount;
    return entry;
}

std::vector<StatementEntry> all_statement_events(const engine::Store& store,
                                                 const std::string& statement_id,
                                                 const std::string& party_id) {
    std::vector<StatementEntry> result;
    for (const Invoice& invoice : data::invoices_for_party(store, party_id)) {
        validate(invoice);
        if (invoice.state == engine::DocumentState::Draft ||
            invoice.state == engine::DocumentState::Discarded) continue;
        const engine::MoneyResult total = invoice_total(data::lines_for_invoice(store, invoice.id));
        if (!total.ok || total.value.minor <= 0) {
            throw RuleViolation("An issued invoice has no usable positive total.");
        }
        result.push_back(event(statement_id, StatementEntryKind::InvoiceCharged,
                               invoice.id, invoice.issued_at,
                               invoice_reference(invoice), "Invoice charged", total.value.minor));
        if (invoice.state == engine::DocumentState::Cancelled ||
            invoice.state == engine::DocumentState::Replaced) {
            result.push_back(event(statement_id, StatementEntryKind::InvoiceCancelled,
                                   invoice.id, invoice.cancelled_at,
                                   invoice_reference(invoice), "Invoice cancelled", total.value.minor));
        }
    }
    for (const Payment& payment : data::payments_for_party(store, party_id)) {
        validate(payment);
        result.push_back(event(statement_id, StatementEntryKind::PaymentReceived,
                               payment.id, payment.paid_at,
                               payment.receipt_series + "-" +
                                   std::to_string(payment.receipt_number),
                               "Payment received", payment.amount_minor));
        for (const PaymentAllocation& allocation :
             data::allocations_for_payment(store, payment.id)) {
            validate(allocation);
            if (allocation.target.module != protocol::ModuleId::receivables) continue;
            const std::string target = engine::to_string(allocation.target.record);
            const auto invoice = data::find_invoice(store, target);
            if (!invoice || invoice->party_id != party_id) {
                throw RuleViolation("A payment allocation points outside its customer account.");
            }
            result.push_back(event(statement_id, StatementEntryKind::PaymentAllocated,
                                   allocation.id, allocation.allocated_at,
                                   invoice_reference(*invoice), "Payment allocated",
                                   allocation.amount_minor));
            if (allocation.state == AllocationState::Released) {
                result.push_back(event(statement_id, StatementEntryKind::AllocationReleased,
                                       allocation.id + ":release", allocation.released_at,
                                       invoice_reference(*invoice), "Allocation released",
                                       allocation.amount_minor));
            }
        }
    }
    return result;
}

}  // namespace

void ReceivablesService::invoice_draft_create(engine::Transaction& transaction,
                                               const Call& call) const {
    const engine::Row row = fields(call);
    const std::string id = subject(call);
    const engine::Session& who = actor(call);
    if (data::find_invoice(transaction, id)) throw RuleViolation("That invoice already exists.");
    Invoice invoice;
    invoice.id = id;
    invoice.party_id = optional_text(row, "party_id", "That customer is not readable.");
    invoice.due_at = row.has("due_at")
        ? required_number(row, "due_at", "That due date is not readable.") : 0;
    invoice.note = optional_text(row, "note", "That invoice note is not readable.");
    invoice.created_at = clock_();
    invoice.created_by = engine::to_string(who.person);
    data::save_invoice(transaction, invoice);
}

void ReceivablesService::invoice_draft_update(engine::Transaction& transaction,
                                               const Call& call) const {
    const engine::Row row = fields(call);
    const std::string invoice_id = subject(call);
    const engine::Session& who = actor(call);
    Invoice invoice = existing_invoice(transaction, invoice_id);
    if (invoice.state != engine::DocumentState::Draft) {
        throw RuleViolation("Only a draft invoice can be changed.");
    }
    const std::string action = required_text(
        row, "action", "An invoice draft update must name its action.");
    if (action == "metadata") {
        bool changed = false;
        if (row.has("due_at")) {
            invoice.due_at = required_number(row, "due_at", "That due date is not readable.");
            changed = true;
        }
        if (row.has("note")) {
            invoice.note = optional_text(row, "note", "That invoice note is not readable.");
            changed = true;
        }
        if (!changed) throw RuleViolation("That metadata update changes nothing.");
        data::save_invoice(transaction, invoice);
        return;
    }
    if (action == "line_remove") {
        const std::string line_id = required_text(row, "line_id", "A line removal needs a line.");
        const auto line = data::find_invoice_line(transaction, line_id);
        if (!line || line->invoice_id != invoice_id) {
            throw RuleViolation("That line is not on this invoice.");
        }
        if (!data::remove_invoice_line(transaction, line_id)) {
            throw RuleViolation("That invoice line could not be removed.");
        }
        return;
    }
    if (action != "line_upsert") throw RuleViolation("That invoice draft action is not understood.");

    const std::string line_id = required_text(row, "line_id", "An invoice line needs an id.");
    const auto prior = data::find_invoice_line(transaction, line_id);
    if (prior && prior->invoice_id != invoice_id) {
        throw RuleViolation("That line belongs to another invoice.");
    }
    InvoiceLine line = prior.value_or(InvoiceLine{});
    line.id = line_id;
    line.invoice_id = invoice_id;
    if (!prior) {
        const auto position = data::next_line_position(transaction, invoice_id);
        if (!position) throw RuleViolation("That invoice has no line position left.");
        line.position = *position;
        line.added_at = clock_();
        line.added_by = engine::to_string(who.person);
    }
    line.product_id = optional_text(row, "product_id", "That product is not readable.");
    line.description = required_text(row, "description", "An invoice line needs a description.");
    line.quantity_scaled = required_number(row, "quantity_scaled", "That quantity is not readable.");
    line.rate_minor = required_number(row, "rate_minor", "That rate is not readable.");
    line.rate_origin = origin_from(required_number(row, "rate_origin", "That price origin is not readable."));
    line.rate_reason = optional_text(row, "rate_reason", "That price reason is not readable.");
    const engine::MoneyResult amount = calculate_amount(line);
    if (!amount.ok) throw RuleViolation("That line amount is too large.");
    line.amount_minor = amount.value.minor;
    data::save_invoice_line(transaction, line);
}

void ReceivablesService::invoice_draft_discard(engine::Transaction& transaction,
                                                const Call& call) const {
    const std::string id = subject(call);
    const engine::Session& who = actor(call);
    Invoice invoice = existing_invoice(transaction, id);
    if (invoice.state != engine::DocumentState::Draft) {
        throw RuleViolation("Only a draft invoice can be discarded.");
    }
    invoice.state = engine::DocumentState::Discarded;
    invoice.discarded_at = clock_();
    invoice.discarded_by = engine::to_string(who.person);
    data::save_invoice(transaction, invoice);
}

void ReceivablesService::payment_allocate(engine::Transaction& transaction,
                                           const Call& call) const {
    const engine::Row row = fields(call);
    const std::string payment_id = subject(call);
    const engine::Session& who = actor(call);
    const Payment payment = existing_payment(transaction, payment_id);
    const std::string action = required_text(row, "action", "A payment allocation needs an action.");
    const std::string allocation_id = required_text(
        row, "allocation_id", "A payment allocation needs an id.");
    if (action == "release") {
        auto allocation = data::find_allocation(transaction, allocation_id);
        if (!allocation || allocation->payment_id != payment_id) {
            throw RuleViolation("That allocation is not on this payment.");
        }
        if (allocation->state != AllocationState::Active) {
            throw RuleViolation("That allocation has already been released.");
        }
        allocation->state = AllocationState::Released;
        allocation->released_at = clock_();
        allocation->released_by = engine::to_string(who.person);
        allocation->release_reason = required_text(
            row, "reason", "Releasing an allocation requires a reason.");
        data::save_allocation(transaction, *allocation);
        return;
    }
    if (action != "allocate") throw RuleViolation("That allocation action is not understood.");
    if (data::find_allocation(transaction, allocation_id)) {
        throw RuleViolation("That allocation already exists.");
    }
    const std::int64_t amount = required_number(row, "amount_minor", "That amount is not readable.");
    const engine::MoneyResult available = unallocated_amount(
        payment, data::allocations_for_payment(transaction, payment_id));
    if (!available.ok || amount <= 0 || amount > available.value.minor) {
        throw RuleViolation("That payment does not have enough unallocated money.");
    }
    const std::string module = required_text(row, "target_module", "An allocation needs a target type.");
    const std::string target_id = required_text(row, "target_id", "An allocation needs a target record.");
    PaymentAllocation allocation;
    allocation.id = allocation_id;
    allocation.payment_id = payment_id;
    allocation.target.module = module == "invoice" ? protocol::ModuleId::receivables
                                                     : protocol::ModuleId::jobs;
    if (module != "invoice" && module != "job") {
        throw RuleViolation("An allocation target must be an invoice or job.");
    }
    allocation.target.record = engine::record_id_from_string(target_id);
    allocation.amount_minor = amount;
    allocation.allocated_at = clock_();
    allocation.allocated_by = engine::to_string(who.person);
    if (allocation.target.module == protocol::ModuleId::receivables) {
        const Invoice invoice = existing_invoice(transaction, target_id);
        if (invoice.state != engine::DocumentState::Issued) {
            throw RuleViolation("Money can only be allocated to an issued invoice.");
        }
        if (invoice.party_id != payment.party_id) {
            throw RuleViolation("A payment cannot be allocated to another customer.");
        }
        const engine::MoneyResult outstanding = data::outstanding_for_invoice(transaction, target_id);
        if (!outstanding.ok || amount > outstanding.value.minor) {
            throw RuleViolation("That invoice does not have that much outstanding.");
        }
    }
    data::save_allocation(transaction, allocation);
}

void ReceivablesService::credit_account_set(engine::Transaction& transaction,
                                             const Call& call) const {
    const engine::Row row = fields(call);
    const std::string party_id = subject(call);
    const engine::Session& who = actor(call);
    const auto party = parties::data::find_party(transaction, party_id);
    if (!party || !party->is_customer || party->kind != parties::PartyKind::Organisation) {
        throw RuleViolation("Credit accounts are only for recorded customer organizations.");
    }
    CreditAccount account;
    account.id = party_id;
    account.party_id = party_id;
    account.credit_limit_minor = required_number(row, "credit_limit_minor", "That credit limit is not readable.");
    const std::int64_t period = required_number(row, "credit_period_days", "That credit period is not readable.");
    const std::int64_t cycle = required_number(row, "cycle_day", "That cycle day is not readable.");
    if (period < 0 || period > static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()) ||
        cycle < 0 || cycle > static_cast<std::int64_t>(std::numeric_limits<std::uint8_t>::max())) {
        throw RuleViolation("Those credit terms are outside their usable range.");
    }
    account.credit_period_days = static_cast<std::int32_t>(period);
    account.cycle_day = static_cast<std::uint8_t>(cycle);
    account.updated_at = clock_();
    account.updated_by = engine::to_string(who.person);
    data::save_credit_account(transaction, account);
}

std::vector<engine::Row> ReceivablesService::statement_prepare(
    const engine::Store& store, const Call& call) const {
    const engine::Row row = fields(call);
    static_cast<void>(actor(call));
    const std::string statement_id = required_text(row, "statement_id", "A statement needs an id.");
    const std::string party_id = required_text(row, "party_id", "A statement needs a customer.");
    const std::int64_t from = required_number(row, "period_from", "That statement start is not readable.");
    const std::int64_t through = required_number(row, "period_through", "That statement end is not readable.");
    const std::int64_t now = clock_();
    if (through > now) throw RuleViolation("A statement cannot include a period that has not finished.");
    std::vector<StatementEntry> events = all_statement_events(store, statement_id, party_id);

    Statement final_statement;
    final_statement.id = statement_id;
    final_statement.party_id = party_id;
    final_statement.period_from = from;
    final_statement.period_through = through;
    final_statement.prepared_at = now;
    final_statement.prepared_by = engine::to_string(actor(call).person);

    if (from > 1) {
        Statement prior = final_statement;
        prior.id = statement_id + ":opening";
        prior.period_from = 1;
        prior.period_through = from - 1;
        prior.entries.clear();
        for (StatementEntry entry_value : events) {
            if (entry_value.occurred_at < from) {
                entry_value.statement_id = prior.id;
                entry_value.id = prior.id + ":" + entry_value.id;
                prior.entries.push_back(std::move(entry_value));
            }
        }
        const StatementResult opening = prepare_statement(std::move(prior));
        if (!opening.ok) throw RuleViolation("Earlier customer activity cannot be reconciled.");
        final_statement.totals.opening_outstanding_minor = opening.value.totals.outstanding_minor;
        final_statement.totals.opening_unallocated_minor = opening.value.totals.unallocated_minor;
    }
    for (StatementEntry entry_value : events) {
        if (entry_value.occurred_at >= from && entry_value.occurred_at <= through) {
            final_statement.entries.push_back(std::move(entry_value));
        }
    }
    const StatementResult prepared = prepare_statement(std::move(final_statement));
    if (!prepared.ok) throw RuleViolation("That customer statement cannot be reconciled.");
    std::vector<engine::Row> rows;
    engine::Row header = to_row(prepared.value);
    header.set("row_type", engine::Value::text("statement"));
    rows.push_back(std::move(header));
    for (const StatementEntry& entry_value : prepared.value.entries) {
        engine::Row output = to_row(entry_value);
        output.set("row_type", engine::Value::text("statement_entry"));
        rows.push_back(std::move(output));
    }
    return rows;
}

void ReceivablesService::statement_send(engine::Transaction& transaction,
                                         const Call& call) const {
    const engine::Row row = fields(call);
    const engine::Session& who = actor(call);
    StatementDelivery delivery;
    delivery.id = required_text(row, "delivery_id", "A statement delivery needs an id.");
    if (data::find_statement_delivery(transaction, delivery.id)) {
        throw RuleViolation("That statement delivery has already been confirmed.");
    }
    delivery.statement_id = required_text(row, "statement_id", "A delivery needs a statement.");
    delivery.recipient = required_text(row, "recipient", "A delivery needs its confirmed recipient.");
    delivery.content_hash = required_text(row, "content_hash", "A delivery needs the exact content hash.");
    delivery.transport_reference = required_text(
        row, "transport_reference", "The message has no confirmed transport reference.");
    delivery.sent_at = clock_();
    delivery.sent_by = engine::to_string(who.person);
    data::save_statement_delivery(transaction, delivery);
}

std::vector<engine::Row> ReceivablesService::document_print(
    const engine::Store& store, const Call& call) const {
    const engine::Row row = fields(call);
    static_cast<void>(actor(call));
    const std::string type = required_text(row, "document_type", "A print request needs a document type.");
    if (type == "statement") return statement_prepare(store, call);
    const std::string id = required_text(row, "document_id", "A print request needs a document id.");
    if (type == "invoice") {
        const Invoice invoice = existing_invoice(store, id);
        if (invoice.state == engine::DocumentState::Draft ||
            invoice.state == engine::DocumentState::Discarded) {
            throw RuleViolation("Only issued invoice evidence can be printed.");
        }
        std::vector<engine::Row> rows;
        engine::Row header = to_row(invoice);
        header.set("row_type", engine::Value::text("invoice"));
        rows.push_back(std::move(header));
        for (const InvoiceLine& line : data::lines_for_invoice(store, id)) {
            engine::Row output = to_row(line);
            output.set("row_type", engine::Value::text("invoice_line"));
            rows.push_back(std::move(output));
        }
        return rows;
    }
    if (type == "receipt") {
        const Payment payment = existing_payment(store, id);
        engine::Row output = to_row(payment);
        output.set("row_type", engine::Value::text("receipt"));
        return {std::move(output)};
    }
    throw RuleViolation("That printable document type is not understood.");
}

}  // namespace squiflow::modules::receivables
