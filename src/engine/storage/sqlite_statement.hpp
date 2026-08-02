#pragma once

// A prepared statement, and the only place in the program that touches the
// SQLite C API's return codes.
//
// The header names no SQLite type. `sqlite3` and `sqlite3_stmt` are forward
// declared, so everything that includes this file - and everything that
// includes anything that includes it - compiles on a machine with no SQLite
// headers at all. That is what keeps the whole engine testable in a sandbox.

#include <cstdint>
#include <string>

#include "engine/storage/store.hpp"

struct sqlite3;
struct sqlite3_stmt;

namespace squiflow::engine {

// Thrown with the message SQLite gave, plus the statement that caused it.
// A bare "error code 5" in a log costs an hour; the text costs nothing.
StoreError sqlite_error(sqlite3* handle, const std::string& what);

class Statement {
public:
    Statement() = default;
    Statement(sqlite3* handle, const std::string& sql);

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;
    ~Statement();

    // One-based, as SQLite counts them.
    void bind(int index, const Value& value);
    void bind_null(int index);
    void bind_integer(int index, std::int64_t value);
    void bind_real(int index, double value);
    void bind_text(int index, const std::string& value);
    void bind_blob(int index, const Blob& value);

    // True when a row is available, false when the statement is done.
    // A busy or locked result is an error here rather than a retry loop:
    // retrying is unfair and can starve a writer, so waiting is the writer
    // gate's job and the busy timeout's, not this file's.
    bool step();

    // Runs a statement that returns no rows.
    void run();

    void reset();

    int column_count() const;
    std::string column_name(int index) const;
    Value column_value(int index) const;
    Row row() const;

    bool valid() const;

private:
    void finalize() noexcept;

    sqlite3* handle_{nullptr};
    sqlite3_stmt* statement_{nullptr};
    std::string sql_{};
};

}  // namespace squiflow::engine
