#pragma once

#include <aurex/frontend/sema/checked_module.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <frontend/sema/internal/core/private/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore::LifetimeAnalyzer final {
public:
    explicit LifetimeAnalyzer(SemanticAnalyzerCore& core) noexcept;

    void analyze_signature(
        const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature);
    void analyze_body(
        const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature);

private:
    struct RegionSet {
        std::vector<base::u32> regions;
        bool unknown = false;
    };

    struct OriginName {
        std::string name;
        IdentId name_id = INVALID_IDENT_ID;
        base::SourceRange range{};
    };

    struct ViolationKey {
        LifetimeViolationKind kind = LifetimeViolationKind::unknown_origin;
        base::u32 region = SEMA_LIFETIME_INVALID_INDEX;
        base::u32 related_region = SEMA_LIFETIME_INVALID_INDEX;
        base::u32 type = INVALID_TYPE_HANDLE.value;
        base::u32 expr = syntax::INVALID_EXPR_ID.value;

        [[nodiscard]] friend bool operator==(const ViolationKey& lhs, const ViolationKey& rhs) noexcept = default;
    };

    struct ViolationKeyHash {
        [[nodiscard]] std::size_t operator()(const ViolationKey& key) const noexcept;
    };

    void analyze(const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature,
        bool include_body_facts);
    void reset(const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature,
        bool include_body_facts);
    [[nodiscard]] syntax::ItemId signature_item() const noexcept;
    [[nodiscard]] bool fact_belongs_to_current_item(const ReferenceOriginFact& fact) const noexcept;
    [[nodiscard]] bool has_declared_borrow_contract() const noexcept;
    [[nodiscard]] bool has_declared_origin_params() const noexcept;
    [[nodiscard]] bool boundary_requires_explicit_contract() const noexcept;
    [[nodiscard]] bool function_has_body_flow_graph() const noexcept;

    [[nodiscard]] base::u32 add_region(
        LifetimeRegionKind kind, IdentId name_id, std::string_view name, base::u32 param_index,
        const base::SourceRange& range);
    [[nodiscard]] base::u32 static_region();
    [[nodiscard]] base::u32 unknown_region(const base::SourceRange& range);
    [[nodiscard]] base::u32 local_region(IdentId name_id, const base::SourceRange& range);
    [[nodiscard]] base::u32 temporary_region(const base::SourceRange& range);
    [[nodiscard]] base::u32 parameter_region(base::usize index);
    [[nodiscard]] std::optional<base::u32> find_declared_origin(std::string_view name) const;
    [[nodiscard]] base::u32 declared_or_unknown_origin(
        std::string_view name, IdentId name_id, const base::SourceRange& range, bool emit_unknown_violation);
    void register_declared_origin(std::string_view name, base::u32 region);
    void sort_unique(RegionSet& set) const;

    void collect_origin_params();
    void collect_parameter_regions();
    void collect_reference_origin_facts();
    void collect_signature_type_lifetime_infos();
    void record_type_lifetime_info(TypeHandle type, LifetimeConstraintReason reason, const base::SourceRange& range);
    void record_generic_lifetime_predicate(
        TypeHandle type, std::string_view origin, IdentId origin_id, GenericLifetimePredicateSource source,
        const base::SourceRange& range);
    void collect_reference_type_constraints(TypeHandle reference_type, base::u32 region,
        LifetimeConstraintReason reason, const base::SourceRange& range);
    void collect_return_regions();
    void collect_return_regions_from_summary(const FunctionBorrowSummary& summary);
    void collect_return_regions_from_contract(const FunctionBorrowContract& contract);
    void collect_storage_escape_regions();
    void collect_storage_escape_region(
        const FunctionBorrowSummary& summary, const FunctionBorrowStorageEscape& escape);
    [[nodiscard]] bool return_regions_contain_unknown() const noexcept;
    void append_unknown_return_region(syntax::ExprId expr, const base::SourceRange& range);
    [[nodiscard]] RegionSet regions_for_summary_origin(const BorrowSummaryOrigin& origin);
    [[nodiscard]] RegionSet regions_for_contract_selector(const BorrowContractSelector& selector);
    [[nodiscard]] RegionSet regions_for_param_type_origin(base::usize param_index);
    [[nodiscard]] RegionSet regions_for_type_origin_key(TypeHandle type, const base::SourceRange& range);
    [[nodiscard]] std::vector<OriginName> origin_names_for_type(TypeHandle type) const;
    [[nodiscard]] bool type_can_contain_borrow(TypeHandle type) const;
    [[nodiscard]] bool type_has_concrete_borrow_surface(TypeHandle type) const;
    void append_return_region(base::u32 region, syntax::ExprId expr, const base::SourceRange& range);

    void solve();
    void initialize_outlives_graph();
    void collect_body_live_ranges();
    [[nodiscard]] base::u32 first_body_flow_point(const BodyFlowGraph& graph) const noexcept;
    [[nodiscard]] base::u32 last_body_flow_point(const BodyFlowGraph& graph) const noexcept;
    [[nodiscard]] base::u32 return_body_flow_point(const BodyFlowGraph& graph, syntax::ExprId expr) const noexcept;
    void append_live_range(base::u32 region, base::u32 first_point, base::u32 last_point,
        base::u32 point_count, const base::SourceRange& range);
    void cache_outlives_reachability(base::u32 longer);
    [[nodiscard]] bool region_outlives(base::u32 longer, base::u32 shorter);
    void enforce_return_origin_subset();
    void enforce_ambiguous_elision();
    void enforce_type_outlives();
    void enforce_diagnostics();
    void add_violation(LifetimeViolationKind kind, base::u32 region, base::u32 related_region, TypeHandle type,
        syntax::ExprId expr, bool diagnostic_emitted, const base::SourceRange& range);
    void finalize_facts();

    SemanticAnalyzerCore& core_;
    const syntax::ItemNode* function_ = nullptr;
    const FunctionSignature* signature_ = nullptr;
    FunctionLookupKey key_;
    FunctionLifetimeFacts facts_;
    syntax::ItemId item_ = syntax::INVALID_ITEM_ID;
    std::unordered_map<std::string, base::u32> declared_origin_regions_;
    std::unordered_map<std::string, base::u32> region_lookup_;
    std::vector<base::u32> parameter_regions_;
    std::vector<std::vector<base::u32>> outlives_successors_;
    std::vector<std::vector<bool>> outlives_reachability_cache_;
    std::vector<bool> outlives_reachability_cached_;
    std::unordered_map<ViolationKey, base::usize, ViolationKeyHash> violation_lookup_;
    mutable std::unordered_map<base::u32, bool> type_borrow_cache_;
    mutable std::unordered_map<base::u32, bool> concrete_borrow_surface_cache_;
    bool include_body_facts_ = false;
};

} // namespace aurex::sema
