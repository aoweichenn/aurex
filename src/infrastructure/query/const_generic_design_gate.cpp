#include <aurex/infrastructure/query/const_generic_design_gate.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <sstream>
#include <utility>

namespace aurex::query {
namespace {

constexpr std::string_view QUERY_CONST_GENERIC_GATE_FINGERPRINT_MARKER =
    "query.const_generic_design_gate.v1";
constexpr std::string_view QUERY_CONST_GENERIC_M15_GATE_NAME =
    "M15 Const Generic Design Baseline";
constexpr std::string_view QUERY_CONST_GENERIC_M15_NO_STD_NON_GOAL =
    "standard_library_runtime_not_in_m15";
constexpr std::string_view QUERY_CONST_GENERIC_M15_NO_RUNTIME_NON_GOAL =
    "runtime_const_generic_not_in_m15";
constexpr std::string_view QUERY_CONST_GENERIC_M15_NO_DYN_NON_GOAL =
    "dyn_const_generic_not_in_m15";
constexpr std::string_view QUERY_CONST_GENERIC_TYPED_PARAM_POLICY =
    "typed_const_param_v1";
constexpr std::string_view QUERY_CONST_GENERIC_CANONICAL_VALUE_POLICY =
    "canonical_const_value_v1";
constexpr std::string_view QUERY_CONST_GENERIC_INSTANCE_KEY_POLICY =
    "generic_instance_const_arg_key_v1";
constexpr std::string_view QUERY_CONST_GENERIC_ARRAY_LENGTH_POLICY =
    "array_length_const_param_v1";
constexpr std::string_view QUERY_CONST_GENERIC_COMPTIME_SUBSET_POLICY =
    "scalar_const_eval_subset_v1";
constexpr std::string_view QUERY_CONST_GENERIC_TRAIT_BOUNDARY_POLICY =
    "const_generic_trait_solver_boundary_v1";
constexpr base::usize QUERY_CONST_GENERIC_M15_CAPABILITY_COUNT = 6;
constexpr base::u8 QUERY_CONST_GENERIC_INVALID_CAPABILITY_VALUE = 255U;
constexpr base::u8 QUERY_CONST_GENERIC_INVALID_STAGE_VALUE = 255U;
constexpr base::u8 QUERY_CONST_GENERIC_INVALID_DECISION_VALUE = 255U;

[[nodiscard]] base::u8 stable_capability_value(const ConstGenericCapability capability) noexcept
{
    return is_valid(capability) ? static_cast<base::u8>(capability)
                                : QUERY_CONST_GENERIC_INVALID_CAPABILITY_VALUE;
}

[[nodiscard]] base::u8 stable_stage_value(const ConstGenericGateStage stage) noexcept
{
    return is_valid(stage) ? static_cast<base::u8>(stage) : QUERY_CONST_GENERIC_INVALID_STAGE_VALUE;
}

[[nodiscard]] base::u8 stable_decision_value(const ConstGenericPolicyDecision decision) noexcept
{
    return is_valid(decision) ? static_cast<base::u8>(decision)
                              : QUERY_CONST_GENERIC_INVALID_DECISION_VALUE;
}

[[nodiscard]] bool contains_text(
    const std::vector<std::string>& values, const std::string_view expected) noexcept
{
    return std::any_of(values.begin(), values.end(), [expected](const std::string& value) {
        return std::string_view(value) == expected;
    });
}

[[nodiscard]] bool has_required_detail_vectors(const ConstGenericDesignCandidate& candidate) noexcept
{
    return !candidate.blockers.empty() && !candidate.required_facts.empty() && !candidate.non_goals.empty();
}

[[nodiscard]] bool decision_matches_impact(const ConstGenericDesignCandidate& candidate) noexcept
{
    if (candidate.impact.runtime_required
        && candidate.decision != ConstGenericPolicyDecision::requires_runtime_or_std_boundary) {
        return false;
    }
    if (candidate.impact.standard_library_required
        && candidate.decision != ConstGenericPolicyDecision::requires_runtime_or_std_boundary) {
        return false;
    }
    if (candidate.impact.comptime_engine_required
        && candidate.decision != ConstGenericPolicyDecision::requires_comptime_engine) {
        return false;
    }
    if (candidate.impact.trait_solver_impact
        && candidate.decision != ConstGenericPolicyDecision::requires_trait_solver_extension) {
        return false;
    }
    return true;
}

[[nodiscard]] bool gate_has_each_const_generic_capability_once(
    const ConstGenericDesignGate& gate) noexcept
{
    if (gate.candidates.size() != QUERY_CONST_GENERIC_M15_CAPABILITY_COUNT) {
        return false;
    }

    std::array<bool, QUERY_CONST_GENERIC_M15_CAPABILITY_COUNT> seen = {};
    for (const ConstGenericDesignCandidate& candidate : gate.candidates) {
        if (!is_valid(candidate.capability)) {
            return false;
        }
        const base::usize index = static_cast<base::usize>(candidate.capability) - 1U;
        if (index >= seen.size() || seen[index]) {
            return false;
        }
        seen[index] = true;
    }
    return std::all_of(seen.begin(), seen.end(), [](const bool present) {
        return present;
    });
}

[[nodiscard]] bool candidate_shape_is_valid(
    const ConstGenericDesignCandidate& candidate) noexcept
{
    return is_valid(candidate.capability) && is_valid(candidate.stage) && is_valid(candidate.decision)
        && !candidate.selected_policy.empty() && has_required_detail_vectors(candidate)
        && decision_matches_impact(candidate);
}

[[nodiscard]] bool m15_capability_gate_is_valid(
    const ConstGenericDesignCandidate& candidate) noexcept
{
    switch (candidate.capability) {
        case ConstGenericCapability::typed_const_parameter_surface:
            return candidate.stage == ConstGenericGateStage::ready_for_implementation
                && candidate.decision == ConstGenericPolicyDecision::selected_m15_frontend_query_path
                && std::string_view(candidate.selected_policy) == QUERY_CONST_GENERIC_TYPED_PARAM_POLICY
                && candidate.impact.parser_ast_impact && candidate.impact.sema_type_system_impact
                && candidate.impact.query_key_impact && candidate.impact.incremental_cache_impact
                && !candidate.impact.ir_layout_impact && !candidate.impact.comptime_engine_required
                && !candidate.impact.trait_solver_impact && !candidate.impact.standard_library_required
                && !candidate.impact.runtime_required;
        case ConstGenericCapability::canonical_const_argument_identity:
            return candidate.stage == ConstGenericGateStage::ready_for_implementation
                && candidate.decision == ConstGenericPolicyDecision::selected_m15_frontend_query_path
                && std::string_view(candidate.selected_policy) == QUERY_CONST_GENERIC_CANONICAL_VALUE_POLICY
                && !candidate.impact.parser_ast_impact && candidate.impact.sema_type_system_impact
                && candidate.impact.query_key_impact && candidate.impact.incremental_cache_impact
                && !candidate.impact.ir_layout_impact && !candidate.impact.comptime_engine_required
                && !candidate.impact.trait_solver_impact && !candidate.impact.standard_library_required
                && !candidate.impact.runtime_required;
        case ConstGenericCapability::generic_instance_key_integration:
            return candidate.stage == ConstGenericGateStage::ready_for_implementation
                && candidate.decision == ConstGenericPolicyDecision::selected_m15_frontend_query_path
                && std::string_view(candidate.selected_policy) == QUERY_CONST_GENERIC_INSTANCE_KEY_POLICY
                && !candidate.impact.parser_ast_impact && candidate.impact.sema_type_system_impact
                && candidate.impact.query_key_impact && candidate.impact.incremental_cache_impact
                && !candidate.impact.ir_layout_impact && !candidate.impact.comptime_engine_required
                && !candidate.impact.trait_solver_impact && !candidate.impact.standard_library_required
                && !candidate.impact.runtime_required;
        case ConstGenericCapability::array_length_type_integration:
            return candidate.stage == ConstGenericGateStage::ready_for_implementation
                && candidate.decision == ConstGenericPolicyDecision::selected_m15_frontend_query_path
                && std::string_view(candidate.selected_policy) == QUERY_CONST_GENERIC_ARRAY_LENGTH_POLICY
                && candidate.impact.parser_ast_impact && candidate.impact.sema_type_system_impact
                && candidate.impact.query_key_impact && candidate.impact.incremental_cache_impact
                && candidate.impact.ir_layout_impact && !candidate.impact.comptime_engine_required
                && !candidate.impact.trait_solver_impact && !candidate.impact.standard_library_required
                && !candidate.impact.runtime_required;
        case ConstGenericCapability::const_expression_evaluation_subset:
            return candidate.stage == ConstGenericGateStage::blocked_by_dependency
                && candidate.decision == ConstGenericPolicyDecision::requires_comptime_engine
                && std::string_view(candidate.selected_policy) == QUERY_CONST_GENERIC_COMPTIME_SUBSET_POLICY
                && !candidate.impact.parser_ast_impact && candidate.impact.sema_type_system_impact
                && candidate.impact.query_key_impact && candidate.impact.incremental_cache_impact
                && candidate.impact.ir_layout_impact && candidate.impact.comptime_engine_required
                && !candidate.impact.trait_solver_impact && !candidate.impact.standard_library_required
                && !candidate.impact.runtime_required;
        case ConstGenericCapability::trait_predicate_and_dyn_boundary:
            return candidate.stage == ConstGenericGateStage::future_stage
                && candidate.decision == ConstGenericPolicyDecision::requires_trait_solver_extension
                && std::string_view(candidate.selected_policy) == QUERY_CONST_GENERIC_TRAIT_BOUNDARY_POLICY
                && !candidate.impact.parser_ast_impact && candidate.impact.sema_type_system_impact
                && candidate.impact.query_key_impact && candidate.impact.incremental_cache_impact
                && !candidate.impact.ir_layout_impact && !candidate.impact.comptime_engine_required
                && candidate.impact.trait_solver_impact && !candidate.impact.standard_library_required
                && !candidate.impact.runtime_required;
    }
    return false;
}

void mix_string_vector(StableHashBuilder& builder, const std::vector<std::string>& values) noexcept
{
    builder.mix_u64(static_cast<base::u64>(values.size()));
    for (const std::string& value : values) {
        builder.mix_string(value);
    }
}

void mix_impact(StableHashBuilder& builder, const ConstGenericImpactSummary& impact) noexcept
{
    builder.mix_bool(impact.parser_ast_impact);
    builder.mix_bool(impact.sema_type_system_impact);
    builder.mix_bool(impact.query_key_impact);
    builder.mix_bool(impact.incremental_cache_impact);
    builder.mix_bool(impact.ir_layout_impact);
    builder.mix_bool(impact.comptime_engine_required);
    builder.mix_bool(impact.trait_solver_impact);
    builder.mix_bool(impact.standard_library_required);
    builder.mix_bool(impact.runtime_required);
}

void mix_candidate(StableHashBuilder& builder, const ConstGenericDesignCandidate& candidate) noexcept
{
    builder.mix_u8(stable_capability_value(candidate.capability));
    builder.mix_u8(stable_stage_value(candidate.stage));
    builder.mix_u8(stable_decision_value(candidate.decision));
    builder.mix_string(candidate.selected_policy);
    mix_impact(builder, candidate.impact);
    mix_string_vector(builder, candidate.blockers);
    mix_string_vector(builder, candidate.required_facts);
    mix_string_vector(builder, candidate.non_goals);
}

void append_impact_summary(std::ostringstream& stream, const ConstGenericImpactSummary& impact)
{
    stream << " impact="
           << "parser_ast:" << (impact.parser_ast_impact ? "yes" : "no")
           << ",sema_type_system:" << (impact.sema_type_system_impact ? "yes" : "no")
           << ",query_key:" << (impact.query_key_impact ? "yes" : "no")
           << ",incremental_cache:" << (impact.incremental_cache_impact ? "yes" : "no")
           << ",ir_layout:" << (impact.ir_layout_impact ? "yes" : "no")
           << ",comptime_engine:" << (impact.comptime_engine_required ? "yes" : "no")
           << ",trait_solver:" << (impact.trait_solver_impact ? "yes" : "no")
           << ",standard_library:" << (impact.standard_library_required ? "yes" : "no")
           << ",runtime:" << (impact.runtime_required ? "yes" : "no");
}

ConstGenericDesignCandidate make_typed_const_parameter_candidate()
{
    return ConstGenericDesignCandidate{
        ConstGenericCapability::typed_const_parameter_surface,
        ConstGenericGateStage::ready_for_implementation,
        ConstGenericPolicyDecision::selected_m15_frontend_query_path,
        std::string(QUERY_CONST_GENERIC_TYPED_PARAM_POLICY),
        ConstGenericImpactSummary{true, true, true, true, false, false, false, false, false},
        {
            "M15 selected the typed scalar const parameter route; M16 implements syntax::GenericParamKind::const_ alongside type and origin params",
            "parser must keep bracket indexing and array contexts unambiguous with angle generic contexts",
            "const parameters need explicit scalar type annotation before value arguments are accepted",
        },
        {
            "const_generic_param_decl_fact",
            "const_generic_param_type_fact",
            "const_generic_param_identity_fact",
        },
        {
            std::string(QUERY_CONST_GENERIC_M15_NO_STD_NON_GOAL),
            std::string(QUERY_CONST_GENERIC_M15_NO_RUNTIME_NON_GOAL),
            "do_not_support_untyped_const_params",
            "do_not_accept_full_const_expressions_as_params_in_m15",
        },
    };
}

ConstGenericDesignCandidate make_canonical_const_argument_candidate()
{
    return ConstGenericDesignCandidate{
        ConstGenericCapability::canonical_const_argument_identity,
        ConstGenericGateStage::ready_for_implementation,
        ConstGenericPolicyDecision::selected_m15_frontend_query_path,
        std::string(QUERY_CONST_GENERIC_CANONICAL_VALUE_POLICY),
        ConstGenericImpactSummary{false, true, true, true, false, false, false, false, false},
        {
            "GenericInstanceKey currently stores canonical type arguments only",
            "value identity must be typed and independent of source spelling",
            "signedness, width, bool, char, and usize array length need deterministic encoding",
        },
        {
            "canonical_const_value_key",
            "const_argument_type_key",
            "const_argument_value_fingerprint",
        },
        {
            std::string(QUERY_CONST_GENERIC_M15_NO_STD_NON_GOAL),
            std::string(QUERY_CONST_GENERIC_M15_NO_RUNTIME_NON_GOAL),
            "do_not_key_const_arguments_by_display_text",
            "do_not_allow_non_scalar_const_arguments_in_m15",
        },
    };
}

ConstGenericDesignCandidate make_generic_instance_key_candidate()
{
    return ConstGenericDesignCandidate{
        ConstGenericCapability::generic_instance_key_integration,
        ConstGenericGateStage::ready_for_implementation,
        ConstGenericPolicyDecision::selected_m15_frontend_query_path,
        std::string(QUERY_CONST_GENERIC_INSTANCE_KEY_POLICY),
        ConstGenericImpactSummary{false, true, true, true, false, false, false, false, false},
        {
            "generic template arity must distinguish type and const parameters",
            "generic side tables currently map identifiers to TypeHandle placeholders only",
            "incremental cache must invalidate when const argument value changes",
        },
        {
            "generic_param_kind_const_key",
            "generic_instance_const_arg_key",
            "generic_param_env_const_binding_fact",
        },
        {
            std::string(QUERY_CONST_GENERIC_M15_NO_STD_NON_GOAL),
            std::string(QUERY_CONST_GENERIC_M15_NO_RUNTIME_NON_GOAL),
            "do_not_reuse_type_placeholder_for_const_params",
            "do_not_make_const_args_part_of_display_name_only",
        },
    };
}

ConstGenericDesignCandidate make_array_length_type_candidate()
{
    return ConstGenericDesignCandidate{
        ConstGenericCapability::array_length_type_integration,
        ConstGenericGateStage::ready_for_implementation,
        ConstGenericPolicyDecision::selected_m15_frontend_query_path,
        std::string(QUERY_CONST_GENERIC_ARRAY_LENGTH_POLICY),
        ConstGenericImpactSummary{true, true, true, true, true, false, false, false, false},
        {
            "array length syntax currently accepts literal lengths only",
            "layout and ABI need resolved length before IR lowering",
            "generic [N]T must preserve N in canonical type and layout fingerprints",
        },
        {
            "array_length_const_param_fact",
            "array_type_const_length_key",
            "array_layout_const_length_fingerprint",
        },
        {
            std::string(QUERY_CONST_GENERIC_M15_NO_STD_NON_GOAL),
            std::string(QUERY_CONST_GENERIC_M15_NO_RUNTIME_NON_GOAL),
            "do_not_support_generic_arithmetic_array_lengths_in_m15",
            "do_not_lower_unresolved_array_lengths_to_ir",
        },
    };
}

ConstGenericDesignCandidate make_const_expression_subset_candidate()
{
    return ConstGenericDesignCandidate{
        ConstGenericCapability::const_expression_evaluation_subset,
        ConstGenericGateStage::blocked_by_dependency,
        ConstGenericPolicyDecision::requires_comptime_engine,
        std::string(QUERY_CONST_GENERIC_COMPTIME_SUBSET_POLICY),
        ConstGenericImpactSummary{false, true, true, true, true, true, false, false, false},
        {
            "current const initializer evaluator is not a general comptime engine",
            "generic arithmetic such as N + 1 needs normalized expression identity",
            "overflow and target-width semantics must match ordinary constant evaluation",
        },
        {
            "const_expr_normal_form_fact",
            "const_eval_dependency_fact",
            "const_eval_overflow_policy_fact",
        },
        {
            std::string(QUERY_CONST_GENERIC_M15_NO_STD_NON_GOAL),
            std::string(QUERY_CONST_GENERIC_M15_NO_RUNTIME_NON_GOAL),
            "do_not_support_generic_const_arithmetic_in_m15",
            "do_not_evaluate_user_functions_in_const_generic_args",
        },
    };
}

ConstGenericDesignCandidate make_trait_predicate_boundary_candidate()
{
    return ConstGenericDesignCandidate{
        ConstGenericCapability::trait_predicate_and_dyn_boundary,
        ConstGenericGateStage::future_stage,
        ConstGenericPolicyDecision::requires_trait_solver_extension,
        std::string(QUERY_CONST_GENERIC_TRAIT_BOUNDARY_POLICY),
        ConstGenericImpactSummary{false, true, true, true, false, false, true, false, false},
        {
            "trait predicates currently bind type parameters and associated type equalities",
            "const equality predicates need solver support before trait-level use",
            "dyn associated equality dispatch must not infer value-level const predicates",
        },
        {
            "const_generic_trait_predicate_fact",
            "const_equality_obligation_fact",
            "dyn_const_generic_boundary_fact",
        },
        {
            std::string(QUERY_CONST_GENERIC_M15_NO_STD_NON_GOAL),
            std::string(QUERY_CONST_GENERIC_M15_NO_DYN_NON_GOAL),
            "do_not_add_const_associated_values_to_dyn_trait_in_m15",
            "do_not_support_const_where_predicates_in_m15",
        },
    };
}

} // namespace

std::string_view const_generic_capability_name(const ConstGenericCapability capability) noexcept
{
    switch (capability) {
        case ConstGenericCapability::typed_const_parameter_surface:
            return "typed_const_parameter_surface";
        case ConstGenericCapability::canonical_const_argument_identity:
            return "canonical_const_argument_identity";
        case ConstGenericCapability::generic_instance_key_integration:
            return "generic_instance_key_integration";
        case ConstGenericCapability::array_length_type_integration:
            return "array_length_type_integration";
        case ConstGenericCapability::const_expression_evaluation_subset:
            return "const_expression_evaluation_subset";
        case ConstGenericCapability::trait_predicate_and_dyn_boundary:
            return "trait_predicate_and_dyn_boundary";
    }
    return "invalid";
}

std::string_view const_generic_gate_stage_name(const ConstGenericGateStage stage) noexcept
{
    switch (stage) {
        case ConstGenericGateStage::research_only:
            return "research_only";
        case ConstGenericGateStage::design_gate:
            return "design_gate";
        case ConstGenericGateStage::ready_for_implementation:
            return "ready_for_implementation";
        case ConstGenericGateStage::blocked_by_dependency:
            return "blocked_by_dependency";
        case ConstGenericGateStage::future_stage:
            return "future_stage";
    }
    return "invalid";
}

std::string_view const_generic_policy_decision_name(
    const ConstGenericPolicyDecision decision) noexcept
{
    switch (decision) {
        case ConstGenericPolicyDecision::rejected:
            return "rejected";
        case ConstGenericPolicyDecision::selected_m15_frontend_query_path:
            return "selected_m15_frontend_query_path";
        case ConstGenericPolicyDecision::requires_comptime_engine:
            return "requires_comptime_engine";
        case ConstGenericPolicyDecision::requires_trait_solver_extension:
            return "requires_trait_solver_extension";
        case ConstGenericPolicyDecision::requires_runtime_or_std_boundary:
            return "requires_runtime_or_std_boundary";
    }
    return "invalid";
}

bool is_valid(const ConstGenericCapability capability) noexcept
{
    switch (capability) {
        case ConstGenericCapability::typed_const_parameter_surface:
        case ConstGenericCapability::canonical_const_argument_identity:
        case ConstGenericCapability::generic_instance_key_integration:
        case ConstGenericCapability::array_length_type_integration:
        case ConstGenericCapability::const_expression_evaluation_subset:
        case ConstGenericCapability::trait_predicate_and_dyn_boundary:
            return true;
    }
    return false;
}

bool is_valid(const ConstGenericGateStage stage) noexcept
{
    switch (stage) {
        case ConstGenericGateStage::research_only:
        case ConstGenericGateStage::design_gate:
        case ConstGenericGateStage::ready_for_implementation:
        case ConstGenericGateStage::blocked_by_dependency:
        case ConstGenericGateStage::future_stage:
            return true;
    }
    return false;
}

bool is_valid(const ConstGenericPolicyDecision decision) noexcept
{
    switch (decision) {
        case ConstGenericPolicyDecision::rejected:
        case ConstGenericPolicyDecision::selected_m15_frontend_query_path:
        case ConstGenericPolicyDecision::requires_comptime_engine:
        case ConstGenericPolicyDecision::requires_trait_solver_extension:
        case ConstGenericPolicyDecision::requires_runtime_or_std_boundary:
            return true;
    }
    return false;
}

bool is_valid(const ConstGenericDesignCandidate& candidate) noexcept
{
    return candidate_shape_is_valid(candidate) && m15_capability_gate_is_valid(candidate)
        && contains_text(candidate.non_goals, QUERY_CONST_GENERIC_M15_NO_STD_NON_GOAL);
}

bool is_valid(const ConstGenericDesignGate& gate) noexcept
{
    return is_valid_m15_const_generic_design_gate(gate);
}

bool is_valid_m15_const_generic_design_gate(const ConstGenericDesignGate& gate) noexcept
{
    return std::string_view(gate.name) == QUERY_CONST_GENERIC_M15_GATE_NAME
        && gate_has_each_const_generic_capability_once(gate)
        && std::all_of(gate.candidates.begin(), gate.candidates.end(),
            [](const ConstGenericDesignCandidate& candidate) {
                return is_valid(candidate);
            });
}

void record_const_generic_design_candidate(
    ConstGenericDesignGate& gate, ConstGenericDesignCandidate candidate)
{
    gate.candidates.push_back(std::move(candidate));
}

StableFingerprint128 const_generic_design_gate_fingerprint(
    const ConstGenericDesignGate& gate) noexcept
{
    StableHashBuilder builder;
    builder.mix_string(QUERY_CONST_GENERIC_GATE_FINGERPRINT_MARKER);
    builder.mix_string(gate.name);
    builder.mix_u64(gate.candidates.size());
    for (const ConstGenericDesignCandidate& candidate : gate.candidates) {
        mix_candidate(builder, candidate);
    }
    return builder.finish();
}

std::string summarize_const_generic_design_gate(const ConstGenericDesignGate& gate)
{
    base::u64 ready_count = 0;
    base::u64 blocked_count = 0;
    base::u64 future_count = 0;
    base::u64 query_key_impact_count = 0;
    base::u64 ir_layout_impact_count = 0;
    for (const ConstGenericDesignCandidate& candidate : gate.candidates) {
        if (candidate.stage == ConstGenericGateStage::ready_for_implementation) {
            ++ready_count;
        }
        if (candidate.stage == ConstGenericGateStage::blocked_by_dependency) {
            ++blocked_count;
        }
        if (candidate.stage == ConstGenericGateStage::future_stage) {
            ++future_count;
        }
        if (candidate.impact.query_key_impact) {
            ++query_key_impact_count;
        }
        if (candidate.impact.ir_layout_impact) {
            ++ir_layout_impact_count;
        }
    }

    std::ostringstream label;
    label << "const_generic_design_gate name="
          << (gate.name.empty() ? "<anonymous>" : gate.name)
          << " candidates=" << gate.candidates.size()
          << " ready_for_implementation=" << ready_count
          << " blocked_by_dependency=" << blocked_count
          << " future_stage=" << future_count
          << " query_key_impact=" << query_key_impact_count
          << " ir_layout_impact=" << ir_layout_impact_count
          << " fingerprint=" << debug_string(const_generic_design_gate_fingerprint(gate));
    return label.str();
}

std::string dump_const_generic_design_gate(const ConstGenericDesignGate& gate)
{
    std::ostringstream stream;
    stream << "const_generic_design_gate name="
           << (gate.name.empty() ? "<anonymous>" : gate.name)
           << " candidates=" << gate.candidates.size()
           << " fingerprint=" << debug_string(const_generic_design_gate_fingerprint(gate)) << '\n';
    for (base::usize index = 0; index < gate.candidates.size(); ++index) {
        const ConstGenericDesignCandidate& candidate = gate.candidates[index];
        stream << "  candidate #" << index
               << " capability=" << const_generic_capability_name(candidate.capability)
               << " stage=" << const_generic_gate_stage_name(candidate.stage)
               << " decision=" << const_generic_policy_decision_name(candidate.decision)
               << " selected_policy=" << candidate.selected_policy;
        append_impact_summary(stream, candidate.impact);
        stream << '\n';
        for (const std::string& blocker : candidate.blockers) {
            stream << "    blocker=" << blocker << '\n';
        }
        for (const std::string& fact : candidate.required_facts) {
            stream << "    required_fact=" << fact << '\n';
        }
        for (const std::string& non_goal : candidate.non_goals) {
            stream << "    non_goal=" << non_goal << '\n';
        }
    }
    return stream.str();
}

ConstGenericDesignGate m15_const_generic_design_gate_baseline()
{
    ConstGenericDesignGate gate;
    gate.name = std::string(QUERY_CONST_GENERIC_M15_GATE_NAME);
    record_const_generic_design_candidate(gate, make_typed_const_parameter_candidate());
    record_const_generic_design_candidate(gate, make_canonical_const_argument_candidate());
    record_const_generic_design_candidate(gate, make_generic_instance_key_candidate());
    record_const_generic_design_candidate(gate, make_array_length_type_candidate());
    record_const_generic_design_candidate(gate, make_const_expression_subset_candidate());
    record_const_generic_design_candidate(gate, make_trait_predicate_boundary_candidate());
    gate.fingerprint = const_generic_design_gate_fingerprint(gate);
    return gate;
}

} // namespace aurex::query
