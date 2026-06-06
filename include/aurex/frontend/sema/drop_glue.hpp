#pragma once

#include <aurex/frontend/sema/function.hpp>
#include <aurex/frontend/sema/resource_semantics.hpp>
#include <aurex/frontend/sema/type.hpp>
#include <aurex/infrastructure/base/result.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>

#include <string_view>
#include <vector>

namespace aurex::sema {

struct CheckedModule;

enum class DropGlueStepKind : base::u8 {
    struct_field,
    tuple_element,
    array_element,
    enum_payload,
    generic_value,
    opaque_value,
    unknown_value,
    custom_destructor,
};

enum class DropGlueAbiPolicy : base::u8 {
    structural_static,
    generic_marker_only,
    associated_projection_marker_only,
    opaque_marker_only,
    unknown_marker_only,
    static_custom_destructor,
};

struct DropGlueStep {
    DropGlueStepKind kind = DropGlueStepKind::generic_value;
    DropGlueAbiPolicy abi_policy = DropGlueAbiPolicy::generic_marker_only;
    TypeHandle owner_type = INVALID_TYPE_HANDLE;
    TypeHandle value_type = INVALID_TYPE_HANDLE;
    base::u32 ordinal = 0;
    ResourceSemanticsSummary resource;
    FunctionLookupKey destructor_function;
};

struct DropGluePlan {
    TypeHandle root_type = INVALID_TYPE_HANDLE;
    ResourceSemanticsSummary root_resource;
    std::vector<DropGlueStep> steps;
    query::StableFingerprint128 fingerprint;
};

[[nodiscard]] std::string_view drop_glue_step_kind_name(DropGlueStepKind kind) noexcept;
[[nodiscard]] std::string_view drop_glue_abi_policy_name(DropGlueAbiPolicy policy) noexcept;
[[nodiscard]] bool drop_glue_plan_needs_drop(const DropGluePlan& plan) noexcept;
[[nodiscard]] base::Result<DropGluePlan> build_drop_glue_plan(const CheckedModule& checked, TypeHandle root);

} // namespace aurex::sema
