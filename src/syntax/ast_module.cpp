#include <aurex/syntax/ast/module.hpp>

#include <utility>

namespace aurex::syntax {

AstModule::AstModule()
{
    this->types.reserve_headers(base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
    this->exprs.reserve_headers(base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
    this->patterns.reserve_headers(base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
    this->stmts.reserve_headers(base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
    this->items.reserve_headers(base::config::AUREX_INITIAL_AST_NODE_CAPACITY);
}

AstModule::AstModule(const AstModule& other)
    : module_path(other.module_path), file_kind(other.file_kind), part_header(other.part_header),
      part_declarations(other.part_declarations), imports(other.imports), reexports(other.reexports),
      modules(other.modules), types(other.types), exprs(other.exprs), patterns(other.patterns), stmts(other.stmts),
      items(other.items), item_modules(other.item_modules), item_part_indices(other.item_part_indices),
      item_import_scopes(other.item_import_scopes), identifiers(other.identifiers), identifiers_ready_(false)
{
    this->intern_identifiers();
}

AstModule& AstModule::operator=(const AstModule& other)
{
    if (this == &other) {
        return *this;
    }
    this->module_path = other.module_path;
    this->file_kind = other.file_kind;
    this->part_header = other.part_header;
    this->part_declarations = other.part_declarations;
    this->imports = other.imports;
    this->reexports = other.reexports;
    this->modules = other.modules;
    this->types = other.types;
    this->exprs = other.exprs;
    this->patterns = other.patterns;
    this->stmts = other.stmts;
    this->items = other.items;
    this->item_modules = other.item_modules;
    this->item_part_indices = other.item_part_indices;
    this->item_import_scopes = other.item_import_scopes;
    this->identifiers = other.identifiers;
    this->identifiers_ready_ = false;
    this->intern_identifiers();
    return *this;
}

TypeId AstModule::push_type(TypeNode node)
{
    this->intern_type_node(node);
    return this->types.append(node);
}

ExprId AstModule::push_invalid_expr(const base::SourceRange& range)
{
    return this->exprs.append_invalid(range);
}

ExprId AstModule::push_literal_expr(const ExprKind kind, const base::SourceRange& range, const std::string_view text)
{
    return this->exprs.append_literal(kind, range, text);
}

ExprId AstModule::push_name_expr(const base::SourceRange& range, NameExprPayload payload)
{
    return this->push_name_expr(range, payload.scope_name, payload.scope_range, payload.text,
        std::move(payload.type_args), payload.scope_name_id, payload.text_id);
}

ExprId AstModule::push_generic_apply_expr(const base::SourceRange& range, GenericApplyExprPayload payload)
{
    return this->push_generic_apply_expr(range, payload.callee, std::move(payload.type_args));
}

ExprId AstModule::push_unary_expr(const ExprKind kind, const base::SourceRange& range, const UnaryExprPayload payload)
{
    return this->push_unary_expr(kind, range, payload.op, payload.operand);
}

ExprId AstModule::push_unary_expr(
    const ExprKind kind, const base::SourceRange& range, const UnaryOp op, const ExprId operand)
{
    return this->exprs.append_unary(kind, range, op, operand);
}

ExprId AstModule::push_try_expr(const base::SourceRange& range, const TryExprPayload payload)
{
    return this->push_try_expr(range, payload.operand);
}

ExprId AstModule::push_try_expr(const base::SourceRange& range, const ExprId operand)
{
    return this->exprs.append_try(range, operand);
}

ExprId AstModule::push_binary_expr(const base::SourceRange& range, const BinaryExprPayload payload)
{
    return this->push_binary_expr(range, payload.op, payload.lhs, payload.rhs);
}

ExprId AstModule::push_binary_expr(
    const base::SourceRange& range, const BinaryOp op, const ExprId lhs, const ExprId rhs)
{
    return this->exprs.append_binary(range, op, lhs, rhs);
}

ExprId AstModule::push_call_expr(const ExprKind kind, const base::SourceRange& range, CallExprPayload payload)
{
    return this->push_call_expr(kind, range, payload.callee, std::move(payload.args));
}

ExprId AstModule::push_if_expr(const base::SourceRange& range, const IfExprPayload payload)
{
    return this->push_if_expr(
        range, payload.condition, payload.condition_pattern, payload.then_expr, payload.else_expr);
}

ExprId AstModule::push_if_expr(const base::SourceRange& range, const ExprId condition,
    const PatternId condition_pattern, const ExprId then_expr, const ExprId else_expr)
{
    return this->exprs.append_if(range, condition, condition_pattern, then_expr, else_expr);
}

ExprId AstModule::push_block_expr(const ExprKind kind, const base::SourceRange& range, const BlockExprPayload payload)
{
    return this->push_block_expr(kind, range, payload.block, payload.result);
}

ExprId AstModule::push_block_expr(
    const ExprKind kind, const base::SourceRange& range, const StmtId block, const ExprId result)
{
    return this->exprs.append_block(kind, range, block, result);
}

ExprId AstModule::push_match_expr(const base::SourceRange& range, MatchExprPayload payload)
{
    return this->push_match_expr(range, payload.value, std::move(payload.arms));
}

ExprId AstModule::push_array_expr(const base::SourceRange& range, ArrayExprPayload payload)
{
    return this->push_array_expr(range, std::move(payload.elements), payload.repeat_value, payload.repeat_count);
}

ExprId AstModule::push_field_expr(const base::SourceRange& range, const FieldExprPayload& payload)
{
    return this->push_field_expr(range, payload.object, payload.field_name, payload.field_name_id);
}

ExprId AstModule::push_field_expr(
    const base::SourceRange& range, const ExprId object, std::string_view field_name, IdentId field_name_id)
{
    this->intern_identifier_text(field_name, field_name_id);
    return this->exprs.append_field(range, object, field_name, field_name_id);
}

ExprId AstModule::push_index_expr(const base::SourceRange& range, const IndexExprPayload payload)
{
    return this->push_index_expr(range, payload.object, payload.index);
}

ExprId AstModule::push_index_expr(const base::SourceRange& range, const ExprId object, const ExprId index)
{
    return this->exprs.append_index(range, object, index);
}

ExprId AstModule::push_slice_expr(const base::SourceRange& range, const SliceExprPayload payload)
{
    return this->push_slice_expr(range, payload.object, payload.start, payload.end);
}

ExprId AstModule::push_slice_expr(
    const base::SourceRange& range, const ExprId object, const ExprId start, const ExprId end)
{
    return this->exprs.append_slice(range, object, start, end);
}

ExprId AstModule::push_struct_literal_expr(const base::SourceRange& range, StructLiteralExprPayload payload)
{
    return this->push_struct_literal_expr(range, payload.object, payload.scope_name, payload.scope_range, payload.name,
        std::move(payload.type_args), std::move(payload.field_inits), payload.scope_name_id, payload.name_id);
}

ExprId AstModule::push_cast_like_expr(
    const ExprKind kind, const base::SourceRange& range, const CastExprPayload payload)
{
    return this->push_cast_like_expr(kind, range, payload.type, payload.expr);
}

ExprId AstModule::push_cast_like_expr(
    const ExprKind kind, const base::SourceRange& range, const TypeId type, const ExprId expr)
{
    return this->exprs.append_cast_like(kind, range, type, expr);
}

PatternId AstModule::push_pattern(PatternNode node)
{
    this->intern_pattern_node(node);
    return this->patterns.append(node);
}

StmtId AstModule::push_stmt(StmtNode node)
{
    this->intern_stmt_node(node);
    return this->stmts.append(std::move(node));
}

ItemId AstModule::push_item(ItemNode node)
{
    return this->push_item_for_module(std::move(node), INVALID_MODULE_ID);
}

ItemId AstModule::push_item_for_module(ItemNode node, const ModuleId module, const base::u32 part_index)
{
    this->intern_item_node(node);
    const ItemId id = this->items.append(std::move(node));
    this->item_modules.push_back(module);
    this->item_part_indices.push_back(part_index);
    return id;
}

void AstModule::set_invalid_expr(const base::usize index, const base::SourceRange& range)
{
    this->exprs.set_invalid(index, range);
}

void AstModule::set_generic_apply_expr(
    const base::usize index, const base::SourceRange& range, GenericApplyExprPayload payload)
{
    this->exprs.set_generic_apply(index, range, std::move(payload));
}

void AstModule::set_unary_expr(
    const base::usize index, const ExprKind kind, const base::SourceRange& range, const UnaryExprPayload payload)
{
    this->exprs.set_unary(index, kind, range, payload);
}

void AstModule::set_unary_expr(const base::usize index, const ExprKind kind, const base::SourceRange& range,
    const UnaryOp op, const ExprId operand)
{
    this->exprs.set_unary(index, kind, range, op, operand);
}

void AstModule::set_try_expr(const base::usize index, const base::SourceRange& range, const TryExprPayload payload)
{
    this->exprs.set_try(index, range, payload);
}

void AstModule::set_try_expr(const base::usize index, const base::SourceRange& range, const ExprId operand)
{
    this->exprs.set_try(index, range, operand);
}

void AstModule::set_call_expr(
    const base::usize index, const ExprKind kind, const base::SourceRange& range, CallExprPayload payload)
{
    this->exprs.set_call(index, kind, range, std::move(payload));
}

void AstModule::set_field_expr(const base::usize index, const base::SourceRange& range, FieldExprPayload payload)
{
    this->intern_identifier_text(payload.field_name, payload.field_name_id);
    this->exprs.set_field(index, range, payload);
}

void AstModule::set_field_expr(const base::usize index, const base::SourceRange& range, const ExprId object,
    std::string_view field_name, IdentId field_name_id)
{
    this->intern_identifier_text(field_name, field_name_id);
    this->exprs.set_field(index, range, object, field_name, field_name_id);
}

void AstModule::set_index_expr(const base::usize index, const base::SourceRange& range, const IndexExprPayload payload)
{
    this->exprs.set_index(index, range, payload);
}

void AstModule::set_index_expr(
    const base::usize index, const base::SourceRange& range, const ExprId object, const ExprId index_expr)
{
    this->exprs.set_index(index, range, object, index_expr);
}

void AstModule::set_slice_expr(const base::usize index, const base::SourceRange& range, const SliceExprPayload payload)
{
    this->exprs.set_slice(index, range, payload);
}

void AstModule::set_slice_expr(
    const base::usize index, const base::SourceRange& range, const ExprId object, const ExprId start, const ExprId end)
{
    this->exprs.set_slice(index, range, object, start, end);
}

void AstModule::set_struct_literal_expr(
    const base::usize index, const base::SourceRange& range, StructLiteralExprPayload payload)
{
    this->intern_struct_literal_payload(payload);
    this->exprs.set_struct_literal(index, range, std::move(payload));
}

void AstModule::set_item(const base::usize index, ItemNode node)
{
    this->intern_item_node(node);
    this->items.set(index, std::move(node));
}

IdentId AstModule::intern_identifier(const std::string_view text)
{
    return this->identifiers.intern(text);
}

IdentId AstModule::find_identifier(const std::string_view text) const noexcept
{
    return this->identifiers.find(text);
}

std::string_view AstModule::identifier_text(const IdentId id) const noexcept
{
    return this->identifiers.text(id);
}

bool AstModule::identifiers_ready() const noexcept
{
    return this->identifiers_ready_;
}

void AstModule::reserve_for_tokens(const base::usize token_count)
{
    AstReserveEstimate estimate;
    estimate.tokens = token_count;
    this->reserve_for_estimate(estimate);
}

void AstModule::reserve_for_estimate(const AstReserveEstimate& estimate)
{
    constexpr base::usize INITIAL_CAPACITY = base::config::AUREX_INITIAL_AST_NODE_CAPACITY;
    const base::usize type_capacity = ast_reserve_larger(estimate.type_sites * SYNTAX_AST_RESERVE_TYPES_PER_TYPE_SITE,
        estimate.items * SYNTAX_AST_RESERVE_TYPES_PER_ITEM);
    const base::usize fallback_expr_capacity =
        ast_reserve_larger(estimate.statements * SYNTAX_AST_RESERVE_EXPRS_PER_STATEMENT,
            ast_reserve_fraction(estimate.tokens, SYNTAX_AST_RESERVE_EXPR_TOKEN_DIVISOR));
    AstReserveEstimate::Exprs expr_capacity = estimate.exprs;
    if (expr_capacity.headers == 0) {
        expr_capacity = ast_expr_reserve_for_node_capacity(fallback_expr_capacity);
    }
    expr_capacity.headers = ast_reserve_at_least(INITIAL_CAPACITY, expr_capacity.headers);
    const base::usize pattern_capacity = estimate.pattern_sites * SYNTAX_AST_RESERVE_PATTERNS_PER_PATTERN_SITE;
    this->types.reserve(ast_reserve_at_least(INITIAL_CAPACITY,
        ast_reserve_larger(
            type_capacity, ast_reserve_fraction(estimate.tokens, SYNTAX_AST_RESERVE_TYPE_TOKEN_DIVISOR))));
    this->exprs.reserve_touched(expr_capacity);
    this->patterns.reserve(ast_reserve_at_least(INITIAL_CAPACITY,
        ast_reserve_larger(
            pattern_capacity, ast_reserve_fraction(estimate.tokens, SYNTAX_AST_RESERVE_PATTERN_TOKEN_DIVISOR))));
    this->stmts.reserve(ast_reserve_at_least(INITIAL_CAPACITY,
        ast_reserve_larger(
            estimate.statements, ast_reserve_fraction(estimate.tokens, SYNTAX_AST_RESERVE_STMT_TOKEN_DIVISOR))));
    const base::usize item_capacity = ast_reserve_at_least(INITIAL_CAPACITY,
        ast_reserve_larger(
            estimate.items, ast_reserve_fraction(estimate.tokens, SYNTAX_AST_RESERVE_ITEM_TOKEN_DIVISOR)));
    this->items.reserve(item_capacity);
    this->item_modules.reserve(item_capacity);
    this->item_import_scopes.reserve(item_capacity);
    this->identifiers.reserve(ast_reserve_at_least(INITIAL_CAPACITY,
        ast_reserve_fraction(estimate.identifier_tokens, SYNTAX_AST_RESERVE_IDENTIFIER_TOKEN_DIVISOR)));
}

void AstModule::finalize_identifiers()
{
    if (this->identifiers_ready_) {
        return;
    }
    this->intern_module_metadata();
    this->identifiers_ready_ = true;
}

void AstModule::intern_identifiers()
{
    if (this->identifiers_ready_) {
        return;
    }
    this->intern_module_metadata();
    for (base::usize i = 0; i < this->types.size(); ++i) {
        TypeNode node = this->types.take(i);
        this->intern_type_node(node);
        this->types.set(i, node);
    }
    for (base::usize i = 0; i < this->exprs.size(); ++i) {
        this->intern_expr_payload(i);
    }
    for (base::usize i = 0; i < this->patterns.size(); ++i) {
        PatternNode node = this->patterns.take(i);
        this->intern_pattern_node(node);
        this->patterns.set(i, node);
    }
    for (base::usize i = 0; i < this->stmts.size(); ++i) {
        StmtNode node = this->stmts.take(i);
        this->intern_stmt_node(node);
        this->stmts.set(i, std::move(node));
    }
    for (base::usize i = 0; i < this->items.size(); ++i) {
        ItemNode node = this->items.take(i);
        this->intern_item_node(node);
        this->items.set(i, std::move(node));
    }
    this->identifiers_ready_ = true;
}

void AstModule::intern_module_path(ModulePath& path)
{
    this->intern_identifier_list(path.parts, path.part_ids);
}

void AstModule::intern_module_part_decl(ModulePartDecl& part)
{
    this->intern_identifier_text(part.name, part.name_id);
}

void AstModule::intern_module_part_header(ModulePartHeader& part)
{
    this->intern_identifier_text(part.name, part.name_id);
}

void AstModule::intern_import_decl(ImportDecl& import)
{
    this->intern_module_path(import.path);
    this->intern_identifier_text(import.alias, import.alias_id);
}

void AstModule::intern_resolved_import(ResolvedImport& import)
{
    this->intern_identifier_text(import.alias, import.alias_id);
}

void AstModule::intern_use_decl(UseDecl& use)
{
    this->intern_module_path(use.module_path);
    this->intern_identifier_text(use.target_name, use.target_name_id);
    this->intern_identifier_text(use.alias, use.alias_id);
}

void AstModule::intern_resolved_use(ResolvedUse& use)
{
    this->intern_identifier_text(use.target_name, use.target_name_id);
    this->intern_identifier_text(use.alias, use.alias_id);
}

void AstModule::intern_identifier_text(std::string_view& text, IdentId& id)
{
    id = this->identifiers.intern(text);
    text = this->identifiers.text(id);
}

void AstModule::intern_type_node(TypeNode& node)
{
    this->intern_identifier_text(node.scope_name, node.scope_name_id);
    this->intern_identifier_list(node.scope_parts, node.scope_part_ids);
    this->intern_identifier_text(node.name, node.name_id);
}

void AstModule::intern_name_expr_payload(NameExprPayload& payload)
{
    this->intern_identifier_text(payload.scope_name, payload.scope_name_id);
    this->intern_identifier_text(payload.text, payload.text_id);
}

void AstModule::intern_struct_literal_payload(StructLiteralExprPayload& payload)
{
    this->intern_identifier_text(payload.scope_name, payload.scope_name_id);
    this->intern_identifier_text(payload.name, payload.name_id);
    this->intern_field_inits(payload.field_inits);
}

void AstModule::intern_expr_payload(const base::usize index)
{
    switch (this->exprs.kind(index)) {
        case ExprKind::name:
            if (NameExprPayload* const payload = this->exprs.name_payload(index); payload != nullptr) {
                this->intern_name_expr_payload(*payload);
            }
            break;
        case ExprKind::field:
            if (FieldExprPayload* const payload = this->exprs.field_payload(index); payload != nullptr) {
                this->intern_identifier_text(payload->field_name, payload->field_name_id);
            }
            break;
        case ExprKind::struct_literal:
            if (StructLiteralExprPayload* const payload = this->exprs.struct_literal_payload(index);
                payload != nullptr) {
                this->intern_struct_literal_payload(*payload);
            }
            break;
        default:
            break;
    }
}

void AstModule::intern_pattern_node(PatternNode& node)
{
    this->intern_identifier_text(node.binding_name, node.binding_name_id);
    this->intern_identifier_text(node.enum_name, node.enum_name_id);
    if (node.kind == PatternKind::enum_case || node.kind == PatternKind::literal) {
        this->intern_identifier_text(node.case_name, node.case_name_id);
    }
    this->intern_identifier_text(node.struct_name, node.struct_name_id);
    this->intern_identifier_list(node.binding_names, node.binding_name_ids);
    this->intern_field_patterns(node.field_patterns);
}

void AstModule::intern_stmt_node(StmtNode& node)
{
    this->intern_identifier_text(node.name, node.name_id);
}

void AstModule::intern_item_node(ItemNode& node)
{
    this->intern_identifier_text(node.name, node.name_id);
    this->intern_generic_params(node.generic_params);
    this->intern_generic_constraints(node.where_constraints);
    this->intern_field_decls(node.fields);
    this->intern_enum_case_decls(node.enum_cases);
    this->intern_param_decls(node.params);
    for (BorrowContractSelectorDecl& selector : node.borrow_contract.return_selectors) {
        this->intern_identifier_text(selector.name, selector.name_id);
    }
}

void AstModule::intern_module_metadata()
{
    this->intern_module_path(this->module_path);
    this->intern_module_part_header(this->part_header);
    for (ModulePartDecl& part : this->part_declarations) {
        this->intern_module_part_decl(part);
    }
    for (ImportDecl& import : this->imports) {
        this->intern_import_decl(import);
    }
    for (UseDecl& use : this->reexports) {
        this->intern_use_decl(use);
    }
    for (ModuleInfo& module : this->modules) {
        this->intern_module_path(module.path);
        for (ResolvedImport& import : module.imports) {
            this->intern_resolved_import(import);
        }
        for (ResolvedUse& use : module.reexports) {
            this->intern_resolved_use(use);
        }
    }
    for (ItemImportScope& scope : this->item_import_scopes) {
        for (ResolvedImport& import : scope.imports) {
            this->intern_resolved_import(import);
        }
    }
}

} // namespace aurex::syntax
