#pragma once

#include <string>
#include <string_view>
#include <variant>

namespace nex {

enum class ErrorCode {
    ok,
    io_error,
    parse_error,
    config_error,
    cycle_detected,
    resolve_error,
    cache_error,
    exec_error,
};

inline std::string_view error_name(ErrorCode code) {
    switch (code) {
    case ErrorCode::ok:             return "ok";
    case ErrorCode::io_error:       return "io_error";
    case ErrorCode::parse_error:    return "parse_error";
    case ErrorCode::config_error:   return "config_error";
    case ErrorCode::cycle_detected: return "cycle_detected";
    case ErrorCode::resolve_error:  return "resolve_error";
    case ErrorCode::cache_error:    return "cache_error";
    case ErrorCode::exec_error:     return "exec_error";
    }
    return "unknown";
}

struct Error {
    ErrorCode code = ErrorCode::ok;
    std::string message;
};

template <typename T>
class [[nodiscard]] Result {
public:
    static Result ok(T value) {
        Result r;
        r.data_ = std::move(value);
        return r;
    }
    static Result err(Error error) {
        Result r;
        r.data_ = std::move(error);
        return r;
    }
    static Result err(ErrorCode code, std::string message) {
        return err(Error{code, std::move(message)});
    }

    bool is_ok() const noexcept { return std::holds_alternative<T>(data_); }
    bool is_err() const noexcept { return !is_ok(); }

    T& value() & { return std::get<T>(data_); }
    const T& value() const& { return std::get<T>(data_); }
    T&& value() && { return std::get<T>(std::move(data_)); }

    const Error& error() const { return std::get<Error>(data_); }

    operator bool() const noexcept { return is_ok(); }

private:
    std::variant<T, Error> data_;
};

template <>
class [[nodiscard]] Result<void> {
public:
    static Result ok() {
        Result r;
        r.ok_ = true;
        return r;
    }
    static Result err(Error error) {
        Result r;
        r.ok_ = false;
        r.error_ = std::move(error);
        return r;
    }
    static Result err(ErrorCode code, std::string message) {
        return err(Error{code, std::move(message)});
    }

    bool is_ok() const noexcept { return ok_; }
    bool is_err() const noexcept { return !ok_; }
    const Error& error() const { return error_; }
    explicit operator bool() const noexcept { return ok_; }

private:
    bool ok_ = false;
    Error error_;
};

} // namespace nex
