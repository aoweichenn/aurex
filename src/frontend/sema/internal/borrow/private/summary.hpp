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

class SemanticAnalyzerCore::BorrowSummaryBuilder final {
public:
    explicit BorrowSummaryBuilder(SemanticAnalyzerCore& core) noexcept;

    void build(const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature);

private:
    struct OriginSet {
        std::vector<base::u32> origins;
        bool unknown = false;
    };

    struct Scope {
        std::unordered_map<IdentId, OriginSet, IdentIdHash> storage;
        std::unordered_map<IdentId, OriginSet, IdentIdHash> borrowed_values;
        std::unordered_map<IdentId, OriginSet, IdentIdHash> pointer_values;
    };
    using ScopeList = std::vector<Scope>;

    enum class TaskKind : base::u8 {
        scoped_block,
        block_statements,
        statement,
        pop_scope,
    };

    struct Task {
        TaskKind kind = TaskKind::statement;
        syntax::StmtId stmt = syntax::INVALID_STMT_ID;
    };

    struct PatternFrame {
        syntax::PatternId pattern = syntax::INVALID_PATTERN_ID;
        TypeHandle type = INVALID_TYPE_HANDLE;
        syntax::ExprId source = syntax::INVALID_EXPR_ID;
    };

    void reset(const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature);
    void bind_parameters();
    void push_scope();
    void pop_scope();
    void bind_storage(IdentId name, OriginSet origin);
    void bind_borrowed_value(IdentId name, OriginSet origin);
    void bind_pointer_value(IdentId name, OriginSet origin);
    void assign_borrowed_value(IdentId name, OriginSet origin);
    void assign_pointer_value(IdentId name, OriginSet origin);
    [[nodiscard]] OriginSet lookup_storage(IdentId name) const;
    [[nodiscard]] OriginSet lookup_borrowed_value(IdentId name) const;
    [[nodiscard]] OriginSet lookup_pointer_value(IdentId name) const;
    [[nodiscard]] base::u32 append_origin(BorrowSummaryOrigin origin);
    [[nodiscard]] OriginSet parameter_origin(base::usize index, const syntax::ParamDecl& param);
    [[nodiscard]] OriginSet static_origin(const base::SourceRange& range);
    [[nodiscard]] OriginSet local_origin(IdentId name, const base::SourceRange& range, syntax::ExprId expr);
    [[nodiscard]] OriginSet temporary_origin(syntax::ExprId expr);
    [[nodiscard]] OriginSet unknown_origin() const;
    [[nodiscard]] OriginSet merge(OriginSet lhs, const OriginSet& rhs) const;
    void sort_unique(OriginSet& origin) const;
    void push_scoped_block(std::vector<Task>& tasks, syntax::StmtId block) const;
    void push_statement(std::vector<Task>& tasks, syntax::StmtId stmt) const;
    void run_tasks(std::vector<Task>& tasks);
    void push_block_statements(std::vector<Task>& tasks, syntax::StmtId block) const;
    void analyze_statement(std::vector<Task>& tasks, syntax::StmtId stmt);
    void analyze_if_statement(std::vector<Task>& tasks, const syntax::StmtNode& stmt);
    void analyze_while_statement(std::vector<Task>& tasks, const syntax::StmtNode& stmt);
    void analyze_for_statement(std::vector<Task>& tasks, const syntax::StmtNode& stmt);
    void analyze_for_range_statement(std::vector<Task>& tasks, const syntax::StmtNode& stmt);
    [[nodiscard]] ScopeList analyze_tasks_from_scopes(const ScopeList& baseline, std::vector<Task> tasks);
    [[nodiscard]] ScopeList merge_control_flow_scopes(
        const ScopeList& baseline, const std::vector<ScopeList>& branches, bool include_baseline_path);
    void merge_scope_borrowed_values(Scope& target, const Scope& branch);
    void merge_scope_pointer_values(Scope& target, const Scope& branch);
    void clear_borrowed_values_for_existing_storage(ScopeList& scopes) const;
    void clear_pointer_values_for_existing_storage(ScopeList& scopes) const;
    void analyze_local_declaration(syntax::StmtId stmt_id, const syntax::StmtNode& stmt);
    void analyze_assignment(const syntax::StmtNode& stmt);
    void bind_pattern_storage(syntax::PatternId pattern, TypeHandle type, syntax::ExprId source);
    void bind_pattern_binding(const syntax::PatternNode& pattern, TypeHandle type, syntax::ExprId source);
    void push_tuple_pattern_frames(std::vector<PatternFrame>& pending, const syntax::PatternNode& pattern,
        TypeHandle type, syntax::ExprId source) const;
    void push_slice_pattern_frames(std::vector<PatternFrame>& pending, const syntax::PatternNode& pattern,
        TypeHandle type, syntax::ExprId source) const;
    void push_struct_pattern_frames(std::vector<PatternFrame>& pending, const syntax::PatternNode& pattern,
        TypeHandle type, syntax::ExprId source) const;
    void push_enum_pattern_frames(std::vector<PatternFrame>& pending, const syntax::PatternNode& pattern,
        TypeHandle type, syntax::ExprId source) const;
    [[nodiscard]] syntax::ExprId struct_field_source(
        const syntax::StructLiteralExprPayload* source, IdentId field_name, syntax::ExprId fallback) const noexcept;
    [[nodiscard]] const StructFieldInfo* pattern_struct_field(
        const StructInfo& structure, IdentId field_name) const noexcept;
    [[nodiscard]] IdentId unqualified_name_id(syntax::ExprId expr) const noexcept;
    [[nodiscard]] OriginSet borrowed_name_origin(syntax::ExprId expr) const;
    [[nodiscard]] OriginSet pointer_name_origin(syntax::ExprId expr) const;
    [[nodiscard]] OriginSet storage_name_origin(syntax::ExprId expr) const;
    [[nodiscard]] OriginSet borrowed_carrier_origin(syntax::ExprId expr);
    [[nodiscard]] OriginSet addressed_place_origin(syntax::ExprId expr);
    [[nodiscard]] OriginSet place_storage_origin(syntax::ExprId expr);
    [[nodiscard]] OriginSet slice_source_origin(syntax::ExprId expr);
    [[nodiscard]] OriginSet call_return_origin(syntax::ExprId expr, const syntax::CallExprPayload& call);
    [[nodiscard]] std::optional<OriginSet> enum_constructor_return_origin(const syntax::CallExprPayload& call);
    [[nodiscard]] OriginSet pointer_origin(syntax::ExprId expr);
    [[nodiscard]] const FunctionCallBinding* direct_call_binding(syntax::ExprId call_expr) const noexcept;
    [[nodiscard]] const TraitMethodCallBinding* trait_call_binding(syntax::ExprId call_expr) const noexcept;
    [[nodiscard]] syntax::ExprId call_argument_for_param(const syntax::CallExprPayload& call, syntax::ExprId callee,
        base::u32 receiver_arg_count, base::u32 param_index) const;
    [[nodiscard]] OriginSet call_origin_for_param(const syntax::CallExprPayload& call, syntax::ExprId callee,
        base::u32 receiver_arg_count, base::u32 param_index, bool receiver_auto_borrow);
    [[nodiscard]] OriginSet map_callee_summary_origins(const FunctionBorrowSummary& callee,
        const syntax::CallExprPayload& call, syntax::ExprId callee_expr, base::u32 receiver_arg_count,
        bool receiver_auto_borrow);
    [[nodiscard]] OriginSet map_callee_contract_origins(const FunctionBorrowContract& callee,
        const syntax::CallExprPayload& call, syntax::ExprId callee_expr, base::u32 receiver_arg_count,
        bool receiver_auto_borrow);
    [[nodiscard]] OriginSet borrow_origin(syntax::ExprId expr);
    [[nodiscard]] bool push_expression_children_for_origin(
        std::vector<syntax::ExprId>& pending, syntax::ExprId expr, syntax::ExprKind kind);
    [[nodiscard]] OriginSet block_result_borrow_origin(const syntax::BlockExprPayload& block);
    [[nodiscard]] bool type_can_contain_borrow(TypeHandle type) const;
    void record_return_origin(syntax::ExprId expr, const base::SourceRange& range);
    void finalize_summary();
    [[nodiscard]] query::StableFingerprint128 fingerprint_summary() const noexcept;

    SemanticAnalyzerCore& core_;
    const syntax::ItemNode* function_ = nullptr;
    const FunctionSignature* signature_ = nullptr;
    FunctionBorrowSummary summary_;
    std::vector<Scope> scopes_;
    mutable std::unordered_map<base::u32, bool> type_borrow_cache_;
};

[[nodiscard]] std::string_view borrow_summary_origin_kind_name(BorrowSummaryOriginKind kind) noexcept;
[[nodiscard]] std::string dump_function_borrow_summary(const FunctionBorrowSummary& summary);

} // namespace aurex::sema
