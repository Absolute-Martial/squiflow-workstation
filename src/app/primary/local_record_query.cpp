#include "app/primary/local_record_query.hpp"

#include "engine/records/identity.hpp"
#include "engine/records/lifecycle.hpp"
#include "engine/records/money.hpp"
#include "engine/records/quantity.hpp"
#include "engine/storage/database.hpp"
#include "modules/administration/data/repository.hpp"
#include "modules/agreements/data/repository.hpp"
#include "modules/catalog/data/repository.hpp"
#include "modules/companion/data/repository.hpp"
#include "modules/files/data/repository.hpp"
#include "modules/jobs/data/repository.hpp"
#include "modules/orders/data/repository.hpp"
#include "modules/parties/data/repository.hpp"
#include "modules/pricing/data/repository.hpp"
#include "modules/quotations/data/repository.hpp"
#include "modules/receivables/data/repository.hpp"
#include "modules/sourcing/data/repository.hpp"

#include <algorithm>
#include <exception>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace squiflow::app::primary {
namespace {
namespace admin = modules::administration;
namespace agreements = modules::agreements;
namespace catalog = modules::catalog;
namespace companion = modules::companion;
namespace files = modules::files;
namespace jobs = modules::jobs;
namespace orders = modules::orders;
namespace parties = modules::parties;
namespace pricing = modules::pricing;
namespace quotations = modules::quotations;
namespace receivables = modules::receivables;
namespace sourcing = modules::sourcing;

constexpr std::size_t kHistoryLimit = 24;
constexpr std::size_t kLineLimit = 24;

DomainError failure(DomainErrorCode code, std::string key,
                    std::string field = {}) {
    return {code, std::move(key),
            field.empty() ? std::optional<std::string>{}
                          : std::optional<std::string>{std::move(field)}};
}

std::string short_id(std::string_view id) {
    return std::string(id.substr(0, std::min<std::size_t>(8, id.size())));
}

std::string bool_text(bool value) {
    return value ? "yes" : "no";
}

std::string integer_text(std::int64_t value) {
    return std::to_string(value);
}

FieldSnapshot text_field(std::string id, std::string key, std::string value) {
    return {std::move(id), std::move(key), std::move(value), std::nullopt,
            std::nullopt};
}

FieldSnapshot money_field(std::string id, std::string key, std::int64_t minor) {
    return {std::move(id), std::move(key), engine::format(engine::Money{minor}),
            minor, std::nullopt};
}

FieldSnapshot quantity_field(std::string id, std::string key, std::int64_t scaled) {
    return {std::move(id), std::move(key), engine::format(engine::Quantity{scaled}),
            std::nullopt, scaled};
}

HistorySnapshot history_item(std::string id, std::string key, std::string detail,
                             std::int64_t occurred_at_ms) {
    return {std::move(id), std::move(key), std::move(detail), occurred_at_ms};
}

ActionSnapshot action(std::string id, std::string key, protocol::OperationId op,
                      std::string record_id) {
    return {std::move(id), std::move(key), op, std::move(record_id)};
}

void push_history(std::vector<HistorySnapshot>& history, std::string id,
                  std::string key, std::string detail, std::int64_t occurred_at_ms) {
    if (occurred_at_ms > 0 && history.size() < kHistoryLimit) {
        history.push_back(history_item(std::move(id), std::move(key),
                                       std::move(detail), occurred_at_ms));
    }
}

void sort_history(std::vector<HistorySnapshot>& history) {
    std::sort(history.begin(), history.end(),
              [](const HistorySnapshot& left, const HistorySnapshot& right) {
                  if (left.occurred_at_ms != right.occurred_at_ms) {
                      return left.occurred_at_ms < right.occurred_at_ms;
                  }
                  return left.id < right.id;
              });
}

Result<RecordSnapshot, DomainError> malformed(std::string field) {
    return Result<RecordSnapshot, DomainError>::failure(
        failure(DomainErrorCode::ValidationFailed,
                "record.error.invalid_snapshot", std::move(field)));
}

std::string invoice_title(const receivables::Invoice& invoice) {
    if (invoice.number > 0) {
        std::string title = invoice.number_series;
        if (!title.empty()) {
            title += "-";
        }
        title += std::to_string(invoice.number);
        return title;
    }
    return "Draft " + short_id(invoice.id);
}

std::string job_title(const jobs::Job& job) {
    if (job.ticket_number > 0) {
        std::string title = job.ticket_series;
        if (!title.empty()) {
            title += "-";
        }
        title += std::to_string(job.ticket_number);
        return title;
    }
    return "Draft " + short_id(job.id);
}

std::string quotation_title(const quotations::Quotation& quotation,
                            const std::optional<quotations::QuotationRevision>& revision) {
    if (revision && revision->issued && revision->number > 0) {
        std::string title = revision->series;
        if (!title.empty()) {
            title += "-";
        }
        title += std::to_string(revision->number);
        return title;
    }
    return "Quote " + short_id(quotation.id);
}

std::string agreement_title(const agreements::Agreement& agreement) {
    return agreement.customer_reference.empty()
        ? "Agreement " + short_id(agreement.id)
        : agreement.customer_reference;
}

std::string file_title(const files::FileAsset& asset) {
    std::string title = "File ";
    title += asset.content_hash.substr(0, std::min<std::size_t>(12, asset.content_hash.size()));
    if (!asset.extension.empty()) {
        title += "." + asset.extension;
    }
    return title;
}

std::string source_or_default(std::string text, std::string fallback) {
    return text.empty() ? std::move(fallback) : std::move(text);
}

Result<RecordSnapshot, DomainError> administration_snapshot(const engine::Store& store,
                                                            std::string_view stable_id) {
    const auto person = admin::data::find_person(store, std::string(stable_id));
    if (!person) {
        return Result<RecordSnapshot, DomainError>::failure(
            failure(DomainErrorCode::NotFound, "record.error.not_found"));
    }

    RecordSnapshot snapshot;
    snapshot.stable_id = person->id;
    snapshot.title = person->display_name;
    snapshot.subtitle = person->username;
    snapshot.fields.push_back(text_field("access.username", "record.person.username",
                                         person->username));
    snapshot.fields.push_back(text_field("access.owner", "record.person.owner",
                                         bool_text(person->is_owner)));
    snapshot.fields.push_back(text_field("access.disabled", "record.person.disabled",
                                         bool_text(person->disabled)));
    snapshot.fields.push_back(text_field("audit.created_by", "record.person.created_by",
                                         person->created_by));
    snapshot.fields.push_back(text_field("audit.created_at", "record.person.created_at",
                                         integer_text(person->created_at)));
    snapshot.fields.push_back(text_field("audit.updated_at", "record.person.updated_at",
                                         integer_text(person->updated_at)));

    for (const auto right : admin::data::rights_of(store, person->id).granted()) {
        if (snapshot.lines.size() >= kLineLimit) {
            break;
        }
        snapshot.lines.push_back({"right." + std::string(protocol::right_name(right)),
                                  std::string(protocol::right_name(right)),
                                  std::string(protocol::module_name(
                                      protocol::right_module(right))),
                                  {}, {}, std::nullopt, std::nullopt});
    }

    push_history(snapshot.history, "person.created", "record.history.created",
                 person->created_by, person->created_at);
    push_history(snapshot.history, "person.updated", "record.history.updated",
                 person->created_by, person->updated_at);
    sort_history(snapshot.history);

    snapshot.actions.push_back(action("person.update", "record.action.person_update",
                                      protocol::OperationId::person_update, person->id));
    snapshot.actions.push_back(action("rights.grant", "record.action.right_grant",
                                      protocol::OperationId::right_grant, person->id));
    snapshot.actions.push_back(action("rights.revoke", "record.action.right_revoke",
                                      protocol::OperationId::right_revoke, person->id));
    if (!person->disabled) {
        snapshot.actions.push_back(action("person.disable",
                                          "record.action.person_disable",
                                          protocol::OperationId::person_disable,
                                          person->id));
    }
    return Result<RecordSnapshot, DomainError>::success(std::move(snapshot));
}

Result<RecordSnapshot, DomainError> parties_snapshot(const engine::Store& store,
                                                     std::string_view stable_id) {
    const auto party = parties::data::find_party(store, std::string(stable_id));
    if (!party) {
        return Result<RecordSnapshot, DomainError>::failure(
            failure(DomainErrorCode::NotFound, "record.error.not_found"));
    }
    RecordSnapshot snapshot;
    snapshot.stable_id = party->id;
    snapshot.title = party->display_name;
    snapshot.subtitle = std::string(parties::billing_name(party->terms.arrangement));
    snapshot.fields.push_back(text_field("party.kind", "record.party.kind",
                                         std::string(parties::party_kind_name(party->kind))));
    snapshot.fields.push_back(text_field("party.customer", "record.party.customer",
                                         bool_text(party->is_customer)));
    snapshot.fields.push_back(text_field("party.supplier", "record.party.supplier",
                                         bool_text(party->is_supplier)));
    snapshot.fields.push_back(text_field("party.billing", "record.party.billing",
                                         std::string(parties::billing_name(
                                             party->terms.arrangement))));
    snapshot.fields.push_back(text_field("party.net_days", "record.party.net_days",
                                         integer_text(party->terms.net_days)));
    snapshot.fields.push_back(text_field("party.reference", "record.party.reference",
                                         source_or_default(party->terms.customer_reference,
                                                           "none")));
    snapshot.fields.push_back(text_field("party.archived", "record.party.archived",
                                         bool_text(party->archived)));
    snapshot.fields.push_back(text_field("party.notes", "record.party.notes",
                                         source_or_default(party->notes, "none")));
    for (const auto& contact : parties::data::contacts_of(store, party->id)) {
        if (snapshot.lines.size() >= kLineLimit) {
            break;
        }
        snapshot.lines.push_back({"contact." + contact.id, contact.label,
                                  contact.value, {}, {}, std::nullopt,
                                  std::nullopt});
    }
    push_history(snapshot.history, "party.created", "record.history.created",
                 party->created_by, party->created_at);
    push_history(snapshot.history, "party.updated", "record.history.updated",
                 party->updated_by, party->updated_at);
    sort_history(snapshot.history);
    snapshot.actions.push_back(action("party.update", "record.action.party_update",
                                      protocol::OperationId::party_update, party->id));
    snapshot.actions.push_back(action("party.contact_add",
                                      "record.action.party_contact_add",
                                      protocol::OperationId::party_contact_add,
                                      party->id));
    snapshot.actions.push_back(action("party.terms_set",
                                      "record.action.party_terms_set",
                                      protocol::OperationId::party_terms_set,
                                      party->id));
    if (!party->archived) {
        snapshot.actions.push_back(action("party.archive",
                                          "record.action.party_archive",
                                          protocol::OperationId::party_archive,
                                          party->id));
    }
    return Result<RecordSnapshot, DomainError>::success(std::move(snapshot));
}

Result<RecordSnapshot, DomainError> catalog_snapshot(const engine::Store& store,
                                                     std::string_view stable_id) {
    const auto product = catalog::data::find_product(store, std::string(stable_id));
    if (!product) {
        return Result<RecordSnapshot, DomainError>::failure(
            failure(DomainErrorCode::NotFound, "record.error.not_found"));
    }
    RecordSnapshot snapshot;
    snapshot.stable_id = product->id;
    snapshot.title = product->name;
    snapshot.subtitle = source_or_default(product->description, "catalog item");
    snapshot.fields.push_back(text_field("product.description",
                                         "record.product.description",
                                         source_or_default(product->description, "none")));
    snapshot.fields.push_back(text_field("product.archived", "record.product.archived",
                                         bool_text(product->archived)));
    snapshot.fields.push_back(text_field("product.created_by", "record.product.created_by",
                                         product->created_by));
    snapshot.fields.push_back(text_field("product.updated_by", "record.product.updated_by",
                                         product->updated_by));
    push_history(snapshot.history, "product.created", "record.history.created",
                 product->created_by, product->created_at);
    push_history(snapshot.history, "product.updated", "record.history.updated",
                 product->updated_by, product->updated_at);
    sort_history(snapshot.history);
    snapshot.actions.push_back(action("product.update", "record.action.product_update",
                                      protocol::OperationId::product_update,
                                      product->id));
    if (!product->archived) {
        snapshot.actions.push_back(action("product.archive",
                                          "record.action.product_archive",
                                          protocol::OperationId::product_archive,
                                          product->id));
    }
    return Result<RecordSnapshot, DomainError>::success(std::move(snapshot));
}

Result<RecordSnapshot, DomainError> pricing_snapshot(const engine::Store& store,
                                                     std::string_view stable_id) {
    const auto rate = pricing::data::find_rate(store, std::string(stable_id));
    if (!rate) {
        return Result<RecordSnapshot, DomainError>::failure(
            failure(DomainErrorCode::NotFound, "record.error.not_found"));
    }
    RecordSnapshot snapshot;
    snapshot.stable_id = rate->id;
    snapshot.title = rate->product_id;
    snapshot.subtitle = engine::format(engine::Money{rate->amount_minor});
    snapshot.fields.push_back(text_field("rate.product", "record.rate.product",
                                         rate->product_id));
    snapshot.fields.push_back(text_field("rate.party", "record.rate.party",
                                         source_or_default(rate->party_id, "all customers")));
    snapshot.fields.push_back(money_field("rate.amount", "record.rate.amount",
                                          rate->amount_minor));
    snapshot.fields.push_back(text_field("rate.valid_from", "record.rate.valid_from",
                                         integer_text(rate->valid_from)));
    snapshot.fields.push_back(text_field("rate.valid_until", "record.rate.valid_until",
                                         integer_text(rate->valid_until)));
    push_history(snapshot.history, "rate.created", "record.history.created",
                 rate->created_by, rate->created_at);
    sort_history(snapshot.history);
    snapshot.actions.push_back(action("rate.set", "record.action.rate_set",
                                      protocol::OperationId::rate_set, rate->id));
    snapshot.actions.push_back(action("rate.remove", "record.action.rate_remove",
                                      protocol::OperationId::rate_remove, rate->id));
    return Result<RecordSnapshot, DomainError>::success(std::move(snapshot));
}

Result<RecordSnapshot, DomainError> orders_snapshot(const engine::Store& store,
                                                    std::string_view stable_id) {
    const auto order = orders::data::find_order(store, std::string(stable_id));
    if (!order) {
        return Result<RecordSnapshot, DomainError>::failure(
            failure(DomainErrorCode::NotFound, "record.error.not_found"));
    }
    const auto lines = orders::data::lines_for_order(store, order->id);
    const auto total = orders::order_total(lines);
    if (!total.ok) {
        return malformed("order.total");
    }

    RecordSnapshot snapshot;
    snapshot.stable_id = order->id;
    snapshot.title = order->id;
    snapshot.subtitle = orders::to_string(order->state);
    snapshot.fields.push_back(text_field("order.party", "record.order.party",
                                         source_or_default(order->party_id, "walk-in")));
    snapshot.fields.push_back(text_field("order.state", "record.order.state",
                                         orders::to_string(order->state)));
    snapshot.fields.push_back(text_field("order.promised_at", "record.order.promised_at",
                                         integer_text(order->promised_at)));
    snapshot.fields.push_back(text_field("order.source_quote", "record.order.source_quote",
                                         source_or_default(order->source_quotation_id,
                                                           "none")));
    snapshot.fields.push_back(text_field("order.source_revision", "record.order.source_revision",
                                         source_or_default(order->source_revision_id,
                                                           "none")));
    snapshot.fields.push_back(text_field("order.note", "record.order.note",
                                         source_or_default(order->note, "none")));
    snapshot.fields.push_back(money_field("order.total", "record.order.total",
                                          total.value.minor));
    for (const auto& line : lines) {
        if (snapshot.lines.size() >= kLineLimit) {
            break;
        }
        const auto amount = orders::line_amount(line);
        if (!amount.ok) {
            return malformed("order.lines");
        }
        snapshot.lines.push_back({"line." + line.id,
                                  source_or_default(line.description,
                                                    source_or_default(line.product_id,
                                                                      "order line")),
                                  source_or_default(line.price_reason,
                                                    pricing::to_string(line.price_source)),
                                  engine::format(engine::Quantity{line.quantity_scaled}),
                                  engine::format(amount.value),
                                  line.quantity_scaled, amount.value.minor});
    }
    push_history(snapshot.history, "order.created", "record.history.created",
                 order->created_by, order->created_at);
    push_history(snapshot.history, "order.cancelled", "record.history.cancelled",
                 order->cancel_reason, order->cancelled_at);
    sort_history(snapshot.history);
    snapshot.actions.push_back(action("order.update", "record.action.order_update",
                                      protocol::OperationId::order_update, order->id));
    snapshot.actions.push_back(action("order.line_add", "record.action.order_line_add",
                                      protocol::OperationId::order_line_add,
                                      order->id));
    snapshot.actions.push_back(action("order.to_jobs", "record.action.order_to_jobs",
                                      protocol::OperationId::order_to_jobs, order->id));
    if (order->state == orders::OrderState::Open) {
        snapshot.actions.push_back(action("order.cancel", "record.action.order_cancel",
                                          protocol::OperationId::order_cancel,
                                          order->id));
    }
    return Result<RecordSnapshot, DomainError>::success(std::move(snapshot));
}

Result<RecordSnapshot, DomainError> receivables_snapshot(const engine::Store& store,
                                                         std::string_view stable_id) {
    const auto invoice = receivables::data::find_invoice(store, std::string(stable_id));
    if (!invoice) {
        return Result<RecordSnapshot, DomainError>::failure(
            failure(DomainErrorCode::NotFound, "record.error.not_found"));
    }
    const auto lines = receivables::data::lines_for_invoice(store, invoice->id);
    const auto total = receivables::invoice_total(lines);
    const auto outstanding = receivables::data::outstanding_for_invoice(store, invoice->id);
    if (!total.ok || !outstanding.ok) {
        return malformed("invoice.total");
    }

    RecordSnapshot snapshot;
    snapshot.stable_id = invoice->id;
    snapshot.title = invoice_title(*invoice);
    snapshot.subtitle = std::string(engine::to_string(invoice->state));
    snapshot.fields.push_back(text_field("invoice.party", "record.invoice.party",
                                         source_or_default(invoice->party_id, "walk-in")));
    snapshot.fields.push_back(text_field("invoice.state", "record.invoice.state",
                                         std::string(engine::to_string(invoice->state))));
    snapshot.fields.push_back(text_field("invoice.due_at", "record.invoice.due_at",
                                         integer_text(invoice->due_at)));
    snapshot.fields.push_back(text_field("invoice.note", "record.invoice.note",
                                         source_or_default(invoice->note, "none")));
    snapshot.fields.push_back(money_field("invoice.total", "record.invoice.total",
                                          total.value.minor));
    snapshot.fields.push_back(money_field("invoice.outstanding",
                                          "record.invoice.outstanding",
                                          outstanding.value.minor));
    snapshot.fields.push_back(text_field("invoice.replaces", "record.invoice.replaces",
                                         source_or_default(invoice->replaces_invoice_id,
                                                           "none")));
    snapshot.fields.push_back(text_field("invoice.replacement",
                                         "record.invoice.replacement",
                                         source_or_default(invoice->replacement_invoice_id,
                                                           "none")));
    for (const auto& line : lines) {
        if (snapshot.lines.size() >= kLineLimit) {
            break;
        }
        const auto amount = receivables::calculate_amount(line);
        if (!amount.ok || amount.value.minor != line.amount_minor) {
            return malformed("invoice.lines");
        }
        snapshot.lines.push_back({"line." + line.id,
                                  line.description,
                                  source_or_default(line.rate_reason,
                                                    std::string(engine::to_string(
                                                        line.rate_origin))),
                                  engine::format(engine::Quantity{line.quantity_scaled}),
                                  engine::format(amount.value),
                                  line.quantity_scaled, amount.value.minor});
    }
    push_history(snapshot.history, "invoice.created", "record.history.created",
                 invoice->created_by, invoice->created_at);
    push_history(snapshot.history, "invoice.issued", "record.history.issued",
                 invoice->issued_by, invoice->issued_at);
    push_history(snapshot.history, "invoice.cancelled", "record.history.cancelled",
                 invoice->cancel_reason, invoice->cancelled_at);
    push_history(snapshot.history, "invoice.discarded", "record.history.discarded",
                 invoice->discarded_by, invoice->discarded_at);
    sort_history(snapshot.history);
    if (invoice->state == engine::DocumentState::Draft) {
        snapshot.actions.push_back(action("invoice.update",
                                          "record.action.invoice_draft_update",
                                          protocol::OperationId::invoice_draft_update,
                                          invoice->id));
        snapshot.actions.push_back(action("invoice.discard",
                                          "record.action.invoice_draft_discard",
                                          protocol::OperationId::invoice_draft_discard,
                                          invoice->id));
        snapshot.actions.push_back(action("invoice.issue",
                                          "record.action.issue_invoice",
                                          protocol::OperationId::issue_invoice,
                                          invoice->id));
    }
    if (invoice->state != engine::DocumentState::Discarded) {
        snapshot.actions.push_back(action("invoice.allocate",
                                          "record.action.payment_allocate",
                                          protocol::OperationId::payment_allocate,
                                          invoice->id));
        snapshot.actions.push_back(action("invoice.print",
                                          "record.action.document_print",
                                          protocol::OperationId::document_print,
                                          invoice->id));
    }
    return Result<RecordSnapshot, DomainError>::success(std::move(snapshot));
}

Result<RecordSnapshot, DomainError> jobs_snapshot(const engine::Store& store,
                                                  std::string_view stable_id) {
    const auto job = jobs::data::find_job(store, std::string(stable_id));
    if (!job) {
        return Result<RecordSnapshot, DomainError>::failure(
            failure(DomainErrorCode::NotFound, "record.error.not_found"));
    }
    const auto total = jobs::job_total(*job);
    if (!total.ok || total.value.minor != job->total_price_minor) {
        return malformed("job.total");
    }
    RecordSnapshot snapshot;
    snapshot.stable_id = job->id;
    snapshot.title = job_title(*job);
    snapshot.subtitle = jobs::to_string(job->state);
    snapshot.fields.push_back(text_field("job.party", "record.job.party",
                                         source_or_default(job->party_id, "walk-in")));
    snapshot.fields.push_back(text_field("job.state", "record.job.state",
                                         jobs::to_string(job->state)));
    snapshot.fields.push_back(text_field("job.title", "record.job.title",
                                         source_or_default(job->title, "none")));
    snapshot.fields.push_back(text_field("job.description", "record.job.description",
                                         source_or_default(job->description, "none")));
    snapshot.fields.push_back(text_field("job.specifications",
                                         "record.job.specifications",
                                         source_or_default(job->specifications, "none")));
    snapshot.fields.push_back(quantity_field("job.quantity", "record.job.quantity",
                                             job->quantity_scaled));
    snapshot.fields.push_back(money_field("job.unit_price",
                                          "record.job.unit_price",
                                          job->unit_price_minor));
    snapshot.fields.push_back(money_field("job.total", "record.job.total",
                                          job->total_price_minor));
    snapshot.fields.push_back(text_field("job.material_reference",
                                         "record.job.material_reference",
                                         source_or_default(job->material_reference,
                                                           "none")));
    push_history(snapshot.history, "job.created", "record.history.created",
                 job->created_by, job->created_at);
    push_history(snapshot.history, "job.started", "record.history.started",
                 job->started_by, job->started_at);
    push_history(snapshot.history, "job.done", "record.history.completed",
                 job->done_by, job->done_at);
    push_history(snapshot.history, "job.cancelled", "record.history.cancelled",
                 job->cancel_reason, job->cancelled_at);
    sort_history(snapshot.history);
    snapshot.actions.push_back(action("job.update", "record.action.job_update",
                                      protocol::OperationId::job_update, job->id));
    snapshot.actions.push_back(action("job.state_change",
                                      "record.action.job_state_change",
                                      protocol::OperationId::job_state_change,
                                      job->id));
    if (job->state != jobs::JobState::Cancelled) {
        snapshot.actions.push_back(action("job.cancel", "record.action.job_cancel",
                                          protocol::OperationId::job_cancel, job->id));
    }
    return Result<RecordSnapshot, DomainError>::success(std::move(snapshot));
}

Result<RecordSnapshot, DomainError> quotations_snapshot(const engine::Store& store,
                                                        std::string_view stable_id) {
    const auto quotation = quotations::data::find_quotation(store, std::string(stable_id));
    if (!quotation) {
        return Result<RecordSnapshot, DomainError>::failure(
            failure(DomainErrorCode::NotFound, "record.error.not_found"));
    }
    const auto revision = quotations::data::latest_revision(store, quotation->id);
    if (!revision) {
        return malformed("quotation.revision");
    }
    const auto lines = quotations::data::lines_for_revision(store, revision->id);
    const auto total = quotations::revision_total(lines);
    if (!total.ok) {
        return malformed("quotation.total");
    }

    RecordSnapshot snapshot;
    snapshot.stable_id = quotation->id;
    snapshot.title = quotation_title(*quotation, revision);
    snapshot.subtitle = quotations::to_string(quotation->state);
    snapshot.fields.push_back(text_field("quotation.state", "record.quotation.state",
                                         quotations::to_string(quotation->state)));
    snapshot.fields.push_back(text_field("quotation.party", "record.quotation.party",
                                         source_or_default(quotation->party_id,
                                                           "walk-in")));
    snapshot.fields.push_back(text_field("quotation.current_revision",
                                         "record.quotation.current_revision",
                                         integer_text(quotation->current_revision)));
    snapshot.fields.push_back(text_field("quotation.accepted_revision",
                                         "record.quotation.accepted_revision",
                                         integer_text(quotation->accepted_revision)));
    snapshot.fields.push_back(text_field("quotation.reference",
                                         "record.quotation.reference",
                                         source_or_default(quotation->customer_reference,
                                                           "none")));
    snapshot.fields.push_back(text_field("quotation.valid_until",
                                         "record.quotation.valid_until",
                                         integer_text(revision->valid_until)));
    snapshot.fields.push_back(text_field("quotation.terms", "record.quotation.terms",
                                         source_or_default(revision->terms, "none")));
    snapshot.fields.push_back(money_field("quotation.total", "record.quotation.total",
                                          total.value.minor));
    for (const auto& line : lines) {
        if (snapshot.lines.size() >= kLineLimit) {
            break;
        }
        const auto amount = quotations::line_amount(line);
        if (!amount.ok || amount.value.minor != line.amount_minor) {
            return malformed("quotation.lines");
        }
        snapshot.lines.push_back({"line." + line.id,
                                  line.description,
                                  source_or_default(line.rate_reason,
                                                    std::string(engine::to_string(
                                                        line.rate_origin))),
                                  engine::format(engine::Quantity{line.quantity_scaled}),
                                  engine::format(amount.value),
                                  line.quantity_scaled, amount.value.minor});
    }
    push_history(snapshot.history, "quotation.created", "record.history.created",
                 quotation->created_by, quotation->created_at);
    for (const auto& item : quotations::data::revisions_for_quotation(store, quotation->id)) {
        push_history(snapshot.history, "revision." + item.id,
                     item.issued ? "record.history.issued" : "record.history.revised",
                     "revision " + std::to_string(item.revision), item.created_at);
    }
    push_history(snapshot.history, "quotation.accepted", "record.history.accepted",
                 quotation->accepted_by, quotation->accepted_at);
    push_history(snapshot.history, "quotation.expired", "record.history.expired",
                 quotation->expiry_reason, quotation->expired_at);
    sort_history(snapshot.history);
    if (quotations::can_revise(quotation->state)) {
        snapshot.actions.push_back(action("quotation.revise",
                                          "record.action.quotation_revise",
                                          protocol::OperationId::quotation_revise,
                                          quotation->id));
    }
    if (!revision->issued && quotation->state == quotations::QuotationState::Draft) {
        snapshot.actions.push_back(action("quotation.issue",
                                          "record.action.quotation_issue",
                                          protocol::OperationId::quotation_issue,
                                          quotation->id));
    }
    if (quotation->state == quotations::QuotationState::Issued) {
        snapshot.actions.push_back(action("quotation.accept",
                                          "record.action.quotation_accept",
                                          protocol::OperationId::quotation_accept,
                                          quotation->id));
        snapshot.actions.push_back(action("quotation.expire",
                                          "record.action.quotation_expire",
                                          protocol::OperationId::quotation_expire,
                                          quotation->id));
    }
    if (quotation->state == quotations::QuotationState::Accepted) {
        snapshot.actions.push_back(action("quotation.to_order",
                                          "record.action.quote_to_order",
                                          protocol::OperationId::quote_to_order,
                                          quotation->id));
    }
    return Result<RecordSnapshot, DomainError>::success(std::move(snapshot));
}

Result<RecordSnapshot, DomainError> agreements_snapshot(const engine::Store& store,
                                                        std::string_view stable_id) {
    const auto agreement = agreements::data::find_agreement(store, std::string(stable_id));
    if (!agreement) {
        return Result<RecordSnapshot, DomainError>::failure(
            failure(DomainErrorCode::NotFound, "record.error.not_found"));
    }
    const auto lines = agreements::data::lines_for_agreement(store, agreement->id);
    std::int64_t capped_total = 0;
    for (const auto& line : lines) {
        const auto capped = agreements::capped_value(line);
        if (!capped.ok) {
            return malformed("agreement.total");
        }
        const auto next = engine::money_add(engine::Money{capped_total}, capped.value);
        if (!next.ok) {
            return malformed("agreement.total");
        }
        capped_total = next.value.minor;
    }

    RecordSnapshot snapshot;
    snapshot.stable_id = agreement->id;
    snapshot.title = agreement_title(*agreement);
    snapshot.subtitle = agreements::to_string(agreement->state);
    snapshot.fields.push_back(text_field("agreement.party", "record.agreement.party",
                                         agreement->party_id));
    snapshot.fields.push_back(text_field("agreement.state", "record.agreement.state",
                                         agreements::to_string(agreement->state)));
    snapshot.fields.push_back(text_field("agreement.valid_from",
                                         "record.agreement.valid_from",
                                         integer_text(agreement->valid_from)));
    snapshot.fields.push_back(text_field("agreement.valid_until",
                                         "record.agreement.valid_until",
                                         integer_text(agreement->valid_until)));
    snapshot.fields.push_back(text_field("agreement.fallback", "record.agreement.fallback",
                                         agreements::to_string(agreement->fallback)));
    snapshot.fields.push_back(text_field("agreement.reference",
                                         "record.agreement.reference",
                                         source_or_default(agreement->customer_reference,
                                                           "none")));
    snapshot.fields.push_back(text_field("agreement.terms", "record.agreement.terms",
                                         source_or_default(agreement->terms, "none")));
    snapshot.fields.push_back(text_field("agreement.close_effect",
                                         "record.agreement.close_effect",
                                         agreements::to_string(agreement->close_effect)));
    snapshot.fields.push_back(money_field("agreement.capped_total",
                                          "record.agreement.capped_total",
                                          capped_total));
    for (const auto& line : lines) {
        if (snapshot.lines.size() >= kLineLimit) {
            break;
        }
        const auto cap = agreements::cap_state(line);
        snapshot.lines.push_back({"line." + line.id,
                                  line.agreed_name,
                                  source_or_default(line.specifications,
                                                    source_or_default(line.product_id,
                                                                      "agreement line")),
                                  cap.capped ? engine::format(engine::Quantity{cap.remaining_scaled})
                                             : std::string{"uncapped"},
                                  engine::format(engine::Money{line.rate_minor}),
                                  cap.capped ? std::optional<std::int64_t>{cap.remaining_scaled}
                                             : std::nullopt,
                                  line.rate_minor});
    }
    push_history(snapshot.history, "agreement.created", "record.history.created",
                 agreement->created_by, agreement->created_at);
    push_history(snapshot.history, "agreement.opened", "record.history.opened",
                 agreement->opened_by, agreement->opened_at);
    push_history(snapshot.history, "agreement.closed", "record.history.closed",
                 agreement->close_reason, agreement->closed_at);
    push_history(snapshot.history, "agreement.reopened", "record.history.reopened",
                 agreement->reopen_reason, agreement->reopened_at);
    sort_history(snapshot.history);
    if (agreements::can_amend(agreement->state)) {
        snapshot.actions.push_back(action("agreement.update",
                                          "record.action.agreement_update",
                                          protocol::OperationId::agreement_update,
                                          agreement->id));
    }
    if (agreement->state == agreements::AgreementState::Open) {
        snapshot.actions.push_back(action("agreement.close",
                                          "record.action.agreement_close",
                                          protocol::OperationId::agreement_close,
                                          agreement->id));
    }
    if (agreement->state == agreements::AgreementState::Closed) {
        snapshot.actions.push_back(action("agreement.reopen",
                                          "record.action.agreement_reopen",
                                          protocol::OperationId::agreement_reopen,
                                          agreement->id));
    }
    return Result<RecordSnapshot, DomainError>::success(std::move(snapshot));
}

Result<RecordSnapshot, DomainError> sourcing_snapshot(const engine::Store& store,
                                                      std::string_view stable_id) {
    const auto supplier = sourcing::data::find_supplier(store, std::string(stable_id));
    if (!supplier) {
        return Result<RecordSnapshot, DomainError>::failure(
            failure(DomainErrorCode::NotFound, "record.error.not_found"));
    }
    RecordSnapshot snapshot;
    snapshot.stable_id = supplier->id;
    snapshot.title = "Supplier " + short_id(supplier->id);
    snapshot.subtitle = sourcing::to_string(supplier->kind);
    snapshot.fields.push_back(text_field("supplier.kind", "record.supplier.kind",
                                         sourcing::to_string(supplier->kind)));
    snapshot.fields.push_back(text_field("supplier.supplies",
                                         "record.supplier.supplies",
                                         source_or_default(supplier->supplies, "none")));
    snapshot.fields.push_back(text_field("supplier.reliability",
                                         "record.supplier.reliability",
                                         source_or_default(supplier->reliability_notes,
                                                           "none")));
    snapshot.fields.push_back(text_field("supplier.lead_time_days",
                                         "record.supplier.lead_time_days",
                                         integer_text(supplier->lead_time_days)));
    snapshot.fields.push_back(text_field("supplier.notes", "record.supplier.notes",
                                         source_or_default(supplier->sourcing_notes,
                                                           "none")));
    for (const auto& purchase : sourcing::data::purchases_for_supplier(store, supplier->id,
                                                                       kLineLimit)) {
        const auto material = sourcing::data::find_material(store, purchase.material_id);
        snapshot.lines.push_back({"purchase." + purchase.id,
                                  material ? material->name : purchase.material_id,
                                  sourcing::to_string(purchase.state),
                                  engine::format(engine::Quantity{purchase.quantity_scaled}),
                                  engine::format(engine::Money{purchase.total_cost_minor}),
                                  purchase.quantity_scaled,
                                  purchase.total_cost_minor});
    }
    push_history(snapshot.history, "supplier.created", "record.history.created",
                 supplier->created_by, supplier->created_at);
    push_history(snapshot.history, "supplier.updated", "record.history.updated",
                 supplier->updated_by, supplier->updated_at);
    sort_history(snapshot.history);
    snapshot.actions.push_back(action("supplier.update",
                                      "record.action.supplier_update",
                                      protocol::OperationId::supplier_update,
                                      supplier->id));
    snapshot.actions.push_back(action("supplier.lookup",
                                      "record.action.purchase_lookup",
                                      protocol::OperationId::purchase_lookup,
                                      supplier->id));
    return Result<RecordSnapshot, DomainError>::success(std::move(snapshot));
}

Result<RecordSnapshot, DomainError> companion_snapshot(const engine::Store& store,
                                                       std::string_view stable_id) {
    const auto task = companion::data::find_task(store, std::string(stable_id));
    if (!task) {
        return Result<RecordSnapshot, DomainError>::failure(
            failure(DomainErrorCode::NotFound, "record.error.not_found"));
    }
    RecordSnapshot snapshot;
    snapshot.stable_id = task->id;
    snapshot.title = task->title;
    snapshot.subtitle = companion::to_string(task->state);
    snapshot.fields.push_back(text_field("task.kind", "record.task.kind",
                                         companion::to_string(task->kind)));
    snapshot.fields.push_back(text_field("task.state", "record.task.state",
                                         companion::to_string(task->state)));
    snapshot.fields.push_back(text_field("task.note", "record.task.note",
                                         source_or_default(task->note, "none")));
    snapshot.fields.push_back(text_field("task.target_module",
                                         "record.task.target_module",
                                         task->target.is_valid()
                                             ? std::string(protocol::module_name(task->target.module))
                                             : std::string{"none"}));
    snapshot.fields.push_back(text_field("task.target_record",
                                         "record.task.target_record",
                                         task->target.is_valid()
                                             ? engine::to_string(task->target.record)
                                             : std::string{"none"}));
    snapshot.fields.push_back(text_field("task.source_key", "record.task.source_key",
                                         source_or_default(task->source_key, "none")));
    snapshot.fields.push_back(text_field("task.due_at", "record.task.due_at",
                                         integer_text(task->due_at)));
    snapshot.fields.push_back(text_field("task.snoozed_until",
                                         "record.task.snoozed_until",
                                         integer_text(task->snoozed_until)));
    snapshot.fields.push_back(text_field("task.recurrence", "record.task.recurrence",
                                         companion::to_string(task->recurrence_unit)));
    snapshot.fields.push_back(text_field("task.recurrence_interval",
                                         "record.task.recurrence_interval",
                                         integer_text(task->recurrence_interval)));
    for (const auto& item : companion::data::events_for_task(store, task->id)) {
        if (snapshot.history.size() >= kHistoryLimit) {
            break;
        }
        snapshot.history.push_back({"event." + item.id,
                                    "record.history.task_event",
                                    companion::to_string(item.kind),
                                    item.happened_at});
    }
    sort_history(snapshot.history);
    if (task->state == companion::TaskState::Open) {
        snapshot.actions.push_back(action("task.update", "record.action.task_update",
                                          protocol::OperationId::task_update,
                                          task->id));
        snapshot.actions.push_back(action("task.complete",
                                          "record.action.task_complete",
                                          protocol::OperationId::task_complete,
                                          task->id));
        snapshot.actions.push_back(action("task.snooze", "record.action.task_snooze",
                                          protocol::OperationId::task_snooze,
                                          task->id));
    }
    return Result<RecordSnapshot, DomainError>::success(std::move(snapshot));
}

Result<RecordSnapshot, DomainError> files_snapshot(const engine::Store& store,
                                                   std::string_view stable_id) {
    const auto asset = files::data::find_asset(store, std::string(stable_id));
    if (!asset) {
        return Result<RecordSnapshot, DomainError>::failure(
            failure(DomainErrorCode::NotFound, "record.error.not_found"));
    }
    const auto locations = files::data::locations_for_asset(store, asset->id);
    const auto links = files::data::links_for_asset(store, asset->id);
    RecordSnapshot snapshot;
    snapshot.stable_id = asset->id;
    snapshot.title = file_title(*asset);
    snapshot.subtitle = source_or_default(asset->media_type, "file");
    snapshot.fields.push_back(text_field("file.hash", "record.file.hash",
                                         asset->content_hash));
    snapshot.fields.push_back(money_field("file.size", "record.file.size_minor_units",
                                          asset->size_bytes));
    snapshot.fields.push_back(text_field("file.extension", "record.file.extension",
                                         source_or_default(asset->extension, "none")));
    snapshot.fields.push_back(text_field("file.media_type", "record.file.media_type",
                                         source_or_default(asset->media_type, "none")));
    snapshot.fields.push_back(text_field("file.forgotten", "record.file.forgotten",
                                         bool_text(asset->forgotten)));
    snapshot.fields.push_back(text_field("file.duplicates", "record.file.location_count",
                                         integer_text(static_cast<std::int64_t>(locations.size()))));
    if (asset->forgotten) {
        snapshot.fields.push_back(text_field("file.forget_reason",
                                             "record.file.forget_reason",
                                             asset->forget_reason));
    }
    std::size_t index = 0;
    for (const auto& location : locations) {
        if (snapshot.lines.size() >= kLineLimit) {
            break;
        }
        snapshot.lines.push_back({"location." + std::to_string(index++),
                                  location.presence == files::Presence::Present ? "present copy"
                                                                                : "missing copy",
                                  "observed " + integer_text(location.observed_at),
                                  integer_text(location.modified_at), {}, std::nullopt,
                                  std::nullopt});
    }
    for (const auto& link : links) {
        if (snapshot.lines.size() >= kLineLimit) {
            break;
        }
        snapshot.lines.push_back({"link." + link.id,
                                  source_or_default(link.role, "linked"),
                                  std::string(protocol::module_name(link.target.module)) +
                                      " " + engine::to_string(link.target.record),
                                  {}, {}, std::nullopt, std::nullopt});
    }
    push_history(snapshot.history, "file.created", "record.history.created",
                 asset->created_by, asset->created_at);
    push_history(snapshot.history, "file.forgotten", "record.history.forgotten",
                 asset->forget_reason, asset->forgotten_at);
    sort_history(snapshot.history);
    snapshot.actions.push_back(action("file.link", "record.action.file_link",
                                      protocol::OperationId::file_link, asset->id));
    if (!asset->forgotten) {
        snapshot.actions.push_back(action("file.forget", "record.action.file_forget",
                                          protocol::OperationId::file_forget,
                                          asset->id));
    }
    return Result<RecordSnapshot, DomainError>::success(std::move(snapshot));
}

}  // namespace

