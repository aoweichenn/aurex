#pragma once

#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/frontend/sema/resource_semantics.hpp>

#include <optional>
#include <unordered_map>
#include <vector>

#include <frontend/sema/internal/core/private/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore::PlaceStateAnalyzer final {
public:
    explicit PlaceStateAnalyzer(SemanticAnalyzerCore& core) noexcept;

    void analyze(const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature);

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

    void collect_place_facts(const BodyFlowGraph& graph);
    void collect_action_events(const BodyFlowGraph& graph);
    void apply_event_to_fact(PlaceStateFact& fact, PlaceStateEventKind kind, const BodyFlowAction& action);

    SemanticAnalyzerCore& core_;
    const FunctionSignature* signature_ = nullptr;
    FunctionLookupKey key_;
    FunctionPlaceStateFacts facts_;
    ResourceSemanticsClassifier resources_;
    std::unordered_map<base::u32, TypeHandle> local_types_by_name_;
    std::unordered_map<base::u32, bool> needs_drop_by_type_;
};

[[nodiscard]] std::string_view place_state_initialization_name(PlaceStateInitialization state) noexcept;
[[nodiscard]] std::string_view place_state_move_state_name(PlaceStateMoveState state) noexcept;
[[nodiscard]] std::string_view place_state_drop_state_name(PlaceStateDropState state) noexcept;
[[nodiscard]] std::string_view place_state_event_kind_name(PlaceStateEventKind kind) noexcept;
[[nodiscard]] query::StableFingerprint128 function_place_state_facts_fingerprint(
    const FunctionPlaceStateFacts& facts) noexcept;
[[nodiscard]] std::string dump_function_place_state_facts(const FunctionPlaceStateFacts& facts);

} // namespace aurex::sema
