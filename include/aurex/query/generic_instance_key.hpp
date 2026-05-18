#pragma once

#include <aurex/query/canonical_type_key.hpp>

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::query {

struct ParamEnvKey {
    StableFingerprint128 predicates;
    base::u32 predicate_count = 0;
    base::u64 global_id = 0;

    [[nodiscard]] friend constexpr bool operator==(
        ParamEnvKey lhs,
        ParamEnvKey rhs) noexcept = default;
};

struct GenericInstanceKey {
    DefKey template_def;
    std::vector<CanonicalTypeKey> type_args;
    std::vector<StableFingerprint128> const_args;
    ParamEnvKey param_env;
    base::u64 global_id = 0;
};

[[nodiscard]] bool is_valid(ParamEnvKey key) noexcept;
[[nodiscard]] bool is_valid(const GenericInstanceKey& key) noexcept;
[[nodiscard]] bool operator==(const GenericInstanceKey& lhs, const GenericInstanceKey& rhs) noexcept;
[[nodiscard]] bool operator!=(const GenericInstanceKey& lhs, const GenericInstanceKey& rhs) noexcept;

[[nodiscard]] ParamEnvKey param_env_key(std::span<const std::string_view> predicates) noexcept;
[[nodiscard]] GenericInstanceKey generic_instance_key(
    DefKey template_def,
    std::span<const CanonicalTypeKey> type_args,
    std::span<const StableFingerprint128> const_args,
    ParamEnvKey param_env);

void append_stable_key(StableKeyWriter& writer, ParamEnvKey key);
void append_stable_key(StableKeyWriter& writer, const GenericInstanceKey& key);

[[nodiscard]] std::string stable_serialize(ParamEnvKey key);
[[nodiscard]] std::string stable_serialize(const GenericInstanceKey& key);
[[nodiscard]] StableFingerprint128 stable_key_fingerprint(ParamEnvKey key);
[[nodiscard]] StableFingerprint128 stable_key_fingerprint(const GenericInstanceKey& key);
[[nodiscard]] std::string debug_string(ParamEnvKey key);
[[nodiscard]] std::string debug_string(const GenericInstanceKey& key);

struct ParamEnvKeyHash {
    [[nodiscard]] std::size_t operator()(ParamEnvKey key) const;
};

struct GenericInstanceKeyHash {
    [[nodiscard]] std::size_t operator()(const GenericInstanceKey& key) const;
};

} // namespace aurex::query
