#include "engine/storage/store.hpp"

#include <algorithm>
#include <utility>

namespace squiflow::engine {
namespace {

// Ordering rank across kinds, so a column holding more than one kind still
// sorts deterministically instead of by whatever the container happened to do.
int rank_of(ValueKind kind) {
    switch (kind) {
        case ValueKind::Null:
            return 0;
        case ValueKind::Integer:
        case ValueKind::Real:
            return 1;
        case ValueKind::Text:
            return 2;
        case ValueKind::Binary:
            return 3;
    }
    return 0;
}

int sign_of(int difference) {
    if (difference < 0) {
        return -1;
    }
    if (difference > 0) {
        return 1;
    }
    return 0;
}

const Value& null_value() {
    static const Value value{};
    return value;
}

}  // namespace

// ---------------------------------------------------------------- Value

Value Value::null() {
    return Value{};
}

Value Value::integer(std::int64_t value) {
    Value result;
    result.data_ = value;
    return result;
}

Value Value::real(double value) {
    Value result;
    result.data_ = value;
    return result;
}

Value Value::text(std::string value) {
    Value result;
    result.data_ = std::move(value);
    return result;
}

Value Value::binary(Blob value) {
    Value result;
    result.data_ = std::move(value);
    return result;
}

// Stored as an integer rather than a dedicated kind. SQLite has no boolean,
// and inventing one here would mean the two implementations disagree.
Value Value::boolean(bool value) {
    return integer(value ? 1 : 0);
}

ValueKind Value::kind() const {
    switch (data_.index()) {
        case 0:
            return ValueKind::Null;
        case 1:
            return ValueKind::Integer;
        case 2:
            return ValueKind::Real;
        case 3:
            return ValueKind::Text;
        default:
            return ValueKind::Binary;
    }
}

bool Value::is_null() const {
    return data_.index() == 0;
}

std::optional<std::int64_t> Value::as_integer() const {
    if (const auto* value = std::get_if<std::int64_t>(&data_)) {
        return *value;
    }
    return std::nullopt;
}

std::optional<double> Value::as_real() const {
    if (const auto* value = std::get_if<double>(&data_)) {
        return *value;
    }
    if (const auto* value = std::get_if<std::int64_t>(&data_)) {
        return static_cast<double>(*value);
    }
    return std::nullopt;
}

const std::string* Value::as_text() const {
    return std::get_if<std::string>(&data_);
}

const Blob* Value::as_binary() const {
    return std::get_if<Blob>(&data_);
}

std::int64_t Value::integer_or(std::int64_t fallback) const {
    return as_integer().value_or(fallback);
}

std::string Value::text_or(std::string fallback) const {
    if (const auto* value = as_text()) {
        return *value;
    }
    return fallback;
}

bool Value::boolean_or(bool fallback) const {
    if (const auto value = as_integer()) {
        return *value != 0;
    }
    return fallback;
}

int Value::compare(const Value& other) const {
    const int own_rank = rank_of(kind());
    const int other_rank = rank_of(other.kind());
    if (own_rank != other_rank) {
        return own_rank < other_rank ? -1 : 1;
    }

    switch (kind()) {
        case ValueKind::Null:
            return 0;
        case ValueKind::Integer:
        case ValueKind::Real: {
            // Both sides are numeric here because the ranks matched. Integers
            // are compared as integers so that two values beyond the exact
            // range of a double do not compare equal.
            const auto own_integer = as_integer();
            const auto other_integer = other.as_integer();
            if (own_integer && other_integer) {
                if (*own_integer == *other_integer) {
                    return 0;
                }
                return *own_integer < *other_integer ? -1 : 1;
            }
            const double own_real = as_real().value_or(0.0);
            const double other_real = other.as_real().value_or(0.0);
            if (own_real == other_real) {
                return 0;
            }
            return own_real < other_real ? -1 : 1;
        }
        case ValueKind::Text:
            return sign_of(as_text()->compare(*other.as_text()));
        case ValueKind::Binary: {
            const Blob& own = *as_binary();
            const Blob& them = *other.as_binary();
            if (own == them) {
                return 0;
            }
            return own < them ? -1 : 1;
        }
    }
    return 0;
}

bool Value::operator==(const Value& other) const {
    if (rank_of(kind()) != rank_of(other.kind())) {
        return false;
    }
    return compare(other) == 0;
}

bool Value::operator!=(const Value& other) const {
    return !(*this == other);
}

std::string Value::describe() const {
    switch (kind()) {
        case ValueKind::Null:
            return "null";
        case ValueKind::Integer:
            return std::to_string(*as_integer());
        case ValueKind::Real:
            return std::to_string(*as_real());
        case ValueKind::Text:
            return *as_text();
        case ValueKind::Binary:
            return "binary(" + std::to_string(as_binary()->size()) + ")";
    }
    return "null";
}

// ------------------------------------------------------------------ Row

Row& Row::set(std::string column, Value value) {
    for (auto& field : fields_) {
        if (field.first == column) {
            field.second = std::move(value);
            return *this;
        }
    }
    fields_.emplace_back(std::move(column), std::move(value));
    return *this;
}

bool Row::has(const std::string& column) const {
    return std::any_of(fields_.begin(), fields_.end(),
                       [&column](const Field& field) { return field.first == column; });
}

const Value& Row::get(const std::string& column) const {
    for (const auto& field : fields_) {
        if (field.first == column) {
            return field.second;
        }
    }
    return null_value();
}

std::optional<Value> Row::lookup(const std::string& column) const {
    for (const auto& field : fields_) {
        if (field.first == column) {
            return field.second;
        }
    }
    return std::nullopt;
}

bool Row::erase(const std::string& column) {
    for (auto it = fields_.begin(); it != fields_.end(); ++it) {
        if (it->first == column) {
            fields_.erase(it);
            return true;
        }
    }
    return false;
}

void Row::merge(const Row& other) {
    for (const auto& field : other.fields_) {
        set(field.first, field.second);
    }
}

std::size_t Row::size() const {
    return fields_.size();
}

bool Row::empty() const {
    return fields_.empty();
}

const std::vector<Row::Field>& Row::fields() const {
    return fields_;
}

std::vector<std::string> Row::columns() const {
    std::vector<std::string> names;
    names.reserve(fields_.size());
    for (const auto& field : fields_) {
        names.push_back(field.first);
    }
    return names;
}

// ---------------------------------------------------------------- Query

Query::Query(std::string table) : table_(std::move(table)) {}

Query& Query::where(std::string column, Comparison comparison, Value value) {
    conditions_.push_back(Condition{std::move(column), comparison, std::move(value)});
    return *this;
}

Query& Query::where_equals(std::string column, Value value) {
    return where(std::move(column), Comparison::Equal, std::move(value));
}

Query& Query::order_by(std::string column, SortOrder order) {
    sorts_.push_back(Sort{std::move(column), order});
    return *this;
}

Query& Query::take(std::size_t count) {
    limit_ = count;
    return *this;
}

Query& Query::skip(std::size_t count) {
    offset_ = count;
    return *this;
}

const std::string& Query::table() const {
    return table_;
}

const std::vector<Condition>& Query::conditions() const {
    return conditions_;
}

const std::vector<Sort>& Query::sorts() const {
    return sorts_;
}

std::optional<std::size_t> Query::limit() const {
    return limit_;
}

std::size_t Query::offset() const {
    return offset_;
}

bool Query::matches(const Row& row) const {
    for (const auto& condition : conditions_) {
        const Value& actual = row.get(condition.column);

        // Null never satisfies an ordering comparison. Treating an absent
        // value as zero is how an unpriced line silently becomes a free one.
        const bool ordering = condition.comparison != Comparison::Equal &&
                              condition.comparison != Comparison::NotEqual;
        if (ordering && (actual.is_null() || condition.value.is_null())) {
            return false;
        }

        switch (condition.comparison) {
            case Comparison::Equal:
                if (!(actual == condition.value)) {
                    return false;
                }
                break;
            case Comparison::NotEqual:
                if (actual == condition.value) {
                    return false;
                }
                break;
            case Comparison::Less:
                if (actual.compare(condition.value) >= 0) {
                    return false;
                }
                break;
            case Comparison::LessOrEqual:
                if (actual.compare(condition.value) > 0) {
                    return false;
                }
                break;
            case Comparison::Greater:
                if (actual.compare(condition.value) <= 0) {
                    return false;
                }
                break;
            case Comparison::GreaterOrEqual:
                if (actual.compare(condition.value) < 0) {
                    return false;
                }
                break;
            case Comparison::Contains: {
                const std::string* haystack = actual.as_text();
                const std::string* needle = condition.value.as_text();
                if (haystack == nullptr || needle == nullptr) {
                    return false;
                }
                if (haystack->find(*needle) == std::string::npos) {
                    return false;
                }
                break;
            }
            case Comparison::StartsWith: {
                const std::string* haystack = actual.as_text();
                const std::string* needle = condition.value.as_text();
                if (haystack == nullptr || needle == nullptr) {
                    return false;
                }
                if (haystack->rfind(*needle, 0) != 0) {
                    return false;
                }
                break;
            }
        }
    }
    return true;
}

// ----------------------------------------------------------- error, bases

StoreError::StoreError(const std::string& message) : std::runtime_error(message) {}

Transaction::~Transaction() = default;

Store::~Store() = default;

}  // namespace squiflow::engine
