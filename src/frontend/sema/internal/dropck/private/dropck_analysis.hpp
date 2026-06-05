#pragma once

#include <aurex/frontend/sema/checked_module.hpp>
#include <aurex/frontend/sema/drop_glue.hpp>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <frontend/sema/internal/core/private/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore::DropCheckAnalyzer final {
public:
    explicit DropCheckAnalyzer(SemanticAnalyzerCore& core) noexcept;

    void analyze(const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature);

private:
    struct PatternTypeFrame {
        syntax::PatternId pattern = syntax::INVALID_PATTERN_ID;
        TypeHandle type = INVALID_TYPE_HANDLE;
    };

    struct TypeFrame {
        TypeHandle type = INVALID_TYPE_HANDLE;
    };

    struct DropGlueCacheEntry {
        DropGluePlan plan;
    };

    struct TypeOutlivesKey {
        base::u32 type = TypeHandle::INVALID_VALUE;
        base::u32 region = SEMA_LIFETIME_INVALID_INDEX;

        [[nodiscard]] friend bool operator==(const TypeOutlivesKey& lhs, const TypeOutlivesKey& rhs) noexcept =
            default;
    };

    struct TypeOutlivesKeyHash {
        [[nodiscard]] std::size_t operator()(const TypeOutlivesKey& key) const noexcept;
    };

    struct DropCheckViolationKey {
        base::u8 kind = 0;
        base::u32 action = SEMA_DROP_CHECK_INVALID_ACTION_INDEX;
        base::u32 point = SEMA_BODY_FLOW_INVALID_INDEX;
        base::u32 place = SEMA_BODY_FLOW_INVALID_INDEX;
        base::u32 type = TypeHandle::INVALID_VALUE;
        base::u32 region = SEMA_LIFETIME_INVALID_INDEX;

        [[nodiscard]] friend bool operator==(const DropCheckViolationKey& lhs,
            const DropCheckViolationKey& rhs) noexcept = default;
    };

    struct DropCheckViolationKeyHash {
        [[nodiscard]] std::size_t operator()(const DropCheckViolationKey& key) const noexcept;
    };

    void reset(const FunctionLookupKey& key, const FunctionSignature& signature);
    void collect(const syntax::ItemNode& function);
    void solve();
    void enforce();
    void finalize();

    void build_local_type_index(const syntax::ItemNode& function);
    [[nodiscard]] bool local_dropck_inputs_may_need_graph();
    void index_statement_locals(syntax::StmtId stmt);
    void index_pattern_bindings(syntax::PatternId pattern, TypeHandle type);
    [[nodiscard]] TypeHandle pattern_field_type(TypeHandle owner, IdentId field_name_id) const noexcept;
    [[nodiscard]] TypeHandle pattern_element_type(TypeHandle owner, base::usize index) const noexcept;
    [[nodiscard]] const EnumCaseInfo* pattern_enum_case(TypeHandle owner, const syntax::PatternNode& pattern) const;

    [[nodiscard]] const BodyFlowGraph* body_graph() const noexcept;
    [[nodiscard]] const FunctionLifetimeFacts* lifetime_facts() const noexcept;
    [[nodiscard]] FunctionLifetimeFacts* mutable_lifetime_facts() noexcept;
    [[nodiscard]] TypeHandle action_type(const BodyFlowGraph& graph, const BodyFlowAction& action) const;
    [[nodiscard]] TypeHandle place_type(const BodyFlowGraph& graph, const BodyFlowPlace& place) const;
    [[nodiscard]] TypeHandle projected_type(TypeHandle current, const BodyFlowPlaceProjection& projection) const;
    [[nodiscard]] std::optional<DropCheckActionKind> action_kind(BodyFlowActionKind kind) const noexcept;
    [[nodiscard]] const DropGlueCacheEntry& cached_drop_glue(TypeHandle type);
    [[nodiscard]] const DropGlueStep* custom_destructor_step_for_type(
        const DropGluePlan& plan, TypeHandle type) const noexcept;
    [[nodiscard]] query::StableFingerprint128 destructor_key_for_plan(
        TypeHandle type, const DropGluePlan& plan) const;
    [[nodiscard]] bool type_can_contain_borrow(TypeHandle type) const;
    [[nodiscard]] bool valid_type(TypeHandle type) const noexcept;

    void append_drop_action(const DropCheckActionKind kind, base::u32 action_index, const BodyFlowAction& action,
        TypeHandle type, const query::StableFingerprint128& destructor_key);
    void append_drop_fact(TypeHandle type, const DropGluePlan& plan);
    [[nodiscard]] bool append_required_outlives(
        DropCheckFact& fact, TypeHandle type, base::u32 region, const base::SourceRange& range);
    [[nodiscard]] bool append_violation(DropCheckViolationKind kind, const DropActionFact& action, base::u32 region,
        bool diagnostic_emitted = false);
    [[nodiscard]] bool append_lifetime_type_outlives(
        TypeHandle type, base::u32 region, const base::SourceRange& range);
    void build_active_region_index(const BodyFlowGraph& graph);
    [[nodiscard]] std::span<const base::u32> active_regions_at_point(base::u32 point) const noexcept;
    void build_lifetime_region_origin_index();
    [[nodiscard]] std::span<const base::u32> concrete_origin_regions_for_type(TypeHandle type) const;
    void enforce_destructor_observed_borrow_safety(
        const DropActionFact& action, const DropGluePlan& plan, std::span<const base::u32> active_regions);

    SemanticAnalyzerCore& core_;
    const FunctionSignature* signature_ = nullptr;
    FunctionLookupKey key_;
    FunctionDropCheckFacts facts_;
    std::unordered_map<base::u32, TypeHandle> local_types_by_name_;
    std::unordered_map<base::u32, DropGlueCacheEntry> drop_glue_cache_;
    std::unordered_map<base::u32, base::usize> fact_by_type_;
    mutable std::unordered_map<base::u32, bool> type_borrow_cache_;
    std::unordered_set<TypeOutlivesKey, TypeOutlivesKeyHash> emitted_type_outlives_;
    std::unordered_set<DropCheckViolationKey, DropCheckViolationKeyHash> violation_dedupe_;
    std::unordered_map<base::u32, std::vector<base::u32>> active_regions_by_point_;
    std::unordered_map<std::string_view, base::u32> lifetime_region_by_origin_name_;
    mutable std::unordered_map<base::u32, std::vector<base::u32>> concrete_origin_regions_by_type_;
};

[[nodiscard]] std::string_view drop_check_action_kind_name(DropCheckActionKind kind) noexcept;
[[nodiscard]] std::string_view drop_check_violation_kind_name(DropCheckViolationKind kind) noexcept;
[[nodiscard]] query::StableFingerprint128 drop_check_fact_fingerprint(const DropCheckFact& fact) noexcept;
[[nodiscard]] query::StableFingerprint128 function_drop_check_facts_fingerprint(
    const FunctionDropCheckFacts& facts) noexcept;
[[nodiscard]] std::string dump_function_drop_check_facts(const FunctionDropCheckFacts& facts);

} // namespace aurex::sema
