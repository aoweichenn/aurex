#pragma once

#include <aurex/base/config.hpp>
#include <aurex/syntax/ast/expr_nodes.hpp>
#include <aurex/syntax/ast/item_nodes.hpp>
#include <aurex/syntax/ast/pattern_nodes.hpp>
#include <aurex/syntax/ast/stmt_nodes.hpp>
#include <aurex/syntax/ast/type_nodes.hpp>
#include <aurex/syntax/identifier.hpp>

#include <string_view>
#include <utility>
#include <vector>

namespace aurex::syntax {

struct ModulePath {
    std::vector<std::string_view> parts;
    base::SourceRange range {};
    std::vector<IdentId> part_ids;
};

struct ImportDecl {
    ModulePath path;
    std::string_view alias;
    base::SourceRange alias_range {};
    Visibility visibility = Visibility::private_;
    bool explicit_visibility = false;
    IdentId alias_id = INVALID_IDENT_ID;
};

struct ResolvedImport {
    ModuleId module = INVALID_MODULE_ID;
    std::string_view alias;
    base::SourceRange alias_range {};
    Visibility visibility = Visibility::private_;
    IdentId alias_id = INVALID_IDENT_ID;
};

struct ModuleInfo {
    ModulePath path;
    std::vector<ResolvedImport> imports;
};

struct AstModule {
    // The AST is intentionally stored as parallel vectors addressed by small
    // IDs. This keeps nodes compact, avoids virtual dispatch, and lets later
    // compiler stages attach side tables without changing syntax nodes.
    ModulePath module_path;
    std::vector<ImportDecl> imports;
    std::vector<ModuleInfo> modules;
    TypeNodeList types;
    ExprNodeList exprs;
    PatternNodeList patterns;
    StmtNodeList stmts;
    ItemNodeList items;
    std::vector<ModuleId> item_modules;
    IdentifierInterner identifiers;

    AstModule();
    AstModule(const AstModule& other);
    AstModule& operator=(const AstModule& other);

    AstModule(AstModule&&) noexcept = default;
    AstModule& operator=(AstModule&&) noexcept = default;

    [[nodiscard]] TypeId push_type(TypeNode node);
    [[nodiscard]] ExprId push_invalid_expr(const base::SourceRange& range);

    [[nodiscard]] ExprId push_literal_expr(
        ExprKind kind,
        const base::SourceRange& range,
        std::string_view text);

    [[nodiscard]] ExprId push_name_expr(const base::SourceRange& range, NameExprPayload payload);

    template <typename TypeArgAllocator = std::allocator<TypeId>>
    [[nodiscard]] ExprId push_name_expr(
        const base::SourceRange& range,
        std::string_view scope_name,
        const base::SourceRange& scope_range,
        std::string_view text,
        std::vector<TypeId, TypeArgAllocator> type_args = std::vector<TypeId, TypeArgAllocator> {},
        IdentId scope_name_id = INVALID_IDENT_ID,
        IdentId text_id = INVALID_IDENT_ID) {
        this->intern_identifier_text(scope_name, scope_name_id);
        this->intern_identifier_text(text, text_id);
        return this->exprs.append_name(
            range,
            scope_name,
            scope_range,
            text,
            scope_name_id,
            text_id,
            std::move(type_args));
    }

    template <typename TypeArgAllocator = std::allocator<TypeId>>
    [[nodiscard]] ExprId push_name_expr(
        const base::SourceRange& range,
        const std::string_view text,
        std::vector<TypeId, TypeArgAllocator> type_args = std::vector<TypeId, TypeArgAllocator> {}) {
        return this->push_name_expr(range, {}, {}, text, std::move(type_args));
    }

    [[nodiscard]] ExprId push_generic_apply_expr(const base::SourceRange& range, GenericApplyExprPayload payload);

    template <typename TypeArgAllocator>
    [[nodiscard]] ExprId push_generic_apply_expr(
        const base::SourceRange& range,
        const ExprId callee,
        std::vector<TypeId, TypeArgAllocator> type_args) {
        return this->exprs.append_generic_apply(range, callee, std::move(type_args));
    }

