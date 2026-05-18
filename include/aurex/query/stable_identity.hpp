#pragma once

#include <aurex/query/stable_hash.hpp>

#include <span>
#include <string>
#include <string_view>

namespace aurex::query {

struct StableModuleId {
    StableFingerprint128 path;
    base::u32 part_count = 0;
    base::u64 global_id = 0;

    [[nodiscard]] friend constexpr bool operator==(StableModuleId lhs, StableModuleId rhs) noexcept = default;
};

enum class StableSymbolKind : base::u8 {
    invalid = 0,
    type,
    function,
    method,
    value,
    enum_case,
    struct_field,
    generic_template,
    synthetic,
};

struct StableDefId {
    StableModuleId module;
    StableFingerprint128 name;
    base::u64 global_id = 0;
    base::u32 disambiguator = 0;
    StableSymbolKind kind = StableSymbolKind::invalid;

    [[nodiscard]] friend constexpr bool operator==(StableDefId lhs, StableDefId rhs) noexcept = default;
};

struct StableMemberKey {
    StableDefId owner;
    StableFingerprint128 member_name;
    base::u64 global_id = 0;
    base::u32 disambiguator = 0;
    StableSymbolKind kind = StableSymbolKind::invalid;

    [[nodiscard]] friend constexpr bool operator==(StableMemberKey lhs, StableMemberKey rhs) noexcept = default;
};

struct IncrementalKey {
    StableDefId definition;
    StableFingerprint128 fingerprint;
    base::u64 global_id = 0;

    [[nodiscard]] friend constexpr bool operator==(IncrementalKey lhs, IncrementalKey rhs) noexcept = default;
};

[[nodiscard]] bool is_valid(StableModuleId id) noexcept;
[[nodiscard]] bool is_valid(StableDefId id) noexcept;
[[nodiscard]] bool is_valid(StableMemberKey key) noexcept;
[[nodiscard]] bool is_valid(IncrementalKey key) noexcept;

[[nodiscard]] StableFingerprint128 stable_identity_fingerprint(std::span<const std::string_view> parts) noexcept;
[[nodiscard]] StableModuleId stable_module_id(std::span<const std::string_view> module_path) noexcept;
[[nodiscard]] StableDefId stable_definition_id(
    const StableModuleId& module, StableSymbolKind kind, std::string_view name, base::u32 disambiguator = 0) noexcept;
[[nodiscard]] StableMemberKey stable_member_key(const StableDefId& owner, StableSymbolKind kind,
    std::string_view member_name, base::u32 disambiguator = 0) noexcept;
[[nodiscard]] IncrementalKey stable_incremental_key(
    const StableDefId& definition, std::string_view semantic_fingerprint) noexcept;

void append_stable_key(StableKeyWriter& writer, StableModuleId id);
void append_stable_key(StableKeyWriter& writer, StableDefId id);
void append_stable_key(StableKeyWriter& writer, StableMemberKey key);
void append_stable_key(StableKeyWriter& writer, IncrementalKey key);

[[nodiscard]] std::string stable_serialize(StableModuleId id);
[[nodiscard]] std::string stable_serialize(StableDefId id);
[[nodiscard]] std::string stable_serialize(StableMemberKey key);
[[nodiscard]] std::string stable_serialize(IncrementalKey key);

[[nodiscard]] StableFingerprint128 stable_key_fingerprint(StableModuleId id);
[[nodiscard]] StableFingerprint128 stable_key_fingerprint(StableDefId id);
[[nodiscard]] StableFingerprint128 stable_key_fingerprint(StableMemberKey key);
[[nodiscard]] StableFingerprint128 stable_key_fingerprint(IncrementalKey key);

[[nodiscard]] std::string debug_string(StableModuleId id);
[[nodiscard]] std::string debug_string(StableDefId id);
[[nodiscard]] std::string debug_string(StableMemberKey key);
[[nodiscard]] std::string debug_string(IncrementalKey key);

} // namespace aurex::query
