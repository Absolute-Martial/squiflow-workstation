#pragma once

#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace squiflow::app {

// Project result boundary. Its API is intentionally the subset of
// std::expected used by application ports. The GCC 11 verification library
// does not provide <expected>; this wrapper can adopt std::expected internally
// when the compiler floor is raised without changing service signatures.
template <class E>
class Unexpected final {
  public:
    explicit Unexpected(E error) : error_(std::move(error)) {}
    const E& error() const& noexcept { return error_; }
    E& error() & noexcept { return error_; }
    E&& error() && noexcept { return std::move(error_); }

  private:
    E error_;
};

template <class T>
struct ResultValue final {
    T value;
};

template <class E>
struct ResultError final {
    E error;
};

template <class T, class E>
class Result final {
    static_assert(!std::is_reference_v<T>);
    static_assert(!std::is_reference_v<E>);

  public:
    static Result success(T value) {
        return Result(ResultValue<T>{std::move(value)});
    }

    static Result failure(E error) {
        return Result(ResultError<E>{std::move(error)});
    }

    static Result failure(Unexpected<E> error) {
        return failure(std::move(error).error());
    }

    bool has_value() const noexcept {
        return std::holds_alternative<ResultValue<T>>(storage_);
    }

    explicit operator bool() const noexcept { return has_value(); }

    T* value_if() noexcept { return std::get_if<ResultValue<T>>(&storage_) == nullptr
        ? nullptr : &std::get<ResultValue<T>>(storage_).value; }
    const T* value_if() const noexcept {
        const auto* holder = std::get_if<ResultValue<T>>(&storage_);
        return holder == nullptr ? nullptr : &holder->value;
    }

    E* error_if() noexcept { return std::get_if<ResultError<E>>(&storage_) == nullptr
        ? nullptr : &std::get<ResultError<E>>(storage_).error; }
    const E* error_if() const noexcept {
        const auto* holder = std::get_if<ResultError<E>>(&storage_);
        return holder == nullptr ? nullptr : &holder->error;
    }

    T& value() & {
        if (auto* result = value_if()) { return *result; }
        throw std::logic_error("Result has no value");
    }
    const T& value() const& {
        if (const auto* result = value_if()) { return *result; }
        throw std::logic_error("Result has no value");
    }
    T&& value() && { return std::move(value()); }

    E& error() & {
        if (auto* result = error_if()) { return *result; }
        throw std::logic_error("Result has no error");
    }
    const E& error() const& {
        if (const auto* result = error_if()) { return *result; }
        throw std::logic_error("Result has no error");
    }
    E&& error() && { return std::move(error()); }

  private:
    explicit Result(ResultValue<T> value) : storage_(std::move(value)) {}
    explicit Result(ResultError<E> error) : storage_(std::move(error)) {}
    std::variant<ResultValue<T>, ResultError<E>> storage_;
};

template <class E>
class Result<void, E> final {
  public:
    static Result success() { return Result(std::monostate{}); }
    static Result failure(E error) {
        return Result(ResultError<E>{std::move(error)});
    }
    static Result failure(Unexpected<E> error) {
        return failure(std::move(error).error());
    }
    bool has_value() const noexcept {
        return std::holds_alternative<std::monostate>(storage_);
    }
    explicit operator bool() const noexcept { return has_value(); }
    const E* error_if() const noexcept {
        const auto* holder = std::get_if<ResultError<E>>(&storage_);
        return holder == nullptr ? nullptr : &holder->error;
    }
    E* error_if() noexcept {
        auto* holder = std::get_if<ResultError<E>>(&storage_);
        return holder == nullptr ? nullptr : &holder->error;
    }
    E& error() & {
        if (auto* result = error_if()) { return *result; }
        throw std::logic_error("Result has no error");
    }
    const E& error() const& {
        if (const auto* result = error_if()) { return *result; }
        throw std::logic_error("Result has no error");
    }

  private:
    explicit Result(std::monostate value) : storage_(value) {}
    explicit Result(ResultError<E> error) : storage_(std::move(error)) {}
    std::variant<std::monostate, ResultError<E>> storage_;
};

}  // namespace squiflow::app