    [[nodiscard]] ExprId push_unary_expr(ExprKind kind, const base::SourceRange& range, UnaryExprPayload payload);
    [[nodiscard]] ExprId push_unary_expr(ExprKind kind, const base::SourceRange& range, UnaryOp op, ExprId operand);

    [[nodiscard]] ExprId push_try_expr(const base::SourceRange& range, TryExprPayload payload);
    [[nodiscard]] ExprId push_try_expr(const base::SourceRange& range, ExprId operand);

    [[nodiscard]] ExprId push_binary_expr(const base::SourceRange& range, BinaryExprPayload payload);
    [[nodiscard]] ExprId push_binary_expr(const base::SourceRange& range, BinaryOp op, ExprId lhs, ExprId rhs);

    [[nodiscard]] ExprId push_call_expr(
        ExprKind kind,
        const base::SourceRange& range,
        CallExprPayload payload);

    template <typename ArgAllocator>
    [[nodiscard]] ExprId push_call_expr(
        const ExprKind kind,
        const base::SourceRange& range,
        const ExprId callee,
        std::vector<ExprId, ArgAllocator> args) {
        return this->exprs.append_call(kind, range, callee, std::move(args));
    }

    [[nodiscard]] ExprId push_if_expr(const base::SourceRange& range, IfExprPayload payload);

    [[nodiscard]] ExprId push_if_expr(
        const base::SourceRange& range,
        ExprId condition,
        PatternId condition_pattern,
        ExprId then_expr,
        ExprId else_expr);

    [[nodiscard]] ExprId push_block_expr(ExprKind kind, const base::SourceRange& range, BlockExprPayload payload);
    [[nodiscard]] ExprId push_block_expr(ExprKind kind, const base::SourceRange& range, StmtId block, ExprId result);

    [[nodiscard]] ExprId push_match_expr(const base::SourceRange& range, MatchExprPayload payload);

    template <typename ArmAllocator>
    [[nodiscard]] ExprId push_match_expr(
        const base::SourceRange& range,
        const ExprId value,
        std::vector<MatchArm, ArmAllocator> arms) {
        return this->exprs.append_match(range, value, std::move(arms));
    }

    [[nodiscard]] ExprId push_array_expr(const base::SourceRange& range, ArrayExprPayload payload);

    template <typename ElementAllocator>
    [[nodiscard]] ExprId push_array_expr(
        const base::SourceRange& range,
        std::vector<ExprId, ElementAllocator> elements,
        const ExprId repeat_value = INVALID_EXPR_ID,
        const ExprId repeat_count = INVALID_EXPR_ID) {
        return this->exprs.append_array(range, std::move(elements), repeat_value, repeat_count);
    }

    template <typename ElementAllocator>
    [[nodiscard]] ExprId push_tuple_expr(const base::SourceRange& range, std::vector<ExprId, ElementAllocator> elements) {
        return this->exprs.append_tuple(range, std::move(elements));
    }

    [[nodiscard]] ExprId push_field_expr(const base::SourceRange& range, const FieldExprPayload& payload);

    [[nodiscard]] ExprId push_field_expr(
        const base::SourceRange& range,
        ExprId object,
        std::string_view field_name,
        IdentId field_name_id = INVALID_IDENT_ID);

    [[nodiscard]] ExprId push_index_expr(const base::SourceRange& range, IndexExprPayload payload);
    [[nodiscard]] ExprId push_index_expr(const base::SourceRange& range, ExprId object, ExprId index);

    [[nodiscard]] ExprId push_slice_expr(const base::SourceRange& range, SliceExprPayload payload);
    [[nodiscard]] ExprId push_slice_expr(const base::SourceRange& range, ExprId object, ExprId start, ExprId end);

    [[nodiscard]] ExprId push_struct_literal_expr(const base::SourceRange& range, StructLiteralExprPayload payload);

