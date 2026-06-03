#pragma once

#include <aurex/infrastructure/query/query_key.hpp>

#include <span>
#include <string>
#include <vector>

namespace aurex::query {

enum class BuiltinTypeKey : base::u8 {
    void_ = 0,
    bool_,
    i8,
    u8,
    i16,
    u16,
    i32,
    u32,
    i64,
    u64,
    isize,
    usize,
    f32,
    f64,
    str,
    char_,
};

enum class CanonicalTypeKind : base::u8 {
    invalid = 0,
    builtin,
    pointer,
    reference,
    array,
    slice,
    tuple,
    function,
    nominal,
    generic_param,
    const_arg,
    associated_type_projection,
    trait_object,
};

enum class PointerMutabilityKey : base::u8 {
    mut = 0,
    const_,
};

enum class FunctionCallConvKey : base::u8 {
    aurex = 0,
    c,
};

struct CanonicalTypeKey {
    CanonicalTypeKind kind = CanonicalTypeKind::invalid;
    BuiltinTypeKey builtin = BuiltinTypeKey::void_;
    PointerMutabilityKey mutability = PointerMutabilityKey::const_;
    base::u64 array_count = 0;
    FunctionCallConvKey function_call_conv = FunctionCallConvKey::aurex;
    bool function_is_unsafe = false;
    bool function_is_variadic = false;
    base::u32 function_param_count = 0;
    DefKey nominal_def;
    GenericParamKey generic_param;
    MemberKey associated_member;
    StableFingerprint128 const_value;
    std::vector<CanonicalTypeKey> children;
};

struct DropGlueKey {
    CanonicalTypeKey type;
    StableFingerprint128 resource;
    base::u64 global_id = 0;
};

[[nodiscard]] bool operator==(const CanonicalTypeKey& lhs, const CanonicalTypeKey& rhs) noexcept;
[[nodiscard]] bool operator!=(const CanonicalTypeKey& lhs, const CanonicalTypeKey& rhs) noexcept;
[[nodiscard]] bool is_valid(const CanonicalTypeKey& key) noexcept;
[[nodiscard]] bool operator==(const DropGlueKey& lhs, const DropGlueKey& rhs) noexcept;
[[nodiscard]] bool operator!=(const DropGlueKey& lhs, const DropGlueKey& rhs) noexcept;
[[nodiscard]] bool is_valid(const DropGlueKey& key) noexcept;

[[nodiscard]] CanonicalTypeKey canonical_builtin(BuiltinTypeKey builtin);
[[nodiscard]] CanonicalTypeKey canonical_pointer(PointerMutabilityKey mutability, CanonicalTypeKey pointee);
[[nodiscard]] CanonicalTypeKey canonical_reference(PointerMutabilityKey mutability, CanonicalTypeKey pointee);
[[nodiscard]] CanonicalTypeKey canonical_array(base::u64 count, CanonicalTypeKey element);
[[nodiscard]] CanonicalTypeKey canonical_slice(PointerMutabilityKey mutability, CanonicalTypeKey element);
[[nodiscard]] CanonicalTypeKey canonical_tuple(std::span<const CanonicalTypeKey> elements);
[[nodiscard]] CanonicalTypeKey canonical_function(FunctionCallConvKey call_conv, bool is_unsafe, bool is_variadic,
    std::span<const CanonicalTypeKey> params, const CanonicalTypeKey& return_type);
[[nodiscard]] CanonicalTypeKey canonical_nominal(DefKey definition, std::span<const CanonicalTypeKey> args);
[[nodiscard]] CanonicalTypeKey canonical_generic_param(GenericParamKey parameter);
[[nodiscard]] CanonicalTypeKey canonical_const_arg(StableFingerprint128 value);
[[nodiscard]] CanonicalTypeKey canonical_associated_type_projection(
    CanonicalTypeKey base_type, MemberKey associated_member);
[[nodiscard]] DropGlueKey drop_glue_key(CanonicalTypeKey type, StableFingerprint128 resource);

void append_stable_key(StableKeyWriter& writer, const CanonicalTypeKey& key);
void append_stable_key(StableKeyWriter& writer, const DropGlueKey& key);

[[nodiscard]] std::string stable_serialize(const CanonicalTypeKey& key);
[[nodiscard]] std::string stable_serialize(const DropGlueKey& key);
[[nodiscard]] StableFingerprint128 stable_key_fingerprint(const CanonicalTypeKey& key);
[[nodiscard]] StableFingerprint128 stable_key_fingerprint(const DropGlueKey& key);
[[nodiscard]] std::string debug_string(const CanonicalTypeKey& key);
[[nodiscard]] std::string debug_string(const DropGlueKey& key);

struct CanonicalTypeKeyHash {
    [[nodiscard]] std::size_t operator()(const CanonicalTypeKey& key) const;
};

struct DropGlueKeyHash {
    [[nodiscard]] std::size_t operator()(const DropGlueKey& key) const;
};

} // namespace aurex::query
