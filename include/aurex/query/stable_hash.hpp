#pragma once

#include <aurex/base/integer.hpp>

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace aurex::query {

struct StableFingerprint128 {
    base::u64 primary = 0;
    base::u64 secondary = 0;
    base::u32 byte_count = 0;

    [[nodiscard]] friend constexpr bool operator==(
        StableFingerprint128 lhs,
        StableFingerprint128 rhs) noexcept = default;
};

[[nodiscard]] base::u64 stable_mix(base::u64 seed, base::u64 value) noexcept;
[[nodiscard]] StableFingerprint128 stable_fingerprint(std::string_view text) noexcept;
[[nodiscard]] StableFingerprint128 stable_fingerprint(std::span<const std::string_view> parts) noexcept;
[[nodiscard]] std::size_t stable_hash_value(StableFingerprint128 fingerprint) noexcept;
[[nodiscard]] std::string debug_string(StableFingerprint128 fingerprint);

class StableHashBuilder final {
public:
    StableHashBuilder() noexcept;

    void mix_u8(base::u8 value) noexcept;
    void mix_u16(base::u16 value) noexcept;
    void mix_u32(base::u32 value) noexcept;
    void mix_u64(base::u64 value) noexcept;
    void mix_bool(bool value) noexcept;
    void mix_bytes(std::string_view bytes) noexcept;
    void mix_string(std::string_view value) noexcept;
    void mix_fingerprint(StableFingerprint128 fingerprint) noexcept;

    [[nodiscard]] StableFingerprint128 finish() const noexcept;

private:
    void mix_raw_byte(base::u8 value) noexcept;

    StableFingerprint128 fingerprint_;
};

class StableKeyWriter final {
public:
    StableKeyWriter();

    void write_u8(base::u8 value);
    void write_u16(base::u16 value);
    void write_u32(base::u32 value);
    void write_u64(base::u64 value);
    void write_bool(bool value);
    void write_string(std::string_view value);
    void write_fingerprint(StableFingerprint128 fingerprint);

    [[nodiscard]] std::string_view bytes() const noexcept;
    [[nodiscard]] const std::string& storage() const noexcept;
    [[nodiscard]] StableFingerprint128 fingerprint() const noexcept;

private:
    void write_raw_byte(base::u8 value);

    std::string bytes_;
    StableHashBuilder hash_;
};

struct StableFingerprintHash {
    [[nodiscard]] std::size_t operator()(StableFingerprint128 fingerprint) const noexcept;
};

} // namespace aurex::query