    template <typename TypeArgAllocator, typename FieldInitAllocator>
    [[nodiscard]] ExprId push_struct_literal_expr(
        const base::SourceRange& range,
        const ExprId object,
        std::string_view scope_name,
        const base::SourceRange& scope_range,
        std::string_view name,
        std::vector<TypeId, TypeArgAllocator> type_args,
        std::vector<FieldInit, FieldInitAllocator> field_inits,
        IdentId scope_name_id = INVALID_IDENT_ID,
        IdentId name_id = INVALID_IDENT_ID) {
        this->intern_identifier_text(scope_name, scope_name_id);
        this->intern_identifier_text(name, name_id);
        this->intern_field_inits(field_inits);
        return this->exprs.append_struct_literal(
            range,
            object,
            scope_name,
            scope_range,
            name,
            scope_name_id,
            name_id,
            std::move(type_args),
            std::move(field_inits));
    }

    [[nodiscard]] ExprId push_cast_like_expr(ExprKind kind, const base::SourceRange& range, CastExprPayload payload);
    [[nodiscard]] ExprId push_cast_like_expr(ExprKind kind, const base::SourceRange& range, TypeId type, ExprId expr);

    [[nodiscard]] PatternId push_pattern(PatternNode node);
    [[nodiscard]] StmtId push_stmt(StmtNode node);
    [[nodiscard]] ItemId push_item(ItemNode node);
    [[nodiscard]] ItemId push_item_for_module(ItemNode node, ModuleId module);

    void set_invalid_expr(base::usize index, const base::SourceRange& range);

    void set_generic_apply_expr(base::usize index, const base::SourceRange& range, GenericApplyExprPayload payload);

    template <typename TypeArgAllocator>
    void set_generic_apply_expr(
        const base::usize index,
        const base::SourceRange& range,
        const ExprId callee,
        std::vector<TypeId, TypeArgAllocator> type_args) {
        this->exprs.set_generic_apply(index, range, callee, std::move(type_args));
    }

    void set_unary_expr(base::usize index, ExprKind kind, const base::SourceRange& range, UnaryExprPayload payload);
    void set_unary_expr(base::usize index, ExprKind kind, const base::SourceRange& range, UnaryOp op, ExprId operand);

    void set_try_expr(base::usize index, const base::SourceRange& range, TryExprPayload payload);
    void set_try_expr(base::usize index, const base::SourceRange& range, ExprId operand);

    void set_call_expr(base::usize index, ExprKind kind, const base::SourceRange& range, CallExprPayload payload);

    template <typename ArgAllocator>
    void set_call_expr(
        const base::usize index,
        const ExprKind kind,
        const base::SourceRange& range,
        const ExprId callee,
        std::vector<ExprId, ArgAllocator> args) {
        this->exprs.set_call(index, kind, range, callee, std::move(args));
    }

    void set_field_expr(base::usize index, const base::SourceRange& range, FieldExprPayload payload);

    void set_field_expr(
        base::usize index,
        const base::SourceRange& range,
        ExprId object,
        std::string_view field_name,
        IdentId field_name_id = INVALID_IDENT_ID);

    void set_index_expr(base::usize index, const base::SourceRange& range, IndexExprPayload payload);
    void set_index_expr(base::usize index, const base::SourceRange& range, ExprId object, ExprId index_expr);

    void set_slice_expr(base::usize index, const base::SourceRange& range, SliceExprPayload payload);
    void set_slice_expr(base::usize index, const base::SourceRange& range, ExprId object, ExprId start, ExprId end);

    void set_struct_literal_expr(base::usize index, const base::SourceRange& range, StructLiteralExprPayload payload);

    template <typename TypeArgAllocator, typename FieldInitAllocator>
    void set_struct_literal_expr(
        const base::usize index,
        const base::SourceRange& range,
        const ExprId object,
        std::string_view scope_name,
        const base::SourceRange& scope_range,
        std::string_view name,
        std::vector<TypeId, TypeArgAllocator> type_args,
        std::vector<FieldInit, FieldInitAllocator> field_inits,
        IdentId scope_name_id = INVALID_IDENT_ID,
        IdentId name_id = INVALID_IDENT_ID) {
        this->intern_identifier_text(scope_name, scope_name_id);
        this->intern_identifier_text(name, name_id);
        this->intern_field_inits(field_inits);
        this->exprs.set_struct_literal(
            index,
            range,
            object,
            scope_name,
            scope_range,
            name,
            scope_name_id,
            name_id,
            std::move(type_args),
            std::move(field_inits));
    }