Result<RecordSnapshot, DomainError> LocalRecordQuery::load(PageKind kind,
                                                           std::string_view stable_id) {
    if (!is_valid(kind)) {
        return Result<RecordSnapshot, DomainError>::failure(
            failure(DomainErrorCode::ValidationFailed,
                    "primary.error.invalid_page_kind", "kind"));
    }
    if (stable_id.size() != 32 || !engine::record_id_from_string(stable_id).is_valid()) {
        return Result<RecordSnapshot, DomainError>::failure(
            failure(DomainErrorCode::ValidationFailed,
                    "record.error.invalid_record_id", "record_id"));
    }
    if (!database_.ready()) {
        return Result<RecordSnapshot, DomainError>::failure(
            failure(DomainErrorCode::InvalidContext,
                    "record.error.database_unavailable"));
    }

    try {
        Result<RecordSnapshot, DomainError> result =
            Result<RecordSnapshot, DomainError>::failure(
                failure(DomainErrorCode::ValidationFailed,
                        "record.error.invalid_snapshot"));
        database_.read([&](const engine::Store& store) {
            switch (kind) {
                case PageKind::Administration:
                    result = administration_snapshot(store, stable_id);
                    return;
                case PageKind::Parties:
                    result = parties_snapshot(store, stable_id);
                    return;
                case PageKind::Catalog:
                    result = catalog_snapshot(store, stable_id);
                    return;
                case PageKind::Pricing:
                    result = pricing_snapshot(store, stable_id);
                    return;
                case PageKind::Orders:
                    result = orders_snapshot(store, stable_id);
                    return;
                case PageKind::Receivables:
                    result = receivables_snapshot(store, stable_id);
                    return;
                case PageKind::Jobs:
                    result = jobs_snapshot(store, stable_id);
                    return;
                case PageKind::Quotations:
                    result = quotations_snapshot(store, stable_id);
                    return;
                case PageKind::Agreements:
                    result = agreements_snapshot(store, stable_id);
                    return;
                case PageKind::Sourcing:
                    result = sourcing_snapshot(store, stable_id);
                    return;
                case PageKind::Companion:
                    result = companion_snapshot(store, stable_id);
                    return;
                case PageKind::Files:
                    result = files_snapshot(store, stable_id);
                    return;
                case PageKind::Count:
                    result = Result<RecordSnapshot, DomainError>::failure(
                        failure(DomainErrorCode::ValidationFailed,
                                "primary.error.invalid_page_kind", "kind"));
                    return;
            }
        });
        return result;
    } catch (const std::exception&) {
        return Result<RecordSnapshot, DomainError>::failure(
            failure(DomainErrorCode::InvalidContext,
                    "record.error.query_failed"));
    }
}

}  // namespace squiflow::app::primary
