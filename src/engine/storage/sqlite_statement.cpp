#include "engine/storage/sqlite_statement.hpp"

#include <sqlite3.h>

#include <utility>

namespace squiflow::engine {

StoreError sqlite_error(sqlite3* handle, const std::string& what) {
    const char* message = handle != nullptr ? sqlite3_errmsg(handle) : "no database handle";
    return StoreError(what + ": " + (message != nullptr ? message : "unknown error"));
}

Statement::Statement(sqlite3* handle, const std::string& sql) : handle_(handle), sql_(sql) {
    if (handle_ == nullptr) {
        throw StoreError("cannot prepare a statement without a database");
    }
    const int result = sqlite3_prepare_v2(handle_, sql_.c_str(), -1, &statement_, nullptr);
    if (result != SQLITE_OK) {
        throw sqlite_error(handle_, "could not prepare '" + sql_ + "'");
    }
}

Statement::Statement(Statement&& other) noexcept
    : handle_(other.handle_), statement_(other.statement_), sql_(std::move(other.sql_)) {
    other.handle_ = nullptr;
    other.statement_ = nullptr;
}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        finalize();
        handle_ = other.handle_;
        statement_ = other.statement_;
        sql_ = std::move(other.sql_);
        other.handle_ = nullptr;
        other.statement_ = nullptr;
    }
    return *this;
}

Statement::~Statement() {
    finalize();
}

void Statement::finalize() noexcept {
    if (statement_ != nullptr) {
        sqlite3_finalize(statement_);
        statement_ = nullptr;
    }
}

bool Statement::valid() const {
    return statement_ != nullptr;
}

void Statement::bind_null(int index) {
    if (sqlite3_bind_null(statement_, index) != SQLITE_OK) {
        throw sqlite_error(handle_, "could not bind null");
    }
}

void Statement::bind_integer(int index, std::int64_t value) {
    if (sqlite3_bind_int64(statement_, index, value) != SQLITE_OK) {
        throw sqlite_error(handle_, "could not bind an integer");
    }
}

void Statement::bind_real(int index, double value) {
    if (sqlite3_bind_double(statement_, index, value) != SQLITE_OK) {
        throw sqlite_error(handle_, "could not bind a real");
    }
}

void Statement::bind_text(int index, const std::string& value) {
    // SQLITE_TRANSIENT: SQLite copies the text. The alternative saves a copy
    // and requires the caller's string to outlive the statement, which is the
    // kind of rule that holds until someone refactors.
    const int result = sqlite3_bind_text(statement_, index, value.c_str(),
                                         static_cast<int>(value.size()), SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        throw sqlite_error(handle_, "could not bind text");
    }
}

void Statement::bind_blob(int index, const Blob& value) {
    const void* data = value.empty() ? "" : static_cast<const void*>(value.data());
    const int result = sqlite3_bind_blob(statement_, index, data,
                                         static_cast<int>(value.size()), SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        throw sqlite_error(handle_, "could not bind a blob");
    }
}

void Statement::bind(int index, const Value& value) {
    switch (value.kind()) {
        case ValueKind::Null:
            bind_null(index);
            return;
        case ValueKind::Integer:
            bind_integer(index, value.integer_or(0));
            return;
        case ValueKind::Real:
            bind_real(index, value.as_real().value_or(0.0));
            return;
        case ValueKind::Text:
            bind_text(index, value.text_or(""));
            return;
        case ValueKind::Binary: {
            const Blob* blob = value.as_binary();
            bind_blob(index, blob != nullptr ? *blob : Blob{});
            return;
        }
    }
    bind_null(index);
}

bool Statement::step() {
    const int result = sqlite3_step(statement_);
    if (result == SQLITE_ROW) {
        return true;
    }
    if (result == SQLITE_DONE) {
        return false;
    }
    // Including SQLITE_BUSY. The busy timeout has already been waited out by
    // the time this returns, and one writer at a time is enforced a layer up,
    // so a busy here is a real problem and not something to sleep on.
    throw sqlite_error(handle_, "could not run '" + sql_ + "'");
}

void Statement::run() {
    while (step()) {
        // A statement that was expected to return nothing returned a row.
        // Draining it is harmless and keeps the handle usable.
    }
}

void Statement::reset() {
    sqlite3_reset(statement_);
    sqlite3_clear_bindings(statement_);
}

int Statement::column_count() const {
    return sqlite3_column_count(statement_);
}

std::string Statement::column_name(int index) const {
    const char* name = sqlite3_column_name(statement_, index);
    return name != nullptr ? std::string{name} : std::string{};
}

Value Statement::column_value(int index) const {
    switch (sqlite3_column_type(statement_, index)) {
        case SQLITE_NULL:
            return Value::null();
        case SQLITE_INTEGER:
            return Value::integer(sqlite3_column_int64(statement_, index));
        case SQLITE_FLOAT:
            return Value::real(sqlite3_column_double(statement_, index));
        case SQLITE_TEXT: {
            const auto* text = sqlite3_column_text(statement_, index);
            const int size = sqlite3_column_bytes(statement_, index);
            if (text == nullptr) {
                return Value::text("");
            }
            return Value::text(std::string{reinterpret_cast<const char*>(text),
                                           static_cast<std::size_t>(size)});
        }
        case SQLITE_BLOB: {
            const void* data = sqlite3_column_blob(statement_, index);
            const int size = sqlite3_column_bytes(statement_, index);
            if (data == nullptr || size <= 0) {
                return Value::binary(Blob{});
            }
            const auto* bytes = static_cast<const unsigned char*>(data);
            return Value::binary(Blob{bytes, bytes + size});
        }
        default:
            return Value::null();
    }
}

Row Statement::row() const {
    Row row;
    const int columns = column_count();
    for (int index = 0; index < columns; ++index) {
        const Value value = column_value(index);
        if (value.is_null()) {
            // An absent column and a stored null read the same way, and not
            // writing the null keeps a row that came back from the database
            // looking like the row that went in.
            continue;
        }
        row.set(column_name(index), value);
    }
    return row;
}

}  // namespace squiflow::engine
