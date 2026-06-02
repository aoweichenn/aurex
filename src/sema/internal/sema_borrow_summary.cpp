#include <aurex/base/integer.hpp>
#include <aurex/query/stable_hash.hpp>

#include <algorithm>
#include <sstream>
#include <utility>

#include <sema/internal/sema_borrow_summary.hpp>

namespace aurex::sema {
namespace {

constexpr std::string_view SEMA_BORROW_SUMMARY_ID_CONTEXT = "sema borrow summary id";
constexpr base::usize SEMA_BORROW_SUMMARY_INITIAL_TASK_CAPACITY = 32;

[[nodiscard]] bool valid_stmt(const syntax::AstModule& module, const syntax::StmtId stmt) noexcept
{
    return syntax::is_valid(stmt) && stmt.value < module.stmts.size();
}

[[nodiscard]] bool valid_expr(const syntax::AstModule& module, const syntax::ExprId expr) noexcept
{
    return syntax::is_valid(expr) && expr.value < module.exprs.size();
}

[[nodiscard]] bool valid_pattern(const syntax::AstModule& module, const syntax::PatternId pattern) noexcept
{
    return syntax::is_valid(pattern) && pattern.value < module.patterns.size();
}

[[nodiscard]] base::SourceRange expr_range(const syntax::AstModule& module, const syntax::ExprId expr) noexcept
{
    return valid_expr(module, expr) ? module.exprs.range(expr.value) : base::SourceRange{};
}

[[nodiscard]] bool same_range(const base::SourceRange& lhs, const base::SourceRange& rhs) noexcept
{
    return lhs.source.value == rhs.source.value && lhs.begin == rhs.begin && lhs.end == rhs.end;
}

[[nodiscard]] bool same_origin(const BorrowSummaryOrigin& lhs, const BorrowSummaryOrigin& rhs) noexcept
{
    return lhs.kind == rhs.kind && lhs.param_index == rhs.param_index && lhs.name_id == rhs.name_id
        && lhs.expr.value == rhs.expr.value && same_range(lhs.range, rhs.range);
}

void append_optional_name_id(std::ostringstream& stream, const IdentId name)
{
    if (syntax::is_valid(name)) {
        stream << '#' << name.value;
        return;
    }
    stream << '-';
}

void append_optional_expr_id(std::ostringstream& stream, const syntax::ExprId expr)
{
    if (syntax::is_valid(expr)) {
        stream << 'e' << expr.value;
        return;
    }
    stream << '-';
}

void append_range(std::ostringstream& stream, const base::SourceRange& range)
{
    stream << range.source.value << ':' << range.begin << ".." << range.end;
}

} // namespace

SemanticAnalyzerCore::BorrowSummaryBuilder::BorrowSummaryBuilder(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::build(
    const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature)
{
    this->reset(function, key, signature);
    this->bind_parameters();
    std::vector<Task> tasks;
    tasks.reserve(SEMA_BORROW_SUMMARY_INITIAL_TASK_CAPACITY);
    this->push_scoped_block(tasks, function.body);
    this->run_tasks(tasks);
    this->finalize_summary();
    this->core_.state_.checked.borrow_summaries[key] = std::move(this->summary_);
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::reset(
    const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature)
{
    this->function_ = &function;
    this->signature_ = &signature;
    this->summary_ = FunctionBorrowSummary{};
    this->summary_.function = key;
    this->summary_.return_type = signature.return_type;
    this->summary_.return_type_can_contain_borrow = this->type_can_contain_borrow(signature.return_type);
    this->summary_.part_index = signature.part_index;
    this->scopes_.clear();
    this->type_borrow_cache_.clear();
    this->push_scope();
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::bind_parameters()
{
    if (this->function_ == nullptr || this->signature_ == nullptr) {
        return;
    }
    for (base::usize index = 0; index < this->function_->params.size(); ++index) {
        const syntax::ParamDecl& param = this->function_->params[index];
        const OriginSet origin = this->parameter_origin(index, param);
        this->bind_storage(param.name_id, origin);
        const TypeHandle param_type =
            index < this->signature_->param_types.size() ? this->signature_->param_types[index] : INVALID_TYPE_HANDLE;
        this->bind_borrowed_value(param.name_id, this->type_can_contain_borrow(param_type) ? origin : OriginSet{});
    }
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::push_scope()
{
    this->scopes_.emplace_back();
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::pop_scope()
{
    if (!this->scopes_.empty()) {
        this->scopes_.pop_back();
    }
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::bind_storage(const IdentId name, OriginSet origin)
{
    if (!syntax::is_valid(name) || this->scopes_.empty()) {
        return;
    }
    this->sort_unique(origin);
    this->scopes_.back().storage[name] = std::move(origin);
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::bind_borrowed_value(const IdentId name, OriginSet origin)
{
    if (!syntax::is_valid(name) || this->scopes_.empty()) {
        return;
    }
    this->sort_unique(origin);
    this->scopes_.back().borrowed_values[name] = std::move(origin);
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::assign_borrowed_value(const IdentId name, OriginSet origin)
{
    if (!syntax::is_valid(name)) {
        return;
    }
    this->sort_unique(origin);
    for (auto scope = this->scopes_.rbegin(); scope != this->scopes_.rend(); ++scope) {
        if (scope->storage.contains(name)) {
            scope->borrowed_values[name] = std::move(origin);
            return;
        }
    }
    this->bind_borrowed_value(name, std::move(origin));
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet SemanticAnalyzerCore::BorrowSummaryBuilder::lookup_storage(
    const IdentId name) const
{
    if (!syntax::is_valid(name)) {
        return {};
    }
    for (auto scope = this->scopes_.rbegin(); scope != this->scopes_.rend(); ++scope) {
        const auto found = scope->storage.find(name);
        if (found != scope->storage.end()) {
            return found->second;
        }
    }
    return {};
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet SemanticAnalyzerCore::BorrowSummaryBuilder::lookup_borrowed_value(
    const IdentId name) const
{
    if (!syntax::is_valid(name)) {
        return {};
    }
    for (auto scope = this->scopes_.rbegin(); scope != this->scopes_.rend(); ++scope) {
        const auto found = scope->borrowed_values.find(name);
        if (found != scope->borrowed_values.end()) {
            return found->second;
        }
    }
    return {};
}

base::u32 SemanticAnalyzerCore::BorrowSummaryBuilder::append_origin(BorrowSummaryOrigin origin)
{
    for (base::usize index = 0; index < this->summary_.origins.size(); ++index) {
        if (same_origin(this->summary_.origins[index], origin)) {
            return base::checked_u32(index, SEMA_BORROW_SUMMARY_ID_CONTEXT);
        }
    }
    const base::u32 index = base::checked_u32(this->summary_.origins.size(), SEMA_BORROW_SUMMARY_ID_CONTEXT);
    this->summary_.origins.push_back(std::move(origin));
    return index;
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet SemanticAnalyzerCore::BorrowSummaryBuilder::parameter_origin(
    const base::usize index, const syntax::ParamDecl& param)
{
    OriginSet origin;
    origin.origins.push_back(this->append_origin(BorrowSummaryOrigin{
        .kind = BorrowSummaryOriginKind::parameter,
        .param_index = base::checked_u32(index, SEMA_BORROW_SUMMARY_ID_CONTEXT),
        .name_id = param.name_id,
        .expr = syntax::INVALID_EXPR_ID,
        .range = param.range,
    }));
    return origin;
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet SemanticAnalyzerCore::BorrowSummaryBuilder::local_origin(
    const IdentId name, const base::SourceRange& range, const syntax::ExprId expr)
{
    OriginSet origin;
    origin.origins.push_back(this->append_origin(BorrowSummaryOrigin{
        .kind = BorrowSummaryOriginKind::local,
        .param_index = SEMA_BORROW_SUMMARY_INVALID_INDEX,
        .name_id = name,
        .expr = expr,
        .range = range,
    }));
    return origin;
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet SemanticAnalyzerCore::BorrowSummaryBuilder::temporary_origin(
    const syntax::ExprId expr)
{
    OriginSet origin;
    origin.origins.push_back(this->append_origin(BorrowSummaryOrigin{
        .kind = BorrowSummaryOriginKind::temporary,
        .param_index = SEMA_BORROW_SUMMARY_INVALID_INDEX,
        .name_id = INVALID_IDENT_ID,
        .expr = expr,
        .range = expr_range(this->core_.ctx_.module, expr),
    }));
    return origin;
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet SemanticAnalyzerCore::BorrowSummaryBuilder::unknown_origin() const
{
    OriginSet origin;
    origin.unknown = true;
    return origin;
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet SemanticAnalyzerCore::BorrowSummaryBuilder::merge(
    OriginSet lhs, const OriginSet& rhs) const
{
    lhs.unknown = lhs.unknown || rhs.unknown;
    lhs.origins.insert(lhs.origins.end(), rhs.origins.begin(), rhs.origins.end());
    this->sort_unique(lhs);
    return lhs;
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::sort_unique(OriginSet& origin) const
{
    std::ranges::sort(origin.origins);
    origin.origins.erase(std::ranges::unique(origin.origins).begin(), origin.origins.end());
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::push_scoped_block(
    std::vector<Task>& tasks, const syntax::StmtId block) const
{
    if (syntax::is_valid(block)) {
        tasks.push_back(Task{TaskKind::scoped_block, block});
    }
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::push_statement(
    std::vector<Task>& tasks, const syntax::StmtId stmt) const
{
    if (syntax::is_valid(stmt)) {
        tasks.push_back(Task{TaskKind::statement, stmt});
    }
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::run_tasks(std::vector<Task>& tasks)
{
    while (!tasks.empty()) {
        const Task task = tasks.back();
        tasks.pop_back();
        switch (task.kind) {
            case TaskKind::scoped_block:
                this->push_scope();
                tasks.push_back(Task{TaskKind::pop_scope, syntax::INVALID_STMT_ID});
                tasks.push_back(Task{TaskKind::block_statements, task.stmt});
                break;
            case TaskKind::block_statements:
                this->push_block_statements(tasks, task.stmt);
                break;
            case TaskKind::statement:
                this->analyze_statement(tasks, task.stmt);
                break;
            case TaskKind::pop_scope:
                this->pop_scope();
                break;
        }
    }
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::push_block_statements(
    std::vector<Task>& tasks, const syntax::StmtId block) const
{
    if (!valid_stmt(this->core_.ctx_.module, block)) {
        return;
    }
    const syntax::AstArenaVector<syntax::StmtId>* const statements =
        this->core_.ctx_.module.stmts.block_statements(block.value);
    if (statements == nullptr) {
        return;
    }
    for (base::usize index = statements->size(); index > 0; --index) {
        this->push_statement(tasks, (*statements)[index - 1]);
    }
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::analyze_statement(
    std::vector<Task>& tasks, const syntax::StmtId stmt_id)
{
    if (!valid_stmt(this->core_.ctx_.module, stmt_id)) {
        return;
    }
    const syntax::StmtNode stmt = this->core_.ctx_.module.stmts[stmt_id.value];
    switch (stmt.kind) {
        case syntax::StmtKind::let:
        case syntax::StmtKind::var:
            this->analyze_local_declaration(stmt_id, stmt);
            this->push_scoped_block(tasks, stmt.else_block);
            break;
        case syntax::StmtKind::assign:
            this->analyze_assignment(stmt);
            break;
        case syntax::StmtKind::if_:
            this->push_scoped_block(tasks, stmt.else_block);
            this->push_statement(tasks, stmt.else_if);
            this->push_scoped_block(tasks, stmt.then_block);
            break;
        case syntax::StmtKind::while_:
            this->push_scoped_block(tasks, stmt.body);
            break;
        case syntax::StmtKind::for_:
            this->push_scope();
            tasks.push_back(Task{TaskKind::pop_scope, syntax::INVALID_STMT_ID});
            this->push_statement(tasks, stmt.for_update);
            this->push_scoped_block(tasks, stmt.body);
            this->push_statement(tasks, stmt.for_init);
            break;
        case syntax::StmtKind::for_range:
            this->push_scope();
            tasks.push_back(Task{TaskKind::pop_scope, syntax::INVALID_STMT_ID});
            this->bind_storage(stmt.name_id, this->local_origin(stmt.name_id, stmt.range, syntax::INVALID_EXPR_ID));
            this->bind_borrowed_value(stmt.name_id, {});
            this->push_scoped_block(tasks, stmt.body);
            break;
        case syntax::StmtKind::return_:
            this->record_return_origin(stmt.return_value, stmt.range);
            break;
        case syntax::StmtKind::block:
            this->push_scoped_block(tasks, stmt_id);
            break;
        case syntax::StmtKind::break_:
        case syntax::StmtKind::continue_:
        case syntax::StmtKind::defer:
        case syntax::StmtKind::expr:
            break;
    }
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::analyze_local_declaration(
    const syntax::StmtId stmt_id, const syntax::StmtNode& stmt)
{
    const TypeHandle local_type = this->core_.cached_stmt_local_type(stmt_id);
    if (syntax::is_valid(stmt.pattern)) {
        this->bind_pattern_storage(stmt.pattern, local_type, stmt.init);
        return;
    }
    if (!syntax::is_valid(stmt.name_id)) {
        return;
    }
    const OriginSet origin = this->type_can_contain_borrow(local_type) ? this->borrow_origin(stmt.init) : OriginSet{};
    this->bind_storage(stmt.name_id, this->local_origin(stmt.name_id, stmt.range, syntax::INVALID_EXPR_ID));
    this->bind_borrowed_value(stmt.name_id, origin);
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::analyze_assignment(const syntax::StmtNode& stmt)
{
    const IdentId assigned_name = this->unqualified_name_id(stmt.lhs);
    if (!syntax::is_valid(assigned_name)) {
        return;
    }
    const TypeHandle lhs_type = this->core_.cached_expr_type(stmt.lhs);
    this->assign_borrowed_value(
        assigned_name, this->type_can_contain_borrow(lhs_type) ? this->borrow_origin(stmt.rhs) : OriginSet{});
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::bind_pattern_storage(
    const syntax::PatternId pattern, const TypeHandle type, const syntax::ExprId source)
{
    std::vector<PatternFrame> pending{{pattern, type, source}};
    while (!pending.empty()) {
        const PatternFrame frame = pending.back();
        pending.pop_back();
        if (!valid_pattern(this->core_.ctx_.module, frame.pattern)) {
            continue;
        }
        const syntax::PatternNode* const node = this->core_.ctx_.module.patterns.ptr(frame.pattern.value);
        if (node == nullptr) {
            continue;
        }
        switch (node->kind) {
            case syntax::PatternKind::binding:
                this->bind_pattern_binding(*node, frame.type, frame.source);
                break;
            case syntax::PatternKind::tuple:
                this->push_tuple_pattern_frames(pending, *node, frame.type, frame.source);
                break;
            case syntax::PatternKind::slice:
                this->push_slice_pattern_frames(pending, *node, frame.type, frame.source);
                break;
            case syntax::PatternKind::struct_:
                this->push_struct_pattern_frames(pending, *node, frame.type, frame.source);
                break;
            case syntax::PatternKind::enum_case:
                this->push_enum_pattern_frames(pending, *node, frame.type, frame.source);
                break;
            case syntax::PatternKind::or_pattern:
                for (const syntax::PatternId alternative : node->alternatives) {
                    pending.push_back(PatternFrame{alternative, frame.type, frame.source});
                }
                break;
            case syntax::PatternKind::wildcard:
            case syntax::PatternKind::literal:
            case syntax::PatternKind::const_:
                break;
        }
    }
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::bind_pattern_binding(
    const syntax::PatternNode& pattern, const TypeHandle type, const syntax::ExprId source)
{
    this->bind_storage(
        pattern.binding_name_id, this->local_origin(pattern.binding_name_id, pattern.range, syntax::INVALID_EXPR_ID));
    this->bind_borrowed_value(
        pattern.binding_name_id, this->type_can_contain_borrow(type) ? this->borrow_origin(source) : OriginSet{});
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::push_tuple_pattern_frames(std::vector<PatternFrame>& pending,
    const syntax::PatternNode& pattern, const TypeHandle type, const syntax::ExprId source) const
{
    const syntax::AstArenaVector<syntax::ExprId>* const tuple_source = valid_expr(this->core_.ctx_.module, source)
        ? this->core_.ctx_.module.exprs.tuple_elements(source.value)
        : nullptr;
    const TypeInfo* const tuple_type =
        this->core_.state_.checked.types.is_tuple(type) ? &this->core_.state_.checked.types.get(type) : nullptr;
    for (base::usize index = pattern.elements.size(); index > 0; --index) {
        const base::usize element_index = index - 1;
        const TypeHandle element_type = tuple_type != nullptr && element_index < tuple_type->tuple_elements.size()
            ? tuple_type->tuple_elements[element_index]
            : INVALID_TYPE_HANDLE;
        const syntax::ExprId element_source =
            tuple_source != nullptr && element_index < tuple_source->size() ? (*tuple_source)[element_index] : source;
        pending.push_back(PatternFrame{pattern.elements[element_index], element_type, element_source});
    }
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::push_slice_pattern_frames(std::vector<PatternFrame>& pending,
    const syntax::PatternNode& pattern, const TypeHandle type, const syntax::ExprId source) const
{
    const syntax::ArrayExprPayload* const array_source = valid_expr(this->core_.ctx_.module, source)
        ? this->core_.ctx_.module.exprs.array_payload(source.value)
        : nullptr;
    TypeHandle element_type = INVALID_TYPE_HANDLE;
    if (is_valid(type) && type.value < this->core_.state_.checked.types.size()) {
        const TypeInfo& info = this->core_.state_.checked.types.get(type);
        if (info.kind == TypeKind::array) {
            element_type = info.array_element;
        } else if (info.kind == TypeKind::slice) {
            element_type = info.slice_element;
        }
    }
    for (base::usize index = pattern.elements.size(); index > 0; --index) {
        const base::usize element_index = index - 1;
        const syntax::ExprId element_source = array_source != nullptr && element_index < array_source->elements.size()
            ? array_source->elements[element_index]
            : source;
        pending.push_back(PatternFrame{pattern.elements[element_index], element_type, element_source});
    }
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::push_struct_pattern_frames(std::vector<PatternFrame>& pending,
    const syntax::PatternNode& pattern, const TypeHandle type, const syntax::ExprId source) const
{
    const StructInfo* const structure = this->core_.find_struct(type);
    const syntax::StructLiteralExprPayload* const struct_source = valid_expr(this->core_.ctx_.module, source)
        ? this->core_.ctx_.module.exprs.struct_literal_payload(source.value)
        : nullptr;
    for (const syntax::FieldPattern& field : pattern.field_patterns) {
        const StructFieldInfo* const field_info =
            structure == nullptr ? nullptr : this->pattern_struct_field(*structure, field.name_id);
        pending.push_back(PatternFrame{
            field.pattern,
            field_info == nullptr ? INVALID_TYPE_HANDLE : field_info->type,
            this->struct_field_source(struct_source, field.name_id, source),
        });
    }
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::push_enum_pattern_frames(std::vector<PatternFrame>& pending,
    const syntax::PatternNode& pattern, const TypeHandle type, const syntax::ExprId source) const
{
    const EnumCaseInfo* const enum_case =
        this->core_.find_enum_case_by_type_and_case(type, pattern.case_name_id, pattern.case_name);
    for (base::usize index = pattern.payload_patterns.size(); index > 0; --index) {
        const base::usize payload_index = index - 1;
        pending.push_back(PatternFrame{
            pattern.payload_patterns[payload_index],
            enum_case == nullptr || payload_index >= enum_case->payload_types.size()
                ? INVALID_TYPE_HANDLE
                : enum_case->payload_types[payload_index],
            source,
        });
    }
}

syntax::ExprId SemanticAnalyzerCore::BorrowSummaryBuilder::struct_field_source(
    const syntax::StructLiteralExprPayload* const source, const IdentId field_name,
    const syntax::ExprId fallback) const noexcept
{
    if (source == nullptr) {
        return fallback;
    }
    for (const syntax::FieldInit& init : source->field_inits) {
        if (init.name_id == field_name) {
            return init.value;
        }
    }
    return fallback;
}

const StructFieldInfo* SemanticAnalyzerCore::BorrowSummaryBuilder::pattern_struct_field(
    const StructInfo& structure, const IdentId field_name) const noexcept
{
    for (const StructFieldInfo& field : structure.fields) {
        if (field.name_id == field_name) {
            return &field;
        }
    }
    return nullptr;
}

IdentId SemanticAnalyzerCore::BorrowSummaryBuilder::unqualified_name_id(const syntax::ExprId expr) const noexcept
{
    if (!valid_expr(this->core_.ctx_.module, expr)) {
        return INVALID_IDENT_ID;
    }
    const syntax::NameExprPayload* const name = this->core_.ctx_.module.exprs.name_payload(expr.value);
    return name != nullptr && name->scope_name.empty() ? name->text_id : INVALID_IDENT_ID;
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet SemanticAnalyzerCore::BorrowSummaryBuilder::borrowed_name_origin(
    const syntax::ExprId expr) const
{
    return this->lookup_borrowed_value(this->unqualified_name_id(expr));
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet SemanticAnalyzerCore::BorrowSummaryBuilder::storage_name_origin(
    const syntax::ExprId expr) const
{
    return this->lookup_storage(this->unqualified_name_id(expr));
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet SemanticAnalyzerCore::BorrowSummaryBuilder::place_storage_origin(
    const syntax::ExprId expr)
{
    syntax::ExprId current = expr;
    bool traversed_projection = false;
    while (valid_expr(this->core_.ctx_.module, current)) {
        const syntax::ExprKind kind = this->core_.ctx_.module.exprs.kind(current.value);
        if (kind == syntax::ExprKind::name) {
            if (const OriginSet alias = this->borrowed_name_origin(current); alias.unknown || !alias.origins.empty()) {
                return alias;
            }
            if (traversed_projection
                && this->core_.state_.checked.types.is_reference(this->core_.cached_expr_type(current))) {
                return this->unknown_origin();
            }
            return this->storage_name_origin(current);
        }
        if (const syntax::FieldExprPayload* const field = this->core_.ctx_.module.exprs.field_payload(current.value);
            kind == syntax::ExprKind::field && field != nullptr) {
            traversed_projection = true;
            current = field->object;
            continue;
        }
        if (const syntax::IndexExprPayload* const index = this->core_.ctx_.module.exprs.index_payload(current.value);
            kind == syntax::ExprKind::index && index != nullptr) {
            traversed_projection = true;
            current = index->object;
            continue;
        }
        if (const syntax::SliceExprPayload* const slice = this->core_.ctx_.module.exprs.slice_payload(current.value);
            kind == syntax::ExprKind::slice && slice != nullptr) {
            traversed_projection = true;
            current = slice->object;
            continue;
        }
        if (const syntax::UnaryExprPayload* const unary = this->core_.ctx_.module.exprs.unary_payload(current.value);
            kind == syntax::ExprKind::unary && unary != nullptr && unary->op == syntax::UnaryOp::dereference) {
            if (const OriginSet alias = this->borrowed_name_origin(unary->operand);
                alias.unknown || !alias.origins.empty()) {
                return alias;
            }
            return this->unknown_origin();
        }
        break;
    }
    return valid_expr(this->core_.ctx_.module, expr) ? this->temporary_origin(expr) : OriginSet{};
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet SemanticAnalyzerCore::BorrowSummaryBuilder::slice_source_origin(
    const syntax::ExprId expr)
{
    if (const OriginSet alias = this->borrowed_name_origin(expr); alias.unknown || !alias.origins.empty()) {
        return alias;
    }
    const TypeHandle type = this->core_.cached_expr_type(expr);
    if (this->core_.state_.checked.types.is_array(type)) {
        return this->place_storage_origin(expr);
    }
    return {};
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet SemanticAnalyzerCore::BorrowSummaryBuilder::call_return_origin(
    const syntax::ExprId expr, const syntax::CallExprPayload& call)
{
    if (this->core_.ctx_.module.exprs.kind(expr.value) == syntax::ExprKind::str_from_bytes_unchecked) {
        return this->unknown_origin();
    }
    if (const FunctionCallBinding* const binding = this->direct_call_binding(expr); binding != nullptr) {
        const auto summary = this->core_.state_.checked.borrow_summaries.find(binding->function_key);
        if (summary != this->core_.state_.checked.borrow_summaries.end()) {
            return this->map_callee_summary_origins(
                summary->second, call, binding->callee_expr, binding->receiver_arg_count);
        }
        return this->type_can_contain_borrow(binding->return_type) ? this->unknown_origin() : OriginSet{};
    }
    if (const TraitMethodCallBinding* const binding = this->trait_call_binding(expr); binding != nullptr) {
        const auto summary = this->core_.state_.checked.borrow_summaries.find(binding->function_key);
        const base::u32 receiver_arg_count = is_valid(binding->receiver_type) ? 1U : 0U;
        if (summary != this->core_.state_.checked.borrow_summaries.end()) {
            return this->map_callee_summary_origins(summary->second, call, binding->callee_expr, receiver_arg_count);
        }
        return this->type_can_contain_borrow(binding->return_type) ? this->unknown_origin() : OriginSet{};
    }
    return this->type_can_contain_borrow(this->core_.cached_expr_type(expr)) ? this->unknown_origin() : OriginSet{};
}

const FunctionCallBinding* SemanticAnalyzerCore::BorrowSummaryBuilder::direct_call_binding(
    const syntax::ExprId call_expr) const noexcept
{
    for (const FunctionCallBinding& binding : this->core_.state_.checked.function_calls) {
        if (binding.call_expr.value == call_expr.value) {
            return &binding;
        }
    }
    return nullptr;
}

const TraitMethodCallBinding* SemanticAnalyzerCore::BorrowSummaryBuilder::trait_call_binding(
    const syntax::ExprId call_expr) const noexcept
{
    for (const TraitMethodCallBinding& binding : this->core_.state_.checked.trait_method_calls) {
        if (binding.call_expr.value == call_expr.value) {
            return &binding;
        }
    }
    return nullptr;
}

syntax::ExprId SemanticAnalyzerCore::BorrowSummaryBuilder::call_argument_for_param(const syntax::CallExprPayload& call,
    const syntax::ExprId callee, const base::u32 receiver_arg_count, const base::u32 param_index) const
{
    if (receiver_arg_count != 0 && param_index < receiver_arg_count) {
        syntax::ExprId current = callee;
        std::unordered_set<base::u32> visited;
        while (valid_expr(this->core_.ctx_.module, current) && visited.insert(current.value).second) {
            if (const syntax::GenericApplyExprPayload* const generic =
                    this->core_.ctx_.module.exprs.generic_apply_payload(current.value);
                this->core_.ctx_.module.exprs.kind(current.value) == syntax::ExprKind::generic_apply
                && generic != nullptr) {
                current = generic->callee;
                continue;
            }
            if (const syntax::FieldExprPayload* const field =
                    this->core_.ctx_.module.exprs.field_payload(current.value);
                this->core_.ctx_.module.exprs.kind(current.value) == syntax::ExprKind::field && field != nullptr) {
                return field->object;
            }
            break;
        }
        return syntax::INVALID_EXPR_ID;
    }
    const base::u32 arg_index = param_index - receiver_arg_count;
    return arg_index < call.args.size() ? call.args[arg_index] : syntax::INVALID_EXPR_ID;
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet
SemanticAnalyzerCore::BorrowSummaryBuilder::map_callee_summary_origins(const FunctionBorrowSummary& callee,
    const syntax::CallExprPayload& call, const syntax::ExprId callee_expr, const base::u32 receiver_arg_count)
{
    OriginSet result;
    result.unknown = callee.has_unknown_return_origin;
    for (const FunctionBorrowReturnOrigin& dependency : callee.return_origins) {
        if (dependency.origin_index >= callee.origins.size()) {
            result.unknown = true;
            continue;
        }
        const BorrowSummaryOrigin& origin = callee.origins[dependency.origin_index];
        if (origin.kind != BorrowSummaryOriginKind::parameter) {
            result.unknown = true;
            continue;
        }
        const syntax::ExprId arg =
            this->call_argument_for_param(call, callee_expr, receiver_arg_count, origin.param_index);
        if (!valid_expr(this->core_.ctx_.module, arg)) {
            result.unknown = true;
            continue;
        }
        result = this->merge(result, this->borrow_origin(arg));
    }
    return result;
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet SemanticAnalyzerCore::BorrowSummaryBuilder::borrow_origin(
    const syntax::ExprId expr)
{
    OriginSet result;
    std::vector<syntax::ExprId> pending{expr};
    std::unordered_set<base::u32> visited;
    while (!pending.empty()) {
        const syntax::ExprId current = pending.back();
        pending.pop_back();
        if (!valid_expr(this->core_.ctx_.module, current) || !visited.insert(current.value).second) {
            continue;
        }
        const syntax::ExprKind kind = this->core_.ctx_.module.exprs.kind(current.value);
        if (kind != syntax::ExprKind::str_from_bytes_unchecked
            && !this->type_can_contain_borrow(this->core_.cached_expr_type(current))) {
            continue;
        }
        if (kind == syntax::ExprKind::name) {
            result = this->merge(result, this->borrowed_name_origin(current));
            continue;
        }
        if (const syntax::CallExprPayload* const call = this->core_.ctx_.module.exprs.call_payload(current.value);
            call != nullptr && (kind == syntax::ExprKind::call || kind == syntax::ExprKind::str_from_bytes_unchecked)) {
            result = this->merge(result, this->call_return_origin(current, *call));
            continue;
        }
        if (const syntax::UnaryExprPayload* const unary = this->core_.ctx_.module.exprs.unary_payload(current.value);
            kind == syntax::ExprKind::unary && unary != nullptr
            && (unary->op == syntax::UnaryOp::address_of || unary->op == syntax::UnaryOp::address_of_mut)) {
            result = this->merge(result, this->place_storage_origin(unary->operand));
            continue;
        }
        if (const syntax::SliceExprPayload* const slice = this->core_.ctx_.module.exprs.slice_payload(current.value);
            kind == syntax::ExprKind::slice && slice != nullptr) {
            result = this->merge(result, this->slice_source_origin(slice->object));
            continue;
        }
        if (const syntax::BlockExprPayload* const block = this->core_.ctx_.module.exprs.block_payload(current.value);
            block != nullptr && (kind == syntax::ExprKind::block_expr || kind == syntax::ExprKind::unsafe_block)) {
            result = this->merge(result, this->block_result_borrow_origin(*block));
            continue;
        }
        if (this->push_expression_children_for_origin(pending, current, kind)) {
            continue;
        }
    }
    return result;
}

bool SemanticAnalyzerCore::BorrowSummaryBuilder::push_expression_children_for_origin(
    std::vector<syntax::ExprId>& pending, const syntax::ExprId expr, const syntax::ExprKind kind)
{
    if (const syntax::CastExprPayload* const cast = this->core_.ctx_.module.exprs.cast_payload(expr.value);
        cast != nullptr && kind == syntax::ExprKind::str_from_utf8_checked) {
        pending.push_back(cast->expr);
        return true;
    }
    if (const syntax::IfExprPayload* const if_expr = this->core_.ctx_.module.exprs.if_payload(expr.value);
        kind == syntax::ExprKind::if_expr && if_expr != nullptr) {
        pending.push_back(if_expr->else_expr);
        pending.push_back(if_expr->then_expr);
        return true;
    }
    if (const syntax::MatchExprPayload* const match = this->core_.ctx_.module.exprs.match_payload(expr.value);
        kind == syntax::ExprKind::match_expr && match != nullptr) {
        for (const syntax::MatchArm& arm : match->arms) {
            pending.push_back(arm.value);
        }
        return true;
    }
    if (const syntax::ArrayExprPayload* const array = this->core_.ctx_.module.exprs.array_payload(expr.value);
        kind == syntax::ExprKind::array_literal && array != nullptr) {
        pending.push_back(array->repeat_value);
        for (const syntax::ExprId element : array->elements) {
            pending.push_back(element);
        }
        return true;
    }
    if (const syntax::AstArenaVector<syntax::ExprId>* const tuple =
            this->core_.ctx_.module.exprs.tuple_elements(expr.value);
        kind == syntax::ExprKind::tuple_literal && tuple != nullptr) {
        for (const syntax::ExprId element : *tuple) {
            pending.push_back(element);
        }
        return true;
    }
    if (const syntax::StructLiteralExprPayload* const structure =
            this->core_.ctx_.module.exprs.struct_literal_payload(expr.value);
        kind == syntax::ExprKind::struct_literal && structure != nullptr) {
        for (const syntax::FieldInit& field : structure->field_inits) {
            pending.push_back(field.value);
        }
        return true;
    }
    if (const syntax::FieldExprPayload* const field = this->core_.ctx_.module.exprs.field_payload(expr.value);
        kind == syntax::ExprKind::field && field != nullptr) {
        pending.push_back(field->object);
        return true;
    }
    if (const syntax::IndexExprPayload* const index = this->core_.ctx_.module.exprs.index_payload(expr.value);
        kind == syntax::ExprKind::index && index != nullptr) {
        pending.push_back(index->object);
        return true;
    }
    return false;
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet
SemanticAnalyzerCore::BorrowSummaryBuilder::block_result_borrow_origin(const syntax::BlockExprPayload& block)
{
    this->push_scope();
    std::vector<Task> tasks;
    tasks.reserve(SEMA_BORROW_SUMMARY_INITIAL_TASK_CAPACITY);
    this->push_block_statements(tasks, block.block);
    this->run_tasks(tasks);
    const OriginSet result = this->borrow_origin(block.result);
    this->pop_scope();
    return result;
}

bool SemanticAnalyzerCore::BorrowSummaryBuilder::type_can_contain_borrow(const TypeHandle type) const
{
    if (!is_valid(type)) {
        return false;
    }
    const auto cached = this->type_borrow_cache_.find(type.value);
    if (cached != this->type_borrow_cache_.end()) {
        return cached->second;
    }

    bool result = false;
    std::vector<TypeHandle> pending{type};
    std::unordered_set<base::u32> visited;
    while (!pending.empty() && !result) {
        const TypeHandle current = pending.back();
        pending.pop_back();
        if (!is_valid(current) || current.value >= this->core_.state_.checked.types.size()
            || !visited.insert(current.value).second) {
            continue;
        }
        const TypeInfo& info = this->core_.state_.checked.types.get(current);
        switch (info.kind) {
            case TypeKind::builtin:
                result = info.builtin == BuiltinType::str;
                break;
            case TypeKind::reference:
            case TypeKind::slice:
                result = true;
                break;
            case TypeKind::generic_param:
            case TypeKind::associated_projection:
                result = true;
                break;
            case TypeKind::array:
                pending.push_back(info.array_element);
                break;
            case TypeKind::tuple:
                pending.insert(pending.end(), info.tuple_elements.begin(), info.tuple_elements.end());
                break;
            case TypeKind::struct_: {
                const StructInfo* const structure = this->core_.find_struct(current);
                if (structure != nullptr) {
                    for (const StructFieldInfo& field : structure->fields) {
                        pending.push_back(field.type);
                    }
                }
                break;
            }
            case TypeKind::enum_: {
                if (const EnumCaseList* const cases = this->core_.find_enum_cases_by_type(current); cases != nullptr) {
                    for (const EnumCaseInfo* const enum_case : *cases) {
                        if (enum_case != nullptr) {
                            pending.insert(
                                pending.end(), enum_case->payload_types.begin(), enum_case->payload_types.end());
                        }
                    }
                }
                break;
            }
            case TypeKind::pointer:
            case TypeKind::function:
            case TypeKind::opaque_struct:
                break;
        }
    }
    this->type_borrow_cache_[type.value] = result;
    return result;
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::record_return_origin(
    const syntax::ExprId expr, const base::SourceRange& range)
{
    if (!this->summary_.return_type_can_contain_borrow
        && !this->type_can_contain_borrow(this->core_.cached_expr_type(expr))) {
        return;
    }
    OriginSet origin = this->borrow_origin(expr);
    this->sort_unique(origin);
    this->summary_.has_unknown_return_origin = this->summary_.has_unknown_return_origin || origin.unknown;
    for (const base::u32 origin_index : origin.origins) {
        if (origin_index >= this->summary_.origins.size()) {
            this->summary_.has_unknown_return_origin = true;
            continue;
        }
        const BorrowSummaryOrigin& summary_origin = this->summary_.origins[origin_index];
        this->summary_.has_local_return_escape = this->summary_.has_local_return_escape
            || summary_origin.kind == BorrowSummaryOriginKind::local
            || summary_origin.kind == BorrowSummaryOriginKind::temporary;
        this->summary_.return_origins.push_back(FunctionBorrowReturnOrigin{
            .origin_index = origin_index,
            .return_expr = expr,
            .range = valid_expr(this->core_.ctx_.module, expr) ? expr_range(this->core_.ctx_.module, expr) : range,
        });
    }
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::finalize_summary()
{
    std::ranges::sort(this->summary_.return_origins,
        [](const FunctionBorrowReturnOrigin& lhs, const FunctionBorrowReturnOrigin& rhs) {
            if (lhs.origin_index != rhs.origin_index) {
                return lhs.origin_index < rhs.origin_index;
            }
            return lhs.return_expr.value < rhs.return_expr.value;
        });
    this->summary_.return_origins.erase(
        std::ranges::unique(this->summary_.return_origins,
            [](const FunctionBorrowReturnOrigin& lhs, const FunctionBorrowReturnOrigin& rhs) {
                return lhs.origin_index == rhs.origin_index && lhs.return_expr.value == rhs.return_expr.value
                    && same_range(lhs.range, rhs.range);
            })
            .begin(),
        this->summary_.return_origins.end());
    this->summary_.fingerprint = this->fingerprint_summary();
}

query::StableFingerprint128 SemanticAnalyzerCore::BorrowSummaryBuilder::fingerprint_summary() const noexcept
{
    query::StableHashBuilder builder;
    builder.mix_u32(this->summary_.function.module);
    builder.mix_u32(this->summary_.function.owner_type);
    builder.mix_u32(this->summary_.function.name.value);
    builder.mix_u32(this->summary_.return_type.value);
    builder.mix_bool(this->summary_.return_type_can_contain_borrow);
    builder.mix_bool(this->summary_.has_unknown_return_origin);
    builder.mix_bool(this->summary_.has_local_return_escape);
    builder.mix_u64(static_cast<base::u64>(this->summary_.origins.size()));
    for (const BorrowSummaryOrigin& origin : this->summary_.origins) {
        builder.mix_u8(static_cast<base::u8>(origin.kind));
        builder.mix_u32(origin.param_index);
        builder.mix_u32(origin.name_id.value);
        builder.mix_u32(origin.expr.value);
        builder.mix_u32(origin.range.source.value);
        builder.mix_u64(static_cast<base::u64>(origin.range.begin));
        builder.mix_u64(static_cast<base::u64>(origin.range.end));
    }
    builder.mix_u64(static_cast<base::u64>(this->summary_.return_origins.size()));
    for (const FunctionBorrowReturnOrigin& dependency : this->summary_.return_origins) {
        builder.mix_u32(dependency.origin_index);
        builder.mix_u32(dependency.return_expr.value);
        builder.mix_u32(dependency.range.source.value);
        builder.mix_u64(static_cast<base::u64>(dependency.range.begin));
        builder.mix_u64(static_cast<base::u64>(dependency.range.end));
    }
    return builder.finish();
}

std::string_view borrow_summary_origin_kind_name(const BorrowSummaryOriginKind kind) noexcept
{
    switch (kind) {
        case BorrowSummaryOriginKind::none:
            return "none";
        case BorrowSummaryOriginKind::parameter:
            return "parameter";
        case BorrowSummaryOriginKind::local:
            return "local";
        case BorrowSummaryOriginKind::temporary:
            return "temporary";
        case BorrowSummaryOriginKind::unknown:
            return "unknown";
    }
    return "<invalid>";
}

std::string dump_function_borrow_summary(const FunctionBorrowSummary& summary)
{
    std::ostringstream stream;
    stream << "borrow_summary function=" << summary.function.module << ':' << summary.function.owner_type << ':';
    append_optional_name_id(stream, summary.function.name);
    stream << " return_type=" << summary.return_type.value
           << " can_borrow=" << (summary.return_type_can_contain_borrow ? "true" : "false")
           << " unknown=" << (summary.has_unknown_return_origin ? "true" : "false")
           << " local_escape=" << (summary.has_local_return_escape ? "true" : "false")
           << " fingerprint=" << query::debug_string(summary.fingerprint) << '\n';

    stream << "origins:\n";
    for (base::usize index = 0; index < summary.origins.size(); ++index) {
        const BorrowSummaryOrigin& origin = summary.origins[index];
        stream << "  o" << index << ' ' << borrow_summary_origin_kind_name(origin.kind)
               << " param=" << origin.param_index << " name=";
        append_optional_name_id(stream, origin.name_id);
        stream << " expr=";
        append_optional_expr_id(stream, origin.expr);
        stream << " range=";
        append_range(stream, origin.range);
        stream << '\n';
    }

    stream << "return_origins:\n";
    for (base::usize index = 0; index < summary.return_origins.size(); ++index) {
        const FunctionBorrowReturnOrigin& dependency = summary.return_origins[index];
        stream << "  r" << index << " origin=o" << dependency.origin_index << " expr=";
        append_optional_expr_id(stream, dependency.return_expr);
        stream << " range=";
        append_range(stream, dependency.range);
        stream << '\n';
    }
    return stream.str();
}

void SemanticAnalyzerCore::build_borrow_summary(
    const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature)
{
    BorrowSummaryBuilder(*this).build(function, key, signature);
}

} // namespace aurex::sema
