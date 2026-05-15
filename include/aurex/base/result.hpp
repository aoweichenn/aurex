#pragma once

#include <cassert>
#include <string>
#include <utility>
#include <variant>

namespace aurex::base {

enum class ErrorCode {
    ok,
    io_error,
    invalid_source,
    lex_error,
    parse_error,
    sema_error,
    codegen_error,
    internal_error,
};

struct Error {
    ErrorCode code = ErrorCode::ok;
    std::string message;
};

template <typename T>
class [[nodiscard]] Result {
public:
    [[nodiscard]] static Result ok(T value) {
        return Result(std::move(value));
    }

    [[nodiscard]] static Result fail(Error error) {
        return Result(std::move(error));
    }

    [[nodiscard]] bool has_value() const noexcept {
        return std::holds_alternative<T>(storage_);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return has_value();
    }

    [[nodiscard]] T& value() noexcept {
        assert(has_value());
        return std::get<T>(storage_);
    }

    [[nodiscard]] const T& value() const noexcept {
        assert(has_value());
        return std::get<T>(storage_);
    }

    [[nodiscard]] T&& take_value() noexcept {
        assert(has_value());
        return std::move(std::get<T>(storage_));
    }

    [[nodiscard]] const Error& error() const noexcept {
        assert(!has_value());
        return std::get<Error>(storage_);
    }

private:
    explicit Result(T value) : storage_(std::move(value)) {}
    explicit Result(Error error) : storage_(std::move(error)) {}

    std::variant<T, Error> storage_;
};

template <>
class [[nodiscard]] Result<void> {
public:
    [[nodiscard]] static Result ok() {
        return Result(true, {});
    }

    [[nodiscard]] static Result fail(Error error) {
        return Result(false, std::move(error));
    }

    [[nodiscard]] bool has_value() const noexcept {
        return has_value_;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return has_value();
    }

    void value() const noexcept {
        assert(has_value());
    }

    [[nodiscard]] const Error& error() const noexcept {
        assert(!has_value());
        return error_;
    }

private:
    Result(const bool has_value, Error error) : has_value_(has_value), error_(std::move(error)) {}

    bool has_value_ = true;
    Error error_;
};

} // namespace aurex::base
