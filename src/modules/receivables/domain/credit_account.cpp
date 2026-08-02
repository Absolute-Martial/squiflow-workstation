#include "modules/receivables/domain/credit_account.hpp"

#include <limits>

#include <squiflow/protocol/module_id.hpp>

#include "engine/records/money.hpp"
#include "modules/context.hpp"

namespace squiflow::modules::receivables {
namespace {

constexpr std::int64_t kMillisecondsPerDay = 86'400'000;

bool blank(const std::string& text) noexcept {
    for (const char c : text) {
        if (static_cast<unsigned char>(c) > ' ') {
            return false;
        }
    }
    return true;
}

std::int32_t period_from(const engine::Row& row) noexcept {
    const std::int64_t stored = row.get("credit_period_days").integer_or(-1);
    if (stored < 0 ||
        stored > static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())) {
        return -1;
    }
    return static_cast<std::int32_t>(stored);
}

std::uint8_t cycle_day_from(const engine::Row& row) noexcept {
    const std::int64_t stored = row.get("cycle_day").integer_or(0);
    if (stored < 1 || stored > 31) {
        return 0;
    }
    return static_cast<std::uint8_t>(stored);
}

protocol::ModuleId target_module_from(const engine::Row& row) noexcept {
    const std::int64_t stored = row.get("target_module").integer_or(-1);
    if (stored == static_cast<std::int64_t>(protocol::ModuleId::jobs)) {
        return protocol::ModuleId::jobs;
    }
    return protocol::ModuleId::Count;
}

}  // namespace

void validate(const CreditAccount& account) {
    if (blank(account.id)) {
        throw RuleViolation("This credit account has no record to be saved under.");
    }
    if (blank(account.party_id)) {
        throw RuleViolation("A credit account must belong to an organization.");
    }
    if (account.credit_limit_minor < 0) {
        throw RuleViolation("A credit limit cannot be negative.");
    }
    if (account.credit_period_days < 0) {
        throw RuleViolation("A credit period cannot be negative.");
    }
    if (account.cycle_day < 1 || account.cycle_day > 31) {
        throw RuleViolation("A billing cycle day must be from 1 through 31.");
    }
    if (account.updated_at <= 0 || blank(account.updated_by)) {
        throw RuleViolation("Credit terms must record when and by whom they were set.");
    }
}

void validate(const CreditExposureItem& item) {
    if (blank(item.id)) {
        throw RuleViolation("A credit exposure item must identify its source.");
    }
    if (item.amount_minor < 0) {
        throw RuleViolation("Credit exposure cannot contain negative money.");
    }
    if (item.due_at < 0) {
        throw RuleViolation("An exposure due time cannot be negative.");
    }
}

CreditEvaluationResult evaluate_credit(
    const CreditAccount& account,
    const std::vector<CreditExposureItem>& exposure,
    std::int64_t proposed_minor,
    std::int64_t as_of) noexcept {
    try {
        validate(account);
    } catch (...) {
        return {};
    }
    if (proposed_minor < 0 || as_of <= 0) {
        return {};
    }

    engine::Money running{0};
    CreditEvaluation evaluation;
    evaluation.proposed_minor = proposed_minor;

    for (const CreditExposureItem& item : exposure) {
        try {
            validate(item);
        } catch (...) {
            return {};
        }

        const engine::MoneyResult sum = engine::money_add(
            running, engine::Money{item.amount_minor});
        if (!sum.ok) {
            return {};
        }
        running = sum.value;

        if (item.amount_minor > 0 && item.due_at > 0 && as_of > item.due_at) {
            evaluation.over_period = true;
            if (evaluation.oldest_overdue_at == 0 ||
                item.due_at < evaluation.oldest_overdue_at) {
                evaluation.oldest_overdue_at = item.due_at;
            }
        }
    }

    evaluation.exposure_minor = running.minor;
    const engine::MoneyResult projected = engine::money_add(
        running, engine::Money{proposed_minor});
    if (!projected.ok) {
        return {};
    }
    evaluation.projected_minor = projected.value.minor;
    evaluation.over_limit = evaluation.projected_minor > account.credit_limit_minor;
    evaluation.hold = evaluation.over_limit || evaluation.over_period;
    return {true, evaluation};
}

DueAtResult credit_due_at(std::int64_t issued_at,
                          std::int32_t credit_period_days) noexcept {
    if (issued_at <= 0 || credit_period_days < 0) {
        return {};
    }
    const std::int64_t days = static_cast<std::int64_t>(credit_period_days);
    if (days > std::numeric_limits<std::int64_t>::max() /
                   kMillisecondsPerDay) {
        return {};
    }
    const std::int64_t offset = days * kMillisecondsPerDay;
    if (issued_at > std::numeric_limits<std::int64_t>::max() - offset) {
        return {};
    }
    return {true, issued_at + offset};
}

