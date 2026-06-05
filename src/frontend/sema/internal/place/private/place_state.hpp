#pragma once

#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/frontend/sema/resource_semantics.hpp>

#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <frontend/sema/internal/core/private/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore::PlaceStateAnalyzer final {
public:
    explicit PlaceStateAnalyzer(SemanticAnalyzerCore& core) noexcept;

    void analyze(const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature);
    [[nodiscard]] bool may_need_check(const syntax::ItemNode& function, const FunctionSignature& signature);

private:
    struct PatternTypeFrame {
        syntax::PatternId pattern = syntax::INVALID_PATTERN_ID;
        TypeHandle type = INVALID_TYPE_HANDLE;
    };

    void reset(const FunctionLookupKey& key, const FunctionSignature& signature);
    void collect(const syntax::ItemNode& function);
    void finalize();

    void build_local_type_index(const syntax::ItemNode& function);
    void index_statement_locals(syntax::StmtId stmt);
    void index_pattern_bindings(syntax::PatternId pattern, TypeHandle type);
    [[nodiscard]] TypeHandle pattern_field_type(TypeHandle owner, IdentId field_name_id) const noexcept;
    [[nodiscard]] TypeHandle pattern_element_type(TypeHandle owner, base::usize index) const noexcept;
    [[nodiscard]] const EnumCaseInfo* pattern_enum_case(TypeHandle owner, const syntax::PatternNode& pattern) const;

    [[nodiscard]] const BodyFlowGraph* body_graph() const noexcept;
    [[nodiscard]] TypeHandle action_type(const BodyFlowGraph& graph, const BodyFlowAction& action) const;
    [[nodiscard]] TypeHandle place_type(const BodyFlowGraph& graph, const BodyFlowPlace& place) const;
    [[nodiscard]] TypeHandle projected_type(TypeHandle current, const BodyFlowPlaceProjection& projection) const;
    [[nodiscard]] std::optional<PlaceStateEventKind> event_kind(BodyFlowActionKind kind) const noexcept;
    [[nodiscard]] bool type_needs_drop(TypeHandle type);
    [[nodiscard]] bool valid_type(TypeHandle type) const noexcept;
    [[nodiscard]] bool is_parameter_name(IdentId name) const noexcept;

    void collect_place_facts(const BodyFlowGraph& graph);
    void collect_action_events(const BodyFlowGraph& graph);
    void apply_event_to_fact(PlaceStateFact& fact, PlaceStateEventKind kind, const BodyFlowAction& action);
    void solve_place_states(const BodyFlowGraph& graph);
    void enforce_diagnostics();

    SemanticAnalyzerCore& core_;
    const FunctionSignature* signature_ = nullptr;
    FunctionLookupKey key_;
    FunctionPlaceStateFacts facts_;
    ResourceSemanticsClassifier resources_;
    std::unordered_map<base::u32, TypeHandle> local_types_by_name_;
    std::unordered_map<base::u32, bool> needs_drop_by_type_;
    std::unordered_set<base::u32> parameter_name_ids_;
    std::vector<base::u32> graph_place_to_fact_place_;
    std::vector<base::u32> parent_place_by_place_;
    std::vector<std::vector<base::u32>> child_places_by_place_;
};

[[nodiscard]] std::string_view place_state_initialization_name(PlaceStateInitialization state) noexcept;
[[nodiscard]] std::string_view place_state_move_state_name(PlaceStateMoveState state) noexcept;
[[nodiscard]] std::string_view place_state_drop_state_name(PlaceStateDropState state) noexcept;
[[nodiscard]] std::string_view place_state_event_kind_name(PlaceStateEventKind kind) noexcept;
[[nodiscard]] std::string_view place_state_violation_kind_name(PlaceStateViolationKind kind) noexcept;
[[nodiscard]] std::string_view place_state_violation_message(PlaceStateViolationKind kind) noexcept;
[[nodiscard]] query::StableFingerprint128 function_place_state_facts_fingerprint(
    const FunctionPlaceStateFacts& facts) noexcept;
[[nodiscard]] std::string dump_function_place_state_facts(const FunctionPlaceStateFacts& facts);

} // namespace aurex::sema