    void set_item(base::usize index, ItemNode node);

    [[nodiscard]] IdentId intern_identifier(std::string_view text);
    [[nodiscard]] IdentId find_identifier(std::string_view text) const noexcept;
    [[nodiscard]] std::string_view identifier_text(IdentId id) const noexcept;

    template <typename T>
    [[nodiscard]] AstArenaVector<T> make_expr_list() {
        return this->exprs.make_list<T>();
    }

    template <typename T>
    [[nodiscard]] AstArenaVector<T> make_item_list() {
        return this->items.make_list<T>();
    }

    [[nodiscard]] bool identifiers_ready() const noexcept;

    void reserve_for_tokens(base::usize token_count);
    void reserve_for_estimate(const AstReserveEstimate& estimate);
    void finalize_identifiers();
    void intern_identifiers();
    void intern_module_path(ModulePath& path);
    void intern_import_decl(ImportDecl& import);
    void intern_resolved_import(ResolvedImport& import);

private:
    void intern_identifier_text(std::string_view& text, IdentId& id);

    template <typename TextAllocator, typename IdAllocator>
    void intern_identifier_list(
        std::vector<std::string_view, TextAllocator>& texts,
        std::vector<IdentId, IdAllocator>& ids) {
        ids.resize(texts.size(), INVALID_IDENT_ID);
        for (base::usize i = 0; i < texts.size(); ++i) {
            this->intern_identifier_text(texts[i], ids[i]);
        }
    }

    template <typename Allocator>
    void intern_generic_params(std::vector<GenericParamDecl, Allocator>& params) {
        for (GenericParamDecl& param : params) {
            this->intern_identifier_text(param.name, param.name_id);
        }
    }

    template <typename Allocator>
    void intern_generic_constraints(std::vector<GenericConstraintDecl, Allocator>& constraints) {
        for (GenericConstraintDecl& constraint : constraints) {
            this->intern_identifier_text(constraint.param_name, constraint.param_name_id);
            this->intern_identifier_list(constraint.capability_names, constraint.capability_name_ids);
        }
    }

    void intern_type_node(TypeNode& node);

    template <typename Allocator>
    void intern_field_inits(std::vector<FieldInit, Allocator>& inits) {
        for (FieldInit& init : inits) {
            this->intern_identifier_text(init.name, init.name_id);
        }
    }

    void intern_name_expr_payload(NameExprPayload& payload);
    void intern_struct_literal_payload(StructLiteralExprPayload& payload);

    void intern_expr_payload(base::usize index);

    template <typename Allocator>
    void intern_field_patterns(std::vector<FieldPattern, Allocator>& fields) {
        for (FieldPattern& field : fields) {
            this->intern_identifier_text(field.name, field.name_id);
        }
    }

    void intern_pattern_node(PatternNode& node);
    void intern_stmt_node(StmtNode& node);

    template <typename Allocator>
    void intern_param_decls(std::vector<ParamDecl, Allocator>& params) {
        for (ParamDecl& param : params) {
            this->intern_identifier_text(param.name, param.name_id);
        }
    }

    template <typename Allocator>
    void intern_field_decls(std::vector<FieldDecl, Allocator>& fields) {
        for (FieldDecl& field : fields) {
            this->intern_identifier_text(field.name, field.name_id);
        }
    }

    template <typename Allocator>
    void intern_enum_case_decls(std::vector<EnumCaseDecl, Allocator>& cases) {
        for (EnumCaseDecl& enum_case : cases) {
            this->intern_identifier_text(enum_case.name, enum_case.name_id);
        }
    }

    void intern_item_node(ItemNode& node);
    void intern_module_metadata();

    bool identifiers_ready_ = false;
};

} // namespace aurex::syntax
