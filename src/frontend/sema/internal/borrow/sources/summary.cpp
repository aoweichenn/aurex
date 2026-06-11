#include <aurex/infrastructure/base/integer.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>

#include <algorithm>
#include <span>
#include <sstream>
#include <utility>

#include <aurex/frontend/sema/call_arguments.hpp>

#include <frontend/sema/internal/borrow/private/summary.hpp>
#include <frontend/sema/internal/core/private/sema_array_repeat_semantics.hpp>

namespace aurex::sema {
namespace {

constexpr std::string_view SEMA_BORROW_SUMMARY_ID_CONTEXT = "sema borrow summary id";
constexpr base::usize SEMA_BORROW_SUMMARY_INITIAL_TASK_CAPACITY = 32;
constexpr std::size_t SEMA_BORROW_SUMMARY_HASH_MIX = 0x9e3779b97f4a7c15ULL;
constexpr base::usize SEMA_BORROW_SUMMARY_HASH_LEFT_SHIFT = 6;
constexpr base::usize SEMA_BORROW_SUMMARY_HASH_RIGHT_SHIFT = 2;

[[nodiscard]] std::size_t mix_borrow_summary_hash(std::size_t hash, const base::u64 value) noexcept
{
    hash ^= static_cast<std::size_t>(value) + SEMA_BORROW_SUMMARY_HASH_MIX
        + (hash << SEMA_BORROW_SUMMARY_HASH_LEFT_SHIFT) + (hash >> SEMA_BORROW_SUMMARY_HASH_RIGHT_SHIFT);
    return hash;
}

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

[[nodiscard]] bool same_storage_escape(
    const FunctionBorrowStorageEscape& lhs, const FunctionBorrowStorageEscape& rhs) noexcept
{
    return lhs.origin_index == rhs.origin_index && lhs.stored_expr.value == rhs.stored_expr.value
        && same_range(lhs.range, rhs.range);
}

[[nodiscard]] bool summary_origin_is_local_escape_source(const BorrowSummaryOrigin& origin) noexcept
{
    return origin.kind == BorrowSummaryOriginKind::local || origin.kind == BorrowSummaryOriginKind::temporary
        || (origin.kind == BorrowSummaryOriginKind::parameter && origin.storage_slot);
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

std::size_t SemanticAnalyzerCore::BorrowSummaryBuilder::OriginKeyHash::operator()(
    const OriginKey& key) const noexcept
{
    std::size_t hash = static_cast<std::size_t>(key.kind);
    hash = mix_borrow_summary_hash(hash, key.param_index);
    hash = mix_borrow_summary_hash(hash, key.name_id);
    hash = mix_borrow_summary_hash(hash, key.expr);
    hash = mix_borrow_summary_hash(hash, key.storage_slot ? 1U : 0U);
    hash = mix_borrow_summary_hash(hash, key.source);
    hash = mix_borrow_summary_hash(hash, key.range_begin);
    hash = mix_borrow_summary_hash(hash, key.range_end);
    return hash;
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
    this->origin_lookup_.clear();
    this->origin_lookup_.reserve(function.params.size());
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

void SemanticAnalyzerCore::BorrowSummaryBuilder::bind_pointer_value(const IdentId name, OriginSet origin)
{
    if (!syntax::is_valid(name) || this->scopes_.empty()) {
        return;
    }
    this->sort_unique(origin);
    this->scopes_.back().pointer_values[name] = std::move(origin);
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

void SemanticAnalyzerCore::BorrowSummaryBuilder::assign_pointer_value(const IdentId name, OriginSet origin)
{
    if (!syntax::is_valid(name)) {
        return;
    }
    this->sort_unique(origin);
    for (auto scope = this->scopes_.rbegin(); scope != this->scopes_.rend(); ++scope) {
        if (scope->storage.contains(name)) {
            scope->pointer_values[name] = std::move(origin);
            return;
        }
    }
    this->bind_pointer_value(name, std::move(origin));
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

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet SemanticAnalyzerCore::BorrowSummaryBuilder::lookup_pointer_value(
    const IdentId name) const
{
    if (!syntax::is_valid(name)) {
        return {};
    }
    for (auto scope = this->scopes_.rbegin(); scope != this->scopes_.rend(); ++scope) {
        const auto found = scope->pointer_values.find(name);
        if (found != scope->pointer_values.end()) {
            return found->second;
        }
    }
    return {};
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginKey
SemanticAnalyzerCore::BorrowSummaryBuilder::origin_key(const BorrowSummaryOrigin& origin) noexcept
{
    return OriginKey{
        .kind = static_cast<base::u8>(origin.kind),
        .param_index = origin.param_index,
        .name_id = origin.name_id.value,
        .expr = origin.expr.value,
        .storage_slot = origin.storage_slot,
        .source = origin.range.source.value,
        .range_begin = origin.range.begin,
        .range_end = origin.range.end,
    };
}

base::u32 SemanticAnalyzerCore::BorrowSummaryBuilder::append_origin(BorrowSummaryOrigin origin)
{
    const OriginKey key = this->origin_key(origin);
    if (const auto found = this->origin_lookup_.find(key); found != this->origin_lookup_.end()) {
        return found->second;
    }
    const base::u32 index = base::checked_u32(this->summary_.origins.size(), SEMA_BORROW_SUMMARY_ID_CONTEXT);
    this->summary_.origins.push_back(std::move(origin));
    this->origin_lookup_.emplace(key, index);
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

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet SemanticAnalyzerCore::BorrowSummaryBuilder::static_origin(
    const base::SourceRange& range)
{
    OriginSet origin;
    origin.origins.push_back(this->append_origin(BorrowSummaryOrigin{
        .kind = BorrowSummaryOriginKind::static_,
        .param_index = SEMA_BORROW_SUMMARY_INVALID_INDEX,
        .name_id = INVALID_IDENT_ID,
        .expr = syntax::INVALID_EXPR_ID,
        .range = range,
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
    if (origin.origins.size() < 2U) {
        return;
    }
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
            this->analyze_if_statement(tasks, stmt);
            break;
        case syntax::StmtKind::while_:
            this->analyze_while_statement(tasks, stmt);
            break;
        case syntax::StmtKind::for_:
            this->analyze_for_statement(tasks, stmt);
            break;
        case syntax::StmtKind::for_range:
            this->analyze_for_range_statement(tasks, stmt);
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

void SemanticAnalyzerCore::BorrowSummaryBuilder::analyze_if_statement(
    std::vector<Task>& tasks, const syntax::StmtNode& stmt)
{
    (void)tasks;
    const ScopeList baseline = this->scopes_;
    std::vector<ScopeList> branches;
    branches.reserve(3);
    if (syntax::is_valid(stmt.then_block)) {
        std::vector<Task> branch_tasks;
        branch_tasks.reserve(SEMA_BORROW_SUMMARY_INITIAL_TASK_CAPACITY);
        this->push_scoped_block(branch_tasks, stmt.then_block);
        branches.push_back(this->analyze_tasks_from_scopes(baseline, std::move(branch_tasks)));
    }
    if (syntax::is_valid(stmt.else_block)) {
        std::vector<Task> branch_tasks;
        branch_tasks.reserve(SEMA_BORROW_SUMMARY_INITIAL_TASK_CAPACITY);
        this->push_scoped_block(branch_tasks, stmt.else_block);
        branches.push_back(this->analyze_tasks_from_scopes(baseline, std::move(branch_tasks)));
    }
    if (syntax::is_valid(stmt.else_if)) {
        std::vector<Task> branch_tasks;
        branch_tasks.reserve(SEMA_BORROW_SUMMARY_INITIAL_TASK_CAPACITY);
        this->push_statement(branch_tasks, stmt.else_if);
        branches.push_back(this->analyze_tasks_from_scopes(baseline, std::move(branch_tasks)));
    }
    const bool include_fallthrough_path = !syntax::is_valid(stmt.else_block) && !syntax::is_valid(stmt.else_if);
    this->scopes_ = this->merge_control_flow_scopes(baseline, branches, include_fallthrough_path);
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::analyze_while_statement(
    std::vector<Task>& tasks, const syntax::StmtNode& stmt)
{
    (void)tasks;
    const ScopeList baseline = this->scopes_;
    std::vector<ScopeList> branches;
    branches.reserve(1);
    if (syntax::is_valid(stmt.body)) {
        std::vector<Task> body_tasks;
        body_tasks.reserve(SEMA_BORROW_SUMMARY_INITIAL_TASK_CAPACITY);
        this->push_scoped_block(body_tasks, stmt.body);
        branches.push_back(this->analyze_tasks_from_scopes(baseline, std::move(body_tasks)));
    }
    this->scopes_ = this->merge_control_flow_scopes(baseline, branches, true);
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::analyze_for_statement(
    std::vector<Task>& tasks, const syntax::StmtNode& stmt)
{
    (void)tasks;
    this->push_scope();
    std::vector<Task> init_tasks;
    init_tasks.reserve(SEMA_BORROW_SUMMARY_INITIAL_TASK_CAPACITY);
    this->push_statement(init_tasks, stmt.for_init);
    this->run_tasks(init_tasks);

    const ScopeList loop_baseline = this->scopes_;
    std::vector<Task> iteration_tasks;
    iteration_tasks.reserve(SEMA_BORROW_SUMMARY_INITIAL_TASK_CAPACITY);
    this->push_statement(iteration_tasks, stmt.for_update);
    this->push_scoped_block(iteration_tasks, stmt.body);
    std::vector<ScopeList> branches;
    branches.reserve(1);
    branches.push_back(this->analyze_tasks_from_scopes(loop_baseline, std::move(iteration_tasks)));
    this->scopes_ = this->merge_control_flow_scopes(loop_baseline, branches, true);
    this->pop_scope();
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::analyze_for_range_statement(
    std::vector<Task>& tasks, const syntax::StmtNode& stmt)
{
    (void)tasks;
    this->push_scope();
    this->bind_storage(stmt.name_id, this->local_origin(stmt.name_id, stmt.range, syntax::INVALID_EXPR_ID));
    this->bind_borrowed_value(stmt.name_id, {});

    const ScopeList loop_baseline = this->scopes_;
    std::vector<Task> body_tasks;
    body_tasks.reserve(SEMA_BORROW_SUMMARY_INITIAL_TASK_CAPACITY);
    this->push_scoped_block(body_tasks, stmt.body);
    std::vector<ScopeList> branches;
    branches.reserve(1);
    branches.push_back(this->analyze_tasks_from_scopes(loop_baseline, std::move(body_tasks)));
    this->scopes_ = this->merge_control_flow_scopes(loop_baseline, branches, true);
    this->pop_scope();
}

SemanticAnalyzerCore::BorrowSummaryBuilder::ScopeList
SemanticAnalyzerCore::BorrowSummaryBuilder::analyze_tasks_from_scopes(
    const ScopeList& baseline, std::vector<Task> tasks)
{
    ScopeList saved = std::move(this->scopes_);
    this->scopes_ = baseline;
    this->run_tasks(tasks);
    ScopeList result = this->scopes_;
    this->scopes_ = std::move(saved);
    return result;
}

SemanticAnalyzerCore::BorrowSummaryBuilder::ScopeList
SemanticAnalyzerCore::BorrowSummaryBuilder::merge_control_flow_scopes(
    const ScopeList& baseline, const std::vector<ScopeList>& branches, const bool include_baseline_path)
{
    ScopeList merged = baseline;
    if (!include_baseline_path) {
        this->clear_borrowed_values_for_existing_storage(merged);
        this->clear_pointer_values_for_existing_storage(merged);
    }
    for (const ScopeList& branch : branches) {
        const base::usize scope_count = std::min(merged.size(), branch.size());
        for (base::usize index = 0; index < scope_count; ++index) {
            this->merge_scope_borrowed_values(merged[index], branch[index]);
            this->merge_scope_pointer_values(merged[index], branch[index]);
        }
    }
    return merged;
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::merge_scope_borrowed_values(Scope& target, const Scope& branch)
{
    for (const auto& entry : target.storage) {
        const IdentId name = entry.first;
        const auto found = branch.borrowed_values.find(name);
        if (found == branch.borrowed_values.end()) {
            continue;
        }
        OriginSet current;
        if (const auto existing = target.borrowed_values.find(name); existing != target.borrowed_values.end()) {
            current = existing->second;
        }
        target.borrowed_values[name] = this->merge(std::move(current), found->second);
    }
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::merge_scope_pointer_values(Scope& target, const Scope& branch)
{
    for (const auto& entry : target.storage) {
        const IdentId name = entry.first;
        const auto found = branch.pointer_values.find(name);
        if (found == branch.pointer_values.end()) {
            continue;
        }
        OriginSet current;
        if (const auto existing = target.pointer_values.find(name); existing != target.pointer_values.end()) {
            current = existing->second;
        }
        target.pointer_values[name] = this->merge(std::move(current), found->second);
    }
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::clear_borrowed_values_for_existing_storage(ScopeList& scopes) const
{
    for (Scope& scope : scopes) {
        for (const auto& entry : scope.storage) {
            scope.borrowed_values[entry.first] = OriginSet{};
        }
    }
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::clear_pointer_values_for_existing_storage(ScopeList& scopes) const
{
    for (Scope& scope : scopes) {
        for (const auto& entry : scope.storage) {
            scope.pointer_values[entry.first] = OriginSet{};
        }
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
    this->bind_pointer_value(stmt.name_id,
        this->core_.state_.checked.types.is_pointer(local_type) ? this->pointer_origin(stmt.init) : OriginSet{});
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::analyze_assignment(const syntax::StmtNode& stmt)
{
    const IdentId assigned_name = this->unqualified_name_id(stmt.lhs);
    const TypeHandle lhs_type = this->core_.cached_expr_type(stmt.lhs);
    const OriginSet borrowed_origin =
        this->type_can_contain_borrow(lhs_type) ? this->borrow_origin(stmt.rhs) : OriginSet{};
    if (!syntax::is_valid(assigned_name)) {
        this->record_storage_escape(borrowed_origin, stmt.rhs, stmt.range);
        return;
    }
    this->assign_borrowed_value(assigned_name, borrowed_origin);
    this->assign_pointer_value(assigned_name,
        this->core_.state_.checked.types.is_pointer(lhs_type) ? this->pointer_origin(stmt.rhs) : OriginSet{});
}

void SemanticAnalyzerCore::BorrowSummaryBuilder::record_storage_escape(
    OriginSet origin, const syntax::ExprId expr, const base::SourceRange& range)
{
    this->sort_unique(origin);
    for (const base::u32 origin_index : origin.origins) {
        if (origin_index >= this->summary_.origins.size()
            || !summary_origin_is_local_escape_source(this->summary_.origins[origin_index])) {
            continue;
        }
        this->summary_.storage_escapes.push_back(FunctionBorrowStorageEscape{
            .origin_index = origin_index,
            .stored_expr = expr,
            .range = valid_expr(this->core_.ctx_.module, expr) ? expr_range(this->core_.ctx_.module, expr) : range,
        });
    }
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
    this->bind_pointer_value(pattern.binding_name_id,
        this->core_.state_.checked.types.is_pointer(type) ? this->pointer_origin(source) : OriginSet{});
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

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet SemanticAnalyzerCore::BorrowSummaryBuilder::pointer_name_origin(
    const syntax::ExprId expr) const
{
    return this->lookup_pointer_value(this->unqualified_name_id(expr));
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet SemanticAnalyzerCore::BorrowSummaryBuilder::storage_name_origin(
    const syntax::ExprId expr) const
{
    return this->lookup_storage(this->unqualified_name_id(expr));
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet
SemanticAnalyzerCore::BorrowSummaryBuilder::borrowed_carrier_origin(const syntax::ExprId expr)
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
        if (kind == syntax::ExprKind::name) {
            result = this->merge(result, this->borrowed_name_origin(current));
            continue;
        }
        if (const syntax::CallExprPayload* const call = this->core_.ctx_.module.exprs.call_payload(current.value);
            call != nullptr && (kind == syntax::ExprKind::call || kind == syntax::ExprKind::str_from_bytes_unchecked)) {
            result = this->merge(result, this->call_return_origin(current, *call));
            continue;
        }
        if (const syntax::SliceExprPayload* const slice = this->core_.ctx_.module.exprs.slice_payload(current.value);
            kind == syntax::ExprKind::slice && slice != nullptr) {
            result = this->merge(result, this->slice_source_origin(slice->object));
            continue;
        }
        if (const syntax::CastExprPayload* const cast = this->core_.ctx_.module.exprs.cast_payload(current.value);
            cast != nullptr && kind == syntax::ExprKind::str_from_utf8_checked) {
            pending.push_back(cast->expr);
            continue;
        }
        if (const syntax::BlockExprPayload* const block = this->core_.ctx_.module.exprs.block_payload(current.value);
            block != nullptr && (kind == syntax::ExprKind::block_expr || kind == syntax::ExprKind::unsafe_block)) {
            result = this->merge(result, this->block_result_borrow_origin(*block));
            continue;
        }
    }
    return result;
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet
SemanticAnalyzerCore::BorrowSummaryBuilder::addressed_place_origin(const syntax::ExprId expr)
{
    if (!valid_expr(this->core_.ctx_.module, expr)) {
        return {};
    }
    if (this->core_.ctx_.module.exprs.kind(expr.value) == syntax::ExprKind::name) {
        OriginSet origin = this->storage_name_origin(expr);
        OriginSet storage_origin;
        storage_origin.unknown = origin.unknown;
        for (base::u32 origin_index : origin.origins) {
            if (origin_index < this->summary_.origins.size()) {
                BorrowSummaryOrigin slot = this->summary_.origins[origin_index];
                slot.storage_slot = true;
                storage_origin.origins.push_back(this->append_origin(std::move(slot)));
                continue;
            }
            storage_origin.unknown = true;
        }
        return storage_origin;
    }
    return this->place_storage_origin(expr);
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
        if (!call.args.empty()) {
            OriginSet origin = this->pointer_origin(call.args.front());
            if (origin.unknown || !origin.origins.empty()) {
                return origin;
            }
        }
        return this->unknown_origin();
    }
    if (std::optional<OriginSet> enum_origin = this->enum_constructor_return_origin(call);
        enum_origin.has_value()) {
        return *enum_origin;
    }
    if (const FunctionCallBinding* const binding = this->direct_call_binding(expr); binding != nullptr) {
        const auto contract = this->core_.state_.checked.borrow_contracts.find(binding->function_key);
        if (contract != this->core_.state_.checked.borrow_contracts.end()
            && contract->second.source != FunctionBorrowContractSource::inferred) {
            return this->map_callee_contract_origins(
                contract->second, ordered_call_args_or_source(binding->ordered_args, call), binding->callee_expr,
                binding->receiver_arg_count, binding->receiver_auto_borrow);
        }
        const auto summary = this->core_.state_.checked.borrow_summaries.find(binding->function_key);
        if (summary != this->core_.state_.checked.borrow_summaries.end()) {
            return this->map_callee_summary_origins(
                summary->second, ordered_call_args_or_source(binding->ordered_args, call), binding->callee_expr,
                binding->receiver_arg_count, binding->receiver_auto_borrow);
        }
        if (contract != this->core_.state_.checked.borrow_contracts.end()) {
            return this->map_callee_contract_origins(
                contract->second, ordered_call_args_or_source(binding->ordered_args, call), binding->callee_expr,
                binding->receiver_arg_count, binding->receiver_auto_borrow);
        }
        return this->type_can_contain_borrow(binding->return_type) ? this->unknown_origin() : OriginSet{};
    }
    if (const TraitMethodCallBinding* const binding = this->trait_call_binding(expr); binding != nullptr) {
        const auto contract = this->core_.state_.checked.borrow_contracts.find(binding->function_key);
        const base::u32 receiver_arg_count = is_valid(binding->receiver_type) ? 1U : 0U;
        if (contract != this->core_.state_.checked.borrow_contracts.end()
            && contract->second.source != FunctionBorrowContractSource::inferred) {
            return this->map_callee_contract_origins(
                contract->second, ordered_call_args_or_source(binding->ordered_args, call), binding->callee_expr,
                receiver_arg_count, binding->receiver_auto_borrow);
        }
        const auto summary = this->core_.state_.checked.borrow_summaries.find(binding->function_key);
        if (summary != this->core_.state_.checked.borrow_summaries.end()) {
            return this->map_callee_summary_origins(
                summary->second, ordered_call_args_or_source(binding->ordered_args, call), binding->callee_expr,
                receiver_arg_count, binding->receiver_auto_borrow);
        }
        if (contract != this->core_.state_.checked.borrow_contracts.end()) {
            return this->map_callee_contract_origins(
                contract->second, ordered_call_args_or_source(binding->ordered_args, call), binding->callee_expr,
                receiver_arg_count, binding->receiver_auto_borrow);
        }
        return this->type_can_contain_borrow(binding->return_type) ? this->unknown_origin() : OriginSet{};
    }
    return this->type_can_contain_borrow(this->core_.cached_expr_type(expr)) ? this->unknown_origin() : OriginSet{};
}

std::optional<SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet>
SemanticAnalyzerCore::BorrowSummaryBuilder::enum_constructor_return_origin(const syntax::CallExprPayload& call)
{
    const EnumCaseInfo* const enum_case = this->core_.find_enum_constructor(call.callee, false);
    if (enum_case == nullptr) {
        return std::nullopt;
    }

    OriginSet result;
    const base::usize checked_arg_count = std::min(call.args.size(), enum_case->payload_types.size());
    for (base::usize index = 0; index < checked_arg_count; ++index) {
        if (this->type_can_contain_borrow(enum_case->payload_types[index])) {
            result = this->merge(result, this->borrow_origin(call.args[index]));
        }
    }
    if (this->type_can_contain_borrow(enum_case->type) && call.args.size() > enum_case->payload_types.size()) {
        result.unknown = true;
    }
    return result;
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet SemanticAnalyzerCore::BorrowSummaryBuilder::pointer_origin(
    const syntax::ExprId expr)
{
    enum class PointerOriginTaskKind : base::u8 {
        expression,
        pop_scope,
    };
    struct PointerOriginTask {
        PointerOriginTaskKind kind = PointerOriginTaskKind::expression;
        syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    };

    OriginSet result;
    std::vector<PointerOriginTask> pending{{PointerOriginTaskKind::expression, expr}};
    std::unordered_set<base::u32> visited;
    while (!pending.empty()) {
        const PointerOriginTask task = pending.back();
        pending.pop_back();
        if (task.kind == PointerOriginTaskKind::pop_scope) {
            this->pop_scope();
            continue;
        }
        const syntax::ExprId current = task.expr;
        if (!valid_expr(this->core_.ctx_.module, current) || !visited.insert(current.value).second) {
            continue;
        }
        const syntax::ExprKind kind = this->core_.ctx_.module.exprs.kind(current.value);
        if (kind == syntax::ExprKind::name) {
            result = this->merge(result, this->pointer_name_origin(current));
            continue;
        }
        if (const syntax::CallExprPayload* const call = this->core_.ctx_.module.exprs.call_payload(current.value);
            kind == syntax::ExprKind::call && call != nullptr) {
            const std::span<const syntax::ExprId> call_args =
                checked_ordered_call_args_or_source(this->core_.state_.checked, current, *call);
            for (const syntax::ExprId arg : call_args) {
                pending.push_back(PointerOriginTask{PointerOriginTaskKind::expression, arg});
            }
            pending.push_back(PointerOriginTask{PointerOriginTaskKind::expression, call->callee});
            continue;
        }
        if (const syntax::CastExprPayload* const cast = this->core_.ctx_.module.exprs.cast_payload(current.value);
            cast != nullptr
            && (kind == syntax::ExprKind::slice_data || kind == syntax::ExprKind::str_data
                || kind == syntax::ExprKind::cast || kind == syntax::ExprKind::pcast
                || kind == syntax::ExprKind::bcast || kind == syntax::ExprKind::paddr)) {
            if (kind == syntax::ExprKind::slice_data || kind == syntax::ExprKind::str_data) {
                result = this->merge(result, this->borrowed_carrier_origin(cast->expr));
                continue;
            }
            pending.push_back(PointerOriginTask{PointerOriginTaskKind::expression, cast->expr});
            continue;
        }
        if (const syntax::BlockExprPayload* const block = this->core_.ctx_.module.exprs.block_payload(current.value);
            block != nullptr && (kind == syntax::ExprKind::block_expr || kind == syntax::ExprKind::unsafe_block)) {
            this->push_scope();
            std::vector<Task> tasks;
            tasks.reserve(SEMA_BORROW_SUMMARY_INITIAL_TASK_CAPACITY);
            this->push_block_statements(tasks, block->block);
            this->run_tasks(tasks);
            pending.push_back(PointerOriginTask{PointerOriginTaskKind::pop_scope, syntax::INVALID_EXPR_ID});
            pending.push_back(PointerOriginTask{PointerOriginTaskKind::expression, block->result});
        }
    }
    return result;
}

const FunctionCallBinding* SemanticAnalyzerCore::BorrowSummaryBuilder::direct_call_binding(
    const syntax::ExprId call_expr) const noexcept
{
    return this->core_.state_.checked.function_call_binding_for_expr(call_expr);
}

const TraitMethodCallBinding* SemanticAnalyzerCore::BorrowSummaryBuilder::trait_call_binding(
    const syntax::ExprId call_expr) const noexcept
{
    return this->core_.state_.checked.trait_method_call_binding_for_expr(call_expr);
}

syntax::ExprId SemanticAnalyzerCore::BorrowSummaryBuilder::call_argument_for_param(
    const std::span<const syntax::ExprId> args, const syntax::ExprId callee, const base::u32 receiver_arg_count,
    const base::u32 param_index) const
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
    return arg_index < args.size() ? args[arg_index] : syntax::INVALID_EXPR_ID;
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet
SemanticAnalyzerCore::BorrowSummaryBuilder::call_origin_for_param(const std::span<const syntax::ExprId> args,
    const syntax::ExprId callee, const base::u32 receiver_arg_count, const base::u32 param_index,
    const bool receiver_auto_borrow)
{
    const syntax::ExprId arg = this->call_argument_for_param(args, callee, receiver_arg_count, param_index);
    if (!valid_expr(this->core_.ctx_.module, arg)) {
        return this->unknown_origin();
    }
    if (receiver_auto_borrow && receiver_arg_count != 0 && param_index < receiver_arg_count) {
        return this->place_storage_origin(arg);
    }
    return this->borrow_origin(arg);
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet
SemanticAnalyzerCore::BorrowSummaryBuilder::map_callee_summary_origins(const FunctionBorrowSummary& callee,
    const std::span<const syntax::ExprId> args, const syntax::ExprId callee_expr, const base::u32 receiver_arg_count,
    const bool receiver_auto_borrow)
{
    OriginSet result;
    result.unknown = callee.has_unknown_return_origin;
    for (const FunctionBorrowReturnOrigin& dependency : callee.return_origins) {
        if (dependency.origin_index >= callee.origins.size()) {
            result.unknown = true;
            continue;
        }
        const BorrowSummaryOrigin& origin = callee.origins[dependency.origin_index];
        if (origin.kind == BorrowSummaryOriginKind::static_) {
            result = this->merge(result, this->static_origin(origin.range));
            continue;
        }
        if (origin.kind != BorrowSummaryOriginKind::parameter) {
            result.unknown = true;
            continue;
        }
        result = this->merge(result,
            this->call_origin_for_param(
                args, callee_expr, receiver_arg_count, origin.param_index, receiver_auto_borrow));
    }
    return result;
}

SemanticAnalyzerCore::BorrowSummaryBuilder::OriginSet
SemanticAnalyzerCore::BorrowSummaryBuilder::map_callee_contract_origins(const FunctionBorrowContract& callee,
    const std::span<const syntax::ExprId> args, const syntax::ExprId callee_expr, const base::u32 receiver_arg_count,
    const bool receiver_auto_borrow)
{
    OriginSet result;
    result.unknown = callee.unknown_return_allowed;
    for (const BorrowContractSelector& selector : callee.return_selectors) {
        switch (selector.kind) {
            case BorrowContractSelectorKind::parameter:
            case BorrowContractSelectorKind::self: {
                if (selector.param_index == SEMA_BORROW_SUMMARY_INVALID_INDEX) {
                    result.unknown = true;
                    break;
                }
                result = this->merge(result,
                    this->call_origin_for_param(
                        args, callee_expr, receiver_arg_count, selector.param_index, receiver_auto_borrow));
                break;
            }
            case BorrowContractSelectorKind::static_:
                result = this->merge(result, this->static_origin(selector.range));
                break;
            case BorrowContractSelectorKind::unknown:
                result.unknown = true;
                break;
        }
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
            result = this->merge(result, this->addressed_place_origin(unary->operand));
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
        if (array_repeat_value_should_be_visited(array_repeat_runtime_semantics(
                this->core_.ctx_.module, this->core_.state_.checked.types, this->core_.cached_expr_type(expr), expr))) {
            pending.push_back(array->repeat_value);
        }
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
            case TypeKind::trait_object:
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
            || summary_origin.kind == BorrowSummaryOriginKind::temporary
            || (summary_origin.kind == BorrowSummaryOriginKind::parameter && summary_origin.storage_slot);
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

    std::ranges::sort(this->summary_.storage_escapes,
        [](const FunctionBorrowStorageEscape& lhs, const FunctionBorrowStorageEscape& rhs) {
            return std::tie(lhs.origin_index, lhs.stored_expr.value, lhs.range.source.value, lhs.range.begin,
                       lhs.range.end)
                < std::tie(rhs.origin_index, rhs.stored_expr.value, rhs.range.source.value, rhs.range.begin,
                    rhs.range.end);
        });
    this->summary_.storage_escapes.erase(
        std::ranges::unique(this->summary_.storage_escapes, same_storage_escape).begin(),
        this->summary_.storage_escapes.end());
    if (!this->summary_.return_type_can_contain_borrow && this->summary_.return_origins.empty()
        && this->summary_.storage_escapes.empty() && !this->summary_.has_unknown_return_origin
        && !this->summary_.has_local_return_escape) {
        this->summary_.origins.clear();
    }
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
        builder.mix_bool(origin.storage_slot);
    }
    builder.mix_u64(static_cast<base::u64>(this->summary_.return_origins.size()));
    for (const FunctionBorrowReturnOrigin& dependency : this->summary_.return_origins) {
        builder.mix_u32(dependency.origin_index);
        builder.mix_u32(dependency.return_expr.value);
    }
    builder.mix_u64(static_cast<base::u64>(this->summary_.storage_escapes.size()));
    for (const FunctionBorrowStorageEscape& escape : this->summary_.storage_escapes) {
        builder.mix_u32(escape.origin_index);
        builder.mix_u32(escape.stored_expr.value);
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
        case BorrowSummaryOriginKind::static_:
            return "static";
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
               << " param=" << origin.param_index << " storage_slot=" << (origin.storage_slot ? "true" : "false")
               << " name=";
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

    stream << "storage_escapes:\n";
    for (base::usize index = 0; index < summary.storage_escapes.size(); ++index) {
        const FunctionBorrowStorageEscape& escape = summary.storage_escapes[index];
        stream << "  s" << index << " origin=o" << escape.origin_index << " expr=";
        append_optional_expr_id(stream, escape.stored_expr);
        stream << " range=";
        append_range(stream, escape.range);
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
