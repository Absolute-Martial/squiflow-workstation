#pragma once

// The storage seam.
//
// Everything above this file is written against these interfaces and contains
// no SQLite type at all. Two implementations exist: an in-memory one used by
// every test, and a SQLite one used by the shop. If a repository can only be
// tested against the real database, the seam is in the wrong place.
//
// Deliberately not a "prepare and bind SQL" interface. That seam cannot be
// faked, because a fake would have to implement SQL. The cost is that the
// SQLite implementation translates rather than passes through; the benefit is
// that every layer above is executable on any machine.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace squiflow::engine {

using Blob = std::vector<unsigned char>;

enum class ValueKind : std::uint8_t {
    Null,
    Integer,
    Real,
    Text,
    Binary,
};

// A single stored value. Money is stored as Integer minor units and quantity
// as Integer scaled units, so Real is used for genuinely approximate values
// only and never for anything a customer is charged.
class Value {
public:
    Value() = default;

    static Value null();
    static Value integer(std::int64_t value);
    static Value real(double value);
    static Value text(std::string value);
    static Value binary(Blob value);
    static Value boolean(bool value);

    ValueKind kind() const;
    bool is_null() const;

    std::optional<std::int64_t> as_integer() const;
    std::optional<double> as_real() const;
    const std::string* as_text() const;
    const Blob* as_binary() const;

    std::int64_t integer_or(std::int64_t fallback) const;
    std::string text_or(std::string fallback) const;
    bool boolean_or(bool fallback) const;

    // Ordering across kinds is Null, then Integer and Real together, then
    // Text, then Binary. Fixed so that sorting a column holding more than one
    // kind is at least deterministic.
    int compare(const Value& other) const;
    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const;

    std::string describe() const;

private:
    std::variant<std::monostate, std::int64_t, double, std::string, Blob> data_{};
};

// An ordered set of named values. Order is preserved so that a row read back
// looks like the row written, which matters when a person is reading a log.
class Row {
public:
    using Field = std::pair<std::string, Value>;

    Row() = default;

    Row& set(std::string column, Value value);
    bool has(const std::string& column) const;

    // Returns a null Value for an absent column rather than throwing. An
    // absent column and a stored null are the same thing to a reader, and
    // making every read a potential throw makes repositories unreadable.
    const Value& get(const std::string& column) const;
    std::optional<Value> lookup(const std::string& column) const;

    bool erase(const std::string& column);
    void merge(const Row& other);

    std::size_t size() const;
    bool empty() const;
    const std::vector<Field>& fields() const;
    std::vector<std::string> columns() const;

private:
    std::vector<Field> fields_{};
};

enum class Comparison : std::uint8_t {
    Equal,
    NotEqual,
    Less,
    LessOrEqual,
    Greater,
    GreaterOrEqual,
    Contains,
    StartsWith,
};

enum class SortOrder : std::uint8_t {
    Ascending,
    Descending,
};

struct Condition {
    std::string column{};
    Comparison comparison{Comparison::Equal};
    Value value{};
};

struct Sort {
    std::string column{};
    SortOrder order{SortOrder::Ascending};
};

// Conditions combine with AND only. Anything needing OR is a reporting query,
// and reporting queries do not belong in the path a person is waiting on.
class Query {
public:
    explicit Query(std::string table);

    Query& where(std::string column, Comparison comparison, Value value);
    Query& where_equals(std::string column, Value value);
    Query& order_by(std::string column, SortOrder order = SortOrder::Ascending);
    Query& take(std::size_t count);
    Query& skip(std::size_t count);

    const std::string& table() const;
    const std::vector<Condition>& conditions() const;
    const std::vector<Sort>& sorts() const;
    std::optional<std::size_t> limit() const;
    std::size_t offset() const;

    bool matches(const Row& row) const;

private:
    std::string table_;
    std::vector<Condition> conditions_{};
    std::vector<Sort> sorts_{};
    std::optional<std::size_t> limit_{};
    std::size_t offset_{0};
};

class StoreError : public std::runtime_error {
public:
    explicit StoreError(const std::string& message);
};

// A write scope. Nothing outside a transaction may write, and a transaction
// that is destroyed without commit rolls back, so an early return or a thrown
// exception cannot leave half a record behind.
class Transaction {
public:
    Transaction() = default;
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    Transaction(Transaction&&) = delete;
    Transaction& operator=(Transaction&&) = delete;
    virtual ~Transaction();

    virtual void insert(const std::string& table, const Row& row) = 0;
    virtual bool update(const std::string& table, const std::string& key, const Row& row) = 0;
    virtual bool replace(const std::string& table, const std::string& key, const Row& row) = 0;
    virtual bool remove(const std::string& table, const std::string& key) = 0;

    virtual std::vector<Row> select(const Query& query) const = 0;
    virtual std::optional<Row> find(const std::string& table, const std::string& key) const = 0;

    virtual void commit() = 0;
    virtual void rollback() = 0;
    virtual bool open() const = 0;
};

// Read access is available without a transaction; write access is not.
class Store {
public:
    Store() = default;
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;
    Store(Store&&) = delete;
    Store& operator=(Store&&) = delete;
    virtual ~Store();

    virtual void define_table(const std::string& table, std::string key_column) = 0;
    virtual bool has_table(const std::string& table) const = 0;
    virtual std::vector<std::string> tables() const = 0;

    // One writer. A second call while a transaction is open is refused rather
    // than queued here; queuing is the writer gate's job, one layer up.
    virtual std::unique_ptr<Transaction> begin() = 0;
    virtual bool writing() const = 0;

    virtual std::vector<Row> select(const Query& query) const = 0;
    virtual std::optional<Row> find(const std::string& table, const std::string& key) const = 0;
    virtual std::size_t count(const std::string& table) const = 0;
};

}  // namespace squiflow::engine