void validate(const CreditOverride& override_evidence) {
    if (blank(override_evidence.id)) {
        throw RuleViolation("This credit override has no record to be saved under.");
    }
    if (blank(override_evidence.party_id)) {
        throw RuleViolation("A credit override must name the customer.");
    }
    if (override_evidence.target_job.module != protocol::ModuleId::jobs ||
        !override_evidence.target_job.record.is_valid()) {
        throw RuleViolation("A credit override must name the job it authorized.");
    }
    if (override_evidence.exposure_minor < 0 ||
        override_evidence.proposed_minor <= 0 ||
        override_evidence.credit_limit_minor < 0) {
        throw RuleViolation("Credit override amounts must be positive, usable evidence.");
    }
    if (override_evidence.oldest_overdue_at < 0) {
        throw RuleViolation("A credit override has an invalid overdue time.");
    }
    if (override_evidence.authorized_at <= 0 ||
        blank(override_evidence.authorized_by) ||
        blank(override_evidence.reason)) {
        throw RuleViolation(
            "A credit override must retain who authorized it, when, and why.");
    }

    const engine::MoneyResult projected = engine::money_add(
        engine::Money{override_evidence.exposure_minor},
        engine::Money{override_evidence.proposed_minor});
    if (!projected.ok) {
        throw RuleViolation("That credit override exposure is too large to record.");
    }
    const bool should_be_over_limit =
        projected.value.minor > override_evidence.credit_limit_minor;
    const bool should_be_over_period =
        override_evidence.oldest_overdue_at > 0 &&
        override_evidence.authorized_at > override_evidence.oldest_overdue_at;

    if (override_evidence.over_limit != should_be_over_limit ||
        override_evidence.over_period != should_be_over_period) {
        throw RuleViolation("That credit override contradicts its retained decision facts.");
    }
    if (!override_evidence.over_limit && !override_evidence.over_period) {
        throw RuleViolation("A safe credit decision cannot have a hold override.");
    }
}

engine::Row to_row(const CreditAccount& account) {
    engine::Row row;
    row.set("id", engine::Value::text(account.id));
    row.set("party_id", engine::Value::text(account.party_id));
    row.set("credit_limit_minor", engine::Value::integer(account.credit_limit_minor));
    row.set("credit_period_days",
            engine::Value::integer(static_cast<std::int64_t>(account.credit_period_days)));
    row.set("cycle_day",
            engine::Value::integer(static_cast<std::int64_t>(account.cycle_day)));
    row.set("updated_at", engine::Value::integer(account.updated_at));
    row.set("updated_by", engine::Value::text(account.updated_by));
    return row;
}

CreditAccount credit_account_from_row(const engine::Row& row) {
    CreditAccount account;
    account.id = row.get("id").text_or({});
    account.party_id = row.get("party_id").text_or({});
    account.credit_limit_minor = row.get("credit_limit_minor").integer_or(-1);
    account.credit_period_days = period_from(row);
    account.cycle_day = cycle_day_from(row);
    account.updated_at = row.get("updated_at").integer_or(0);
    account.updated_by = row.get("updated_by").text_or({});
    return account;
}

engine::Row to_row(const CreditOverride& override_evidence) {
    engine::Row row;
    row.set("id", engine::Value::text(override_evidence.id));
    row.set("party_id", engine::Value::text(override_evidence.party_id));
    row.set("target_module",
            engine::Value::integer(
                static_cast<std::int64_t>(override_evidence.target_job.module)));
    row.set("target_record_id",
            engine::Value::text(engine::to_string(override_evidence.target_job.record)));
    row.set("exposure_minor",
            engine::Value::integer(override_evidence.exposure_minor));
    row.set("proposed_minor",
            engine::Value::integer(override_evidence.proposed_minor));
    row.set("credit_limit_minor",
            engine::Value::integer(override_evidence.credit_limit_minor));
    row.set("oldest_overdue_at",
            engine::Value::integer(override_evidence.oldest_overdue_at));
    row.set("over_limit", engine::Value::boolean(override_evidence.over_limit));
    row.set("over_period", engine::Value::boolean(override_evidence.over_period));
    row.set("reason", engine::Value::text(override_evidence.reason));
    row.set("authorized_at",
            engine::Value::integer(override_evidence.authorized_at));
    row.set("authorized_by",
            engine::Value::text(override_evidence.authorized_by));
    return row;
}

CreditOverride credit_override_from_row(const engine::Row& row) {
    CreditOverride override_evidence;
    override_evidence.id = row.get("id").text_or({});
    override_evidence.party_id = row.get("party_id").text_or({});
    override_evidence.target_job.module = target_module_from(row);
    override_evidence.target_job.record = engine::record_id_from_string(
        row.get("target_record_id").text_or({}));
    override_evidence.exposure_minor = row.get("exposure_minor").integer_or(-1);
    override_evidence.proposed_minor = row.get("proposed_minor").integer_or(0);
    override_evidence.credit_limit_minor =
        row.get("credit_limit_minor").integer_or(-1);
    override_evidence.oldest_overdue_at =
        row.get("oldest_overdue_at").integer_or(-1);
    override_evidence.over_limit = row.get("over_limit").boolean_or(false);
    override_evidence.over_period = row.get("over_period").boolean_or(false);
    override_evidence.reason = row.get("reason").text_or({});
    override_evidence.authorized_at = row.get("authorized_at").integer_or(0);
    override_evidence.authorized_by = row.get("authorized_by").text_or({});
    return override_evidence;
}

}  // namespace squiflow::modules::receivables
