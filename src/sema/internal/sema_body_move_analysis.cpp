#include <aurex/sema/resource_semantics.hpp>
#include <aurex/sema/sema_messages.hpp>

#include <algorithm>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <sema/internal/sema_body_move_analysis.hpp>

namespace aurex::sema {
namespace {

constexpr base::usize SEMA_MOVE_INVALID_LOCAL = static_cast<base::usize>(-1);
constexpr base::usize SEMA_MOVE_ENTRY_BLOCK = 0;
constexpr base::usize SEMA_MOVE_EXIT_BLOCK = 1;
constexpr base::usize SEMA_MOVE_INITIAL_BLOCK_CAPACITY = 16;
constexpr base::usize SEMA_MOVE_INITIAL_TASK_CAPACITY = 32;

enum class RequestedUse {
    owned,
    shared_borrow,
    mutable_borrow,
    place_only,
    initialized_place,
};

enum class MoveActionKind {
    use_local,
    initialize_local,
    reject_partial_field_move,
    reject_indexed_move,
    reject_pattern_payload,
    reject_try_payload,
};

struct MoveLocal {
    std::string_view name;
    TypeHandle type = INVALID_TYPE_HANDLE;
    base::SourceRange range{};
};

struct MoveAction {
    MoveActionKind kind = MoveActionKind::use_local;
    base::usize local = SEMA_MOVE_INVALID_LOCAL;
    syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    OwnedUseMode mode = OwnedUseMode::none;
    base::SourceRange range{};
    bool requires_initialized = true;
};

struct MoveBlock {
    std::vector<MoveAction> actions;
    std::vector<base::usize> successors;
    std::vector<base::usize> predecessors;
};

struct ConsumeOrigin {
    base::SourceRange range{};
    bool present = false;
};

struct MoveState {
    std::vector<bool> definitely_initialized;
    std::vector<bool> maybe_initialized;
    std::vector<ConsumeOrigin> consume_origins;
};

using MoveEnvironment = std::unordered_map<IdentId, base::usize, IdentIdHash>;

enum class BuildTaskKind {
    statement_list,
    statement,
    expression,
};

struct BuildTask {
    BuildTaskKind kind = BuildTaskKind::expression;
    syntax::StmtId stmt = syntax::INVALID_STMT_ID;
    syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    base::usize statement_index = 0;
    base::usize start = SEMA_MOVE_ENTRY_BLOCK;
    base::usize continuation = SEMA_MOVE_EXIT_BLOCK;
    base::usize break_target = SEMA_MOVE_EXIT_BLOCK;
    base::usize continue_target = SEMA_MOVE_EXIT_BLOCK;
    RequestedUse requested = RequestedUse::owned;
    MoveEnvironment environment;
};

struct ModeTask {
    BuildTaskKind kind = BuildTaskKind::expression;
    syntax::StmtId stmt = syntax::INVALID_STMT_ID;
    syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    RequestedUse requested = RequestedUse::owned;
};

[[nodiscard]] bool same_range(const base::SourceRange& lhs, const base::SourceRange& rhs) noexcept
{
    return lhs.source.value == rhs.source.value && lhs.begin == rhs.begin && lhs.end == rhs.end;
}

[[nodiscard]] bool same_state(const MoveState& lhs, const MoveState& rhs) noexcept
{
    if (lhs.definitely_initialized != rhs.definitely_initialized || lhs.maybe_initialized != rhs.maybe_initialized
        || lhs.consume_origins.size() != rhs.consume_origins.size()) {
        return false;
    }
    for (base::usize i = 0; i < lhs.consume_origins.size(); ++i) {
        if (lhs.consume_origins[i].present != rhs.consume_origins[i].present
            || (lhs.consume_origins[i].present
                && !same_range(lhs.consume_origins[i].range, rhs.consume_origins[i].range))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] OwnedUseMode requested_use_mode(const RequestedUse requested) noexcept
{
    switch (requested) {
        case RequestedUse::shared_borrow:
            return OwnedUseMode::shared_borrow;
        case RequestedUse::mutable_borrow:
            return OwnedUseMode::mutable_borrow;
        case RequestedUse::place_only:
        case RequestedUse::initialized_place:
            return OwnedUseMode::place_only;
        case RequestedUse::owned:
            break;
    }
    return OwnedUseMode::none;
}

} // namespace

class BodyMoveAnalysis final {
public:
    BodyMoveAnalysis(SemanticAnalyzerCore& core, const syntax::ItemNode& function, const FunctionSignature& signature)
        : core_(core), function_(function), signature_(signature),
          resources_(
              this->core_.state_.checked,
              [this](const TypeHandle type) {
                  return this->core_.generic_param_has_capability(type, CapabilityKind::copy);
              },
              [this](const TypeHandle type) {
                  return this->shared_structural_components(type);
              })
    {
    }

    void run()
    {
        MoveEnvironment environment;
        environment.reserve(this->function_.params.size());
        for (base::usize i = 0; i < this->function_.params.size(); ++i) {
            const syntax::ParamDecl& param = this->function_.params[i];
            const TypeHandle type =
                i < this->signature_.param_types.size() ? this->signature_.param_types[i] : INVALID_TYPE_HANDLE;
            const base::usize local = this->add_tracked_local(param.name, type, param.range, true);
            this->bind_environment(environment, param.name_id, local);
        }

        if (this->try_record_modes_without_move_state()) {
            return;
        }

        this->initialize_cfg_storage();
        this->push_statement_list(this->function_.body, SEMA_MOVE_ENTRY_BLOCK, SEMA_MOVE_EXIT_BLOCK, environment);
        this->build_cfg();
        this->solve_dataflow();
        this->emit_diagnostics();
    }

private:
    void initialize_cfg_storage()
    {
        if (!this->blocks_.empty()) {
            return;
        }
        this->blocks_.reserve(SEMA_MOVE_INITIAL_BLOCK_CAPACITY);
        this->tasks_.reserve(SEMA_MOVE_INITIAL_TASK_CAPACITY);
        static_cast<void>(this->new_block());
        static_cast<void>(this->new_block());
    }

    [[nodiscard]] base::usize new_block()
    {
        const base::usize index = this->blocks_.size();
        this->blocks_.emplace_back();
        return index;
    }

    void add_edge(const base::usize from, const base::usize to)
    {
        if (from >= this->blocks_.size() || to >= this->blocks_.size()) {
            return;
        }
        std::vector<base::usize>& successors = this->blocks_[from].successors;
        if (std::find(successors.begin(), successors.end(), to) != successors.end()) {
            return;
        }
        successors.push_back(to);
        this->blocks_[to].predecessors.push_back(from);
    }

    [[nodiscard]] base::usize add_local(
        const std::string_view name, const TypeHandle type, const base::SourceRange& range, const bool parameter)
    {
        const base::usize index = this->locals_.size();
        this->locals_.push_back(MoveLocal{name, type, range});
        this->parameter_locals_.push_back(parameter);
        return index;
    }

    [[nodiscard]] base::usize add_tracked_local(
        const std::string_view name, const TypeHandle type, const base::SourceRange& range, const bool parameter)
    {
        return this->is_tracked_resource_type(type) ? this->add_local(name, type, range, parameter)
                                                    : SEMA_MOVE_INVALID_LOCAL;
    }

    [[nodiscard]] bool is_tracked_resource_type(const TypeHandle type)
    {
        return is_valid(type) && this->core_.is_valid_storage_type(type)
            && !resource_is_copy(this->resource_summary(type));
    }

    void bind_environment(MoveEnvironment& environment, const IdentId name, const base::usize local) const
    {
        if (!is_valid(name)) {
            return;
        }
        if (local != SEMA_MOVE_INVALID_LOCAL) {
            environment[name] = local;
            return;
        }
        if (environment.contains(name)) {
            environment[name] = SEMA_MOVE_INVALID_LOCAL;
        }
    }

    [[nodiscard]] base::usize declare_plain_local(
        const syntax::StmtId stmt_id, const syntax::StmtNode& stmt, MoveEnvironment& environment)
    {
        if (stmt.kind != syntax::StmtKind::let && stmt.kind != syntax::StmtKind::var) {
            return SEMA_MOVE_INVALID_LOCAL;
        }
        if (syntax::is_valid(stmt.pattern) || !is_valid(stmt.name_id)) {
            return SEMA_MOVE_INVALID_LOCAL;
        }
        const auto existing = this->declared_locals_.find(stmt_id.value);
        if (existing != this->declared_locals_.end()) {
            this->bind_environment(environment, stmt.name_id, existing->second);
            return existing->second;
        }
        const base::usize local =
            this->add_tracked_local(stmt.name, this->core_.cached_stmt_local_type(stmt_id), stmt.range, false);
        this->declared_locals_.emplace(stmt_id.value, local);
        this->bind_environment(environment, stmt.name_id, local);
        return local;
    }

    [[nodiscard]] MoveEnvironment block_result_environment(const syntax::StmtId block, MoveEnvironment environment)
    {
        if (!syntax::is_valid(block) || block.value >= this->core_.ctx_.module.stmts.size()) {
            return environment;
        }
        const syntax::AstArenaVector<syntax::StmtId>* const statements =
            this->core_.ctx_.module.stmts.block_statements(block.value);
        if (statements == nullptr) {
            return environment;
        }
        for (const syntax::StmtId stmt_id : *statements) {
            if (!syntax::is_valid(stmt_id) || stmt_id.value >= this->core_.ctx_.module.stmts.size()) {
                continue;
            }
            const syntax::StmtNode stmt = this->core_.ctx_.module.stmts[stmt_id.value];
            static_cast<void>(this->declare_plain_local(stmt_id, stmt, environment));
        }
        return environment;
    }

    [[nodiscard]] ResourceSemanticsSummary resource_summary(const TypeHandle type)
    {
        if (!is_valid(type)) {
            return this->resources_.classify(type);
        }
        const auto found = this->resource_cache_.find(type.value);
        if (found != this->resource_cache_.end()) {
            return found->second;
        }
        const ResourceSemanticsSummary summary = this->resources_.classify(type);
        this->resource_cache_.emplace(type.value, summary);
        return summary;
    }

    [[nodiscard]] std::optional<std::vector<TypeHandle>> shared_structural_components(const TypeHandle type) const
    {
        std::vector<TypeHandle> components;
        if (!is_valid(type) || type.value >= this->core_.state_.checked.types.size()) {
            return std::nullopt;
        }
        const TypeInfo& info = this->core_.state_.checked.types.get(type);
        switch (info.kind) {
            case TypeKind::array:
                components.push_back(info.array_element);
                break;
            case TypeKind::tuple:
                components.insert(components.end(), info.tuple_elements.begin(), info.tuple_elements.end());
                break;
            case TypeKind::struct_: {
                const auto found = this->core_.state_.types.struct_infos_by_type.find(type.value);
                if (found != this->core_.state_.types.struct_infos_by_type.end() && found->second != nullptr) {
                    const std::span<const StructFieldInfo> fields = found->second->fields;
                    components.reserve(fields.size());
                    for (const StructFieldInfo& field : fields) {
                        components.push_back(field.type);
                    }
                    break;
                }
                return std::nullopt;
            }
            case TypeKind::enum_: {
                if (const SemanticAnalyzerCore::EnumCaseList* const cases = this->core_.find_enum_cases_by_type(type);
                    cases != nullptr) {
                    for (const EnumCaseInfo* const enum_case : *cases) {
                        if (enum_case != nullptr) {
                            components.insert(
                                components.end(), enum_case->payload_types.begin(), enum_case->payload_types.end());
                        }
                    }
                    break;
                }
                if (!is_valid(info.enum_underlying)) {
                    return std::nullopt;
                }
                break;
            }
            default:
                break;
        }
        return components;
    }

    [[nodiscard]] OwnedUseMode effective_mode(const syntax::ExprId expr, const RequestedUse requested)
    {
        const OwnedUseMode direct = requested_use_mode(requested);
        if (direct != OwnedUseMode::none) {
            return direct;
        }
        return this->is_tracked_resource_type(this->core_.cached_expr_type(expr)) ? OwnedUseMode::owned_consume
                                                                                  : OwnedUseMode::owned_copy;
    }

    void push_statement_list(const syntax::StmtId block, const base::usize start, const base::usize continuation,
        const MoveEnvironment& environment, const base::usize break_target = SEMA_MOVE_EXIT_BLOCK,
        const base::usize continue_target = SEMA_MOVE_EXIT_BLOCK, const base::usize statement_index = 0)
    {
        this->tasks_.push_back(BuildTask{
            BuildTaskKind::statement_list,
            block,
            syntax::INVALID_EXPR_ID,
            statement_index,
            start,
            continuation,
            break_target,
            continue_target,
            RequestedUse::owned,
            environment,
        });
    }

    void push_statement(const syntax::StmtId stmt, const base::usize start, const base::usize continuation,
        const MoveEnvironment& environment, const base::usize break_target = SEMA_MOVE_EXIT_BLOCK,
        const base::usize continue_target = SEMA_MOVE_EXIT_BLOCK)
    {
        this->tasks_.push_back(BuildTask{
            BuildTaskKind::statement,
            stmt,
            syntax::INVALID_EXPR_ID,
            0,
            start,
            continuation,
            break_target,
            continue_target,
            RequestedUse::owned,
            environment,
        });
    }

    void push_expression(const syntax::ExprId expr, const RequestedUse requested, const base::usize start,
        const base::usize continuation, const MoveEnvironment& environment)
    {
        this->tasks_.push_back(BuildTask{
            BuildTaskKind::expression,
            syntax::INVALID_STMT_ID,
            expr,
            0,
            start,
            continuation,
            SEMA_MOVE_EXIT_BLOCK,
            SEMA_MOVE_EXIT_BLOCK,
            requested,
            environment,
        });
    }

    void push_expression_sequence(const std::vector<std::pair<syntax::ExprId, RequestedUse>>& expressions,
        const base::usize start, const base::usize continuation, const MoveEnvironment& environment)
    {
        if (expressions.empty()) {
            this->add_edge(start, continuation);
            return;
        }
        std::vector<base::usize> boundaries(expressions.size() + 1, continuation);
        boundaries.front() = start;
        for (base::usize i = 1; i < expressions.size(); ++i) {
            boundaries[i] = this->new_block();
        }
        for (base::usize i = expressions.size(); i > 0; --i) {
            this->push_expression(
                expressions[i - 1].first, expressions[i - 1].second, boundaries[i - 1], boundaries[i], environment);
        }
    }

    [[nodiscard]] bool try_record_modes_without_move_state()
    {
        if (!this->locals_.empty()) {
            return false;
        }
        std::vector<ModeTask> mode_tasks;
        mode_tasks.reserve(SEMA_MOVE_INITIAL_TASK_CAPACITY);
        this->push_mode_statement_list(mode_tasks, this->function_.body);
        while (!mode_tasks.empty()) {
            const ModeTask task = mode_tasks.back();
            mode_tasks.pop_back();
            switch (task.kind) {
                case BuildTaskKind::statement_list:
                    this->push_mode_statement_list(mode_tasks, task.stmt);
                    break;
                case BuildTaskKind::statement:
                    if (!this->record_mode_statement(mode_tasks, task.stmt)) {
                        return false;
                    }
                    break;
                case BuildTaskKind::expression:
                    if (!this->record_mode_expression(mode_tasks, task.expr, task.requested)) {
                        return false;
                    }
                    break;
            }
        }
        return true;
    }

    void push_mode_statement_list(std::vector<ModeTask>& tasks, const syntax::StmtId block) const
    {
        if (!syntax::is_valid(block) || block.value >= this->core_.ctx_.module.stmts.size()) {
            return;
        }
        const syntax::AstArenaVector<syntax::StmtId>* const statements =
            this->core_.ctx_.module.stmts.block_statements(block.value);
        if (statements == nullptr) {
            return;
        }
        for (base::usize i = statements->size(); i > 0; --i) {
            tasks.push_back(ModeTask{
                BuildTaskKind::statement,
                (*statements)[i - 1],
                syntax::INVALID_EXPR_ID,
                RequestedUse::owned,
            });
        }
    }

    void push_mode_expression(
        std::vector<ModeTask>& tasks, const syntax::ExprId expr, const RequestedUse requested) const
    {
        if (!syntax::is_valid(expr) || expr.value >= this->core_.ctx_.module.exprs.size()) {
            return;
        }
        tasks.push_back(ModeTask{
            BuildTaskKind::expression,
            syntax::INVALID_STMT_ID,
            expr,
            requested,
        });
    }

    void push_mode_block(std::vector<ModeTask>& tasks, const syntax::StmtId block) const
    {
        if (!syntax::is_valid(block)) {
            return;
        }
        tasks.push_back(ModeTask{
            BuildTaskKind::statement_list,
            block,
            syntax::INVALID_EXPR_ID,
            RequestedUse::owned,
        });
    }

    [[nodiscard]] bool record_mode_statement(std::vector<ModeTask>& tasks, const syntax::StmtId stmt_id)
    {
        if (!syntax::is_valid(stmt_id) || stmt_id.value >= this->core_.ctx_.module.stmts.size()) {
            return true;
        }
        const syntax::StmtNode stmt = this->core_.ctx_.module.stmts[stmt_id.value];
        switch (stmt.kind) {
            case syntax::StmtKind::let:
            case syntax::StmtKind::var:
                if (!this->copy_only_local_declaration(stmt_id, stmt)) {
                    return false;
                }
                this->push_mode_expression(tasks, stmt.init, RequestedUse::owned);
                break;
            case syntax::StmtKind::assign:
                this->push_mode_expression(tasks, stmt.rhs, RequestedUse::owned);
                this->push_mode_expression(tasks, stmt.lhs,
                    stmt.assign_op == syntax::AssignOp::assign ? RequestedUse::place_only : RequestedUse::owned);
                break;
            case syntax::StmtKind::if_:
                this->push_mode_block(tasks, stmt.else_block);
                if (syntax::is_valid(stmt.else_if)) {
                    tasks.push_back(ModeTask{
                        BuildTaskKind::statement,
                        stmt.else_if,
                        syntax::INVALID_EXPR_ID,
                        RequestedUse::owned,
                    });
                }
                this->push_mode_block(tasks, stmt.then_block);
                this->push_mode_expression(tasks, stmt.condition, RequestedUse::owned);
                break;
            case syntax::StmtKind::while_:
                this->push_mode_block(tasks, stmt.body);
                this->push_mode_expression(tasks, stmt.condition, RequestedUse::owned);
                break;
            case syntax::StmtKind::for_:
                if (syntax::is_valid(stmt.for_update)) {
                    tasks.push_back(ModeTask{
                        BuildTaskKind::statement,
                        stmt.for_update,
                        syntax::INVALID_EXPR_ID,
                        RequestedUse::owned,
                    });
                }
                this->push_mode_block(tasks, stmt.body);
                this->push_mode_expression(tasks, stmt.condition, RequestedUse::owned);
                if (syntax::is_valid(stmt.for_init)) {
                    tasks.push_back(ModeTask{
                        BuildTaskKind::statement,
                        stmt.for_init,
                        syntax::INVALID_EXPR_ID,
                        RequestedUse::owned,
                    });
                }
                break;
            case syntax::StmtKind::for_range:
                this->push_mode_block(tasks, stmt.body);
                this->push_mode_expression(tasks, stmt.range_step, RequestedUse::owned);
                this->push_mode_expression(tasks, stmt.range_end, RequestedUse::owned);
                this->push_mode_expression(tasks, stmt.range_start, RequestedUse::owned);
                break;
            case syntax::StmtKind::return_:
                this->push_mode_expression(tasks, stmt.return_value, RequestedUse::owned);
                break;
            case syntax::StmtKind::expr:
                this->push_mode_expression(tasks, stmt.init, RequestedUse::owned);
                break;
            case syntax::StmtKind::block:
                this->push_mode_block(tasks, stmt_id);
                break;
            case syntax::StmtKind::break_:
            case syntax::StmtKind::continue_:
            case syntax::StmtKind::defer:
                break;
        }
        return true;
    }

    [[nodiscard]] bool copy_only_local_declaration(const syntax::StmtId stmt_id, const syntax::StmtNode& stmt)
    {
        if (syntax::is_valid(stmt.pattern)) {
            return !this->is_tracked_resource_type(this->core_.cached_expr_type(stmt.init));
        }
        if (!is_valid(stmt.name_id)) {
            return true;
        }
        return !this->is_tracked_resource_type(this->core_.cached_stmt_local_type(stmt_id));
    }

    [[nodiscard]] bool record_mode_expression(
        std::vector<ModeTask>& tasks, const syntax::ExprId expr_id, const RequestedUse requested)
    {
        if (!syntax::is_valid(expr_id) || expr_id.value >= this->core_.ctx_.module.exprs.size()) {
            return true;
        }
        const SemanticAnalyzerCore::ExprView expr = this->core_.expr_view(expr_id);
        if (this->expression_requires_full_move_analysis(expr_id, expr, requested)) {
            return false;
        }
        this->push_mode_expression_children(tasks, expr, requested);
        return true;
    }

    [[nodiscard]] bool expression_requires_full_move_analysis(
        const syntax::ExprId expr_id, const SemanticAnalyzerCore::ExprView& expr, const RequestedUse requested)
    {
        switch (expr.kind) {
            case syntax::ExprKind::try_expr:
                return this->is_tracked_resource_type(this->core_.cached_expr_type(expr.try_operand));
            case syntax::ExprKind::match_expr:
                if (!this->is_tracked_resource_type(this->core_.cached_expr_type(expr.match_value))) {
                    return false;
                }
                return std::ranges::any_of(expr.match_arms, [this](const syntax::MatchArm& arm) {
                    return this->pattern_has_bindings(arm.pattern);
                });
            case syntax::ExprKind::field:
            case syntax::ExprKind::index:
                return requested == RequestedUse::owned
                    && this->is_tracked_resource_type(this->core_.cached_expr_type(expr_id));
            default:
                break;
        }
        return false;
    }

    void push_mode_expression_children(
        std::vector<ModeTask>& tasks, const SemanticAnalyzerCore::ExprView& expr, const RequestedUse requested)
    {
        switch (expr.kind) {
            case syntax::ExprKind::name:
                break;
            case syntax::ExprKind::generic_apply:
                this->push_mode_expression(tasks, expr.callee, RequestedUse::place_only);
                break;
            case syntax::ExprKind::unary:
                this->push_mode_expression(tasks, expr.unary_operand, this->unary_operand_use(expr.unary_op));
                break;
            case syntax::ExprKind::binary:
                this->push_mode_expression(tasks, expr.binary_rhs, RequestedUse::owned);
                this->push_mode_expression(tasks, expr.binary_lhs, RequestedUse::owned);
                break;
            case syntax::ExprKind::call:
            case syntax::ExprKind::str_from_bytes_unchecked:
                for (base::usize i = expr.args.size(); i > 0; --i) {
                    this->push_mode_expression(tasks, expr.args[i - 1], RequestedUse::owned);
                }
                this->push_mode_expression(tasks, expr.callee, RequestedUse::place_only);
                break;
            case syntax::ExprKind::try_expr:
                this->push_mode_expression(tasks, expr.try_operand, RequestedUse::owned);
                break;
            case syntax::ExprKind::if_expr:
                this->push_mode_expression(tasks, expr.else_expr, requested);
                this->push_mode_expression(tasks, expr.then_expr, requested);
                this->push_mode_expression(tasks, expr.condition, RequestedUse::owned);
                break;
            case syntax::ExprKind::block_expr:
            case syntax::ExprKind::unsafe_block:
                this->push_mode_expression(tasks, expr.block_result, requested);
                this->push_mode_block(tasks, expr.block);
                break;
            case syntax::ExprKind::match_expr:
                for (base::usize i = expr.match_arms.size(); i > 0; --i) {
                    const syntax::MatchArm& arm = expr.match_arms[i - 1];
                    this->push_mode_expression(tasks, arm.value, requested);
                    this->push_mode_expression(tasks, arm.guard, RequestedUse::owned);
                }
                this->push_mode_expression(tasks, expr.match_value, RequestedUse::owned);
                break;
            case syntax::ExprKind::array_literal:
                this->push_mode_expression(tasks, expr.array_repeat_count, RequestedUse::owned);
                this->push_mode_expression(tasks, expr.array_repeat_value, RequestedUse::owned);
                for (base::usize i = expr.array_elements.size(); i > 0; --i) {
                    this->push_mode_expression(tasks, expr.array_elements[i - 1], RequestedUse::owned);
                }
                break;
            case syntax::ExprKind::tuple_literal:
                for (base::usize i = expr.tuple_elements.size(); i > 0; --i) {
                    this->push_mode_expression(tasks, expr.tuple_elements[i - 1], RequestedUse::owned);
                }
                break;
            case syntax::ExprKind::struct_literal:
                for (base::usize i = expr.field_inits.size(); i > 0; --i) {
                    this->push_mode_expression(tasks, expr.field_inits[i - 1].value, RequestedUse::owned);
                }
                break;
            case syntax::ExprKind::field:
                this->push_mode_expression(tasks, expr.object, RequestedUse::initialized_place);
                break;
            case syntax::ExprKind::index:
                this->push_mode_expression(tasks, expr.index, RequestedUse::owned);
                this->push_mode_expression(tasks, expr.object, RequestedUse::initialized_place);
                break;
            case syntax::ExprKind::slice:
                this->push_mode_expression(tasks, expr.slice_end, RequestedUse::owned);
                this->push_mode_expression(tasks, expr.slice_start, RequestedUse::owned);
                this->push_mode_expression(tasks, expr.object, RequestedUse::initialized_place);
                break;
            case syntax::ExprKind::cast:
            case syntax::ExprKind::pcast:
            case syntax::ExprKind::bcast:
            case syntax::ExprKind::ptr_addr:
            case syntax::ExprKind::paddr:
            case syntax::ExprKind::slice_data:
            case syntax::ExprKind::slice_len:
            case syntax::ExprKind::str_data:
            case syntax::ExprKind::str_byte_len:
            case syntax::ExprKind::str_is_valid_utf8:
            case syntax::ExprKind::str_from_utf8_checked:
                this->push_mode_expression(tasks, expr.cast_expr, RequestedUse::owned);
                break;
            case syntax::ExprKind::invalid:
            case syntax::ExprKind::integer_literal:
            case syntax::ExprKind::float_literal:
            case syntax::ExprKind::bool_literal:
            case syntax::ExprKind::null_literal:
            case syntax::ExprKind::string_literal:
            case syntax::ExprKind::c_string_literal:
            case syntax::ExprKind::raw_string_literal:
            case syntax::ExprKind::byte_string_literal:
            case syntax::ExprKind::byte_literal:
            case syntax::ExprKind::char_literal:
            case syntax::ExprKind::size_of:
            case syntax::ExprKind::align_of:
                break;
        }
    }

    void build_cfg()
    {
        while (!this->tasks_.empty()) {
            BuildTask task = std::move(this->tasks_.back());
            this->tasks_.pop_back();
            switch (task.kind) {
                case BuildTaskKind::statement_list:
                    this->build_statement_list(std::move(task));
                    break;
                case BuildTaskKind::statement:
                    this->build_statement(std::move(task));
                    break;
                case BuildTaskKind::expression:
                    this->build_expression(std::move(task));
                    break;
            }
        }
    }

    void build_statement_list(BuildTask task)
    {
        if (!syntax::is_valid(task.stmt) || task.stmt.value >= this->core_.ctx_.module.stmts.size()) {
            this->add_edge(task.start, task.continuation);
            return;
        }
        const syntax::AstArenaVector<syntax::StmtId>* const statements =
            this->core_.ctx_.module.stmts.block_statements(task.stmt.value);
        if (statements == nullptr || task.statement_index >= statements->size()) {
            this->add_edge(task.start, task.continuation);
            return;
        }

        const syntax::StmtId stmt_id = (*statements)[task.statement_index];
        const base::usize next = task.statement_index + 1 == statements->size() ? task.continuation : this->new_block();
        MoveEnvironment next_environment = task.environment;
        if (syntax::is_valid(stmt_id) && stmt_id.value < this->core_.ctx_.module.stmts.size()) {
            const syntax::StmtNode stmt = this->core_.ctx_.module.stmts[stmt_id.value];
            static_cast<void>(this->declare_plain_local(stmt_id, stmt, next_environment));
        }
        if (task.statement_index + 1 < statements->size()) {
            this->push_statement_list(task.stmt, next, task.continuation, next_environment, task.break_target,
                task.continue_target, task.statement_index + 1);
        }
        this->push_statement(stmt_id, task.start, next, task.environment, task.break_target, task.continue_target);
    }

    void build_statement(BuildTask task)
    {
        if (!syntax::is_valid(task.stmt) || task.stmt.value >= this->core_.ctx_.module.stmts.size()) {
            this->add_edge(task.start, task.continuation);
            return;
        }
        const syntax::StmtNode stmt = this->core_.ctx_.module.stmts[task.stmt.value];
        switch (stmt.kind) {
            case syntax::StmtKind::let:
            case syntax::StmtKind::var: {
                const base::usize initialized = this->declared_local(task.stmt);
                const base::usize finish = this->new_block();
                if (initialized != SEMA_MOVE_INVALID_LOCAL) {
                    this->blocks_[finish].actions.push_back(MoveAction{MoveActionKind::initialize_local, initialized,
                        syntax::INVALID_EXPR_ID, OwnedUseMode::none, stmt.range});
                } else if (syntax::is_valid(stmt.pattern)
                    && this->is_tracked_resource_type(this->core_.cached_expr_type(stmt.init))) {
                    this->blocks_[finish].actions.push_back(MoveAction{MoveActionKind::reject_pattern_payload,
                        SEMA_MOVE_INVALID_LOCAL, syntax::INVALID_EXPR_ID, OwnedUseMode::none, stmt.range});
                }
                this->add_edge(finish, task.continuation);
                this->push_expression(stmt.init, RequestedUse::owned, task.start, finish, task.environment);
                break;
            }
            case syntax::StmtKind::assign:
                this->build_assignment(stmt, task);
                break;
            case syntax::StmtKind::if_:
                this->build_if_statement(stmt, task);
                break;
            case syntax::StmtKind::while_:
                this->build_while_statement(stmt, task);
                break;
            case syntax::StmtKind::for_:
                this->build_for_statement(stmt, task);
                break;
            case syntax::StmtKind::for_range:
                this->build_for_range_statement(stmt, task);
                break;
            case syntax::StmtKind::return_:
                if (syntax::is_valid(stmt.return_value)) {
                    this->push_expression(
                        stmt.return_value, RequestedUse::owned, task.start, SEMA_MOVE_EXIT_BLOCK, task.environment);
                } else {
                    this->add_edge(task.start, SEMA_MOVE_EXIT_BLOCK);
                }
                break;
            case syntax::StmtKind::expr:
                this->push_expression(stmt.init, RequestedUse::owned, task.start, task.continuation, task.environment);
                break;
            case syntax::StmtKind::block:
                this->push_statement_list(task.stmt, task.start, task.continuation, task.environment, task.break_target,
                    task.continue_target);
                break;
            case syntax::StmtKind::break_:
                this->add_edge(task.start, task.break_target);
                break;
            case syntax::StmtKind::continue_:
                this->add_edge(task.start, task.continue_target);
                break;
            case syntax::StmtKind::defer:
                this->add_edge(task.start, task.continuation);
                break;
        }
    }

    void build_assignment(const syntax::StmtNode& stmt, const BuildTask& task)
    {
        const base::usize finish = this->new_block();
        const std::optional<base::usize> whole_local = this->whole_local(stmt.lhs, task.environment);
        if (whole_local.has_value()) {
            this->blocks_[finish].actions.push_back(MoveAction{MoveActionKind::initialize_local, *whole_local,
                syntax::INVALID_EXPR_ID, OwnedUseMode::none, stmt.range});
        }
        this->add_edge(finish, task.continuation);
        this->push_expression_sequence(
            {
                {stmt.lhs, stmt.assign_op == syntax::AssignOp::assign ? RequestedUse::place_only : RequestedUse::owned},
                {stmt.rhs, RequestedUse::owned},
            },
            task.start, finish, task.environment);
    }

    void build_if_statement(const syntax::StmtNode& stmt, const BuildTask& task)
    {
        const base::usize condition = this->new_block();
        const base::usize then_entry = this->new_block();
        const base::usize else_entry = this->new_block();
        this->push_expression(stmt.condition, RequestedUse::owned, task.start, condition, task.environment);
        this->add_edge(condition, then_entry);
        this->add_edge(condition, else_entry);
        this->push_statement_list(
            stmt.then_block, then_entry, task.continuation, task.environment, task.break_target, task.continue_target);
        if (syntax::is_valid(stmt.else_if)) {
            this->push_statement(
                stmt.else_if, else_entry, task.continuation, task.environment, task.break_target, task.continue_target);
        } else if (syntax::is_valid(stmt.else_block)) {
            this->push_statement_list(stmt.else_block, else_entry, task.continuation, task.environment,
                task.break_target, task.continue_target);
        } else {
            this->add_edge(else_entry, task.continuation);
        }
    }

    void build_while_statement(const syntax::StmtNode& stmt, const BuildTask& task)
    {
        const base::usize condition = this->new_block();
        const base::usize decision = this->new_block();
        const base::usize body = this->new_block();
        this->add_edge(task.start, condition);
        this->push_expression(stmt.condition, RequestedUse::owned, condition, decision, task.environment);
        this->add_edge(decision, body);
        this->add_edge(decision, task.continuation);
        this->push_statement_list(stmt.body, body, condition, task.environment, task.continuation, condition);
    }

    void build_for_statement(const syntax::StmtNode& stmt, const BuildTask& task)
    {
        MoveEnvironment loop_environment = task.environment;
        if (syntax::is_valid(stmt.for_init) && stmt.for_init.value < this->core_.ctx_.module.stmts.size()) {
            const syntax::StmtNode init = this->core_.ctx_.module.stmts[stmt.for_init.value];
            static_cast<void>(this->declare_plain_local(stmt.for_init, init, loop_environment));
        }
        const base::usize condition = this->new_block();
        const base::usize decision = this->new_block();
        const base::usize body = this->new_block();
        const base::usize update = this->new_block();
        if (syntax::is_valid(stmt.for_init)) {
            this->push_statement(stmt.for_init, task.start, condition, task.environment, task.continuation, update);
        } else {
            this->add_edge(task.start, condition);
        }
        if (syntax::is_valid(stmt.condition)) {
            this->push_expression(stmt.condition, RequestedUse::owned, condition, decision, loop_environment);
        } else {
            this->add_edge(condition, decision);
        }
        this->add_edge(decision, body);
        this->add_edge(decision, task.continuation);
        this->push_statement_list(stmt.body, body, update, loop_environment, task.continuation, update);
        if (syntax::is_valid(stmt.for_update)) {
            this->push_statement(stmt.for_update, update, condition, loop_environment, task.continuation, update);
        } else {
            this->add_edge(update, condition);
        }
    }

    void build_for_range_statement(const syntax::StmtNode& stmt, const BuildTask& task)
    {
        const base::usize header = this->new_block();
        const base::usize body = this->new_block();
        std::vector<std::pair<syntax::ExprId, RequestedUse>> bounds;
        if (syntax::is_valid(stmt.range_start)) {
            bounds.emplace_back(stmt.range_start, RequestedUse::owned);
        }
        if (syntax::is_valid(stmt.range_end)) {
            bounds.emplace_back(stmt.range_end, RequestedUse::owned);
        }
        if (syntax::is_valid(stmt.range_step)) {
            bounds.emplace_back(stmt.range_step, RequestedUse::owned);
        }
        this->push_expression_sequence(bounds, task.start, header, task.environment);
        this->add_edge(header, body);
        this->add_edge(header, task.continuation);
        this->push_statement_list(stmt.body, body, header, task.environment, task.continuation, header);
    }

    void build_expression(BuildTask task)
    {
        if (!syntax::is_valid(task.expr) || task.expr.value >= this->core_.ctx_.module.exprs.size()) {
            this->add_edge(task.start, task.continuation);
            return;
        }
        const SemanticAnalyzerCore::ExprView expr = this->core_.expr_view(task.expr);
        const OwnedUseMode mode = this->effective_mode(task.expr, task.requested);
        switch (expr.kind) {
            case syntax::ExprKind::name:
                this->build_name_expression(task, expr, mode);
                break;
            case syntax::ExprKind::generic_apply:
                this->push_expression(
                    expr.callee, RequestedUse::place_only, task.start, task.continuation, task.environment);
                break;
            case syntax::ExprKind::unary:
                this->push_expression(expr.unary_operand, this->unary_operand_use(expr.unary_op), task.start,
                    task.continuation, task.environment);
                break;
            case syntax::ExprKind::binary:
                this->build_binary_expression(task, expr);
                break;
            case syntax::ExprKind::call:
            case syntax::ExprKind::str_from_bytes_unchecked:
                this->build_call_expression(task, expr);
                break;
            case syntax::ExprKind::try_expr:
                this->build_try_expression(task, expr);
                break;
            case syntax::ExprKind::if_expr:
                this->build_if_expression(task, expr);
                break;
            case syntax::ExprKind::block_expr:
            case syntax::ExprKind::unsafe_block:
                this->build_block_expression(task, expr);
                break;
            case syntax::ExprKind::match_expr:
                this->build_match_expression(task, expr);
                break;
            case syntax::ExprKind::array_literal:
                this->build_array_expression(task, expr);
                break;
            case syntax::ExprKind::tuple_literal: {
                std::vector<std::pair<syntax::ExprId, RequestedUse>> elements;
                for (const syntax::ExprId element : expr.tuple_elements) {
                    elements.emplace_back(element, RequestedUse::owned);
                }
                this->push_expression_sequence(elements, task.start, task.continuation, task.environment);
                break;
            }
            case syntax::ExprKind::struct_literal: {
                std::vector<std::pair<syntax::ExprId, RequestedUse>> fields;
                for (const syntax::FieldInit& field : expr.field_inits) {
                    fields.emplace_back(field.value, RequestedUse::owned);
                }
                this->push_expression_sequence(fields, task.start, task.continuation, task.environment);
                break;
            }
            case syntax::ExprKind::field:
                this->build_field_expression(task, expr);
                break;
            case syntax::ExprKind::index:
                this->build_index_expression(task, expr);
                break;
            case syntax::ExprKind::slice:
                this->build_slice_expression(task, expr);
                break;
            case syntax::ExprKind::cast:
            case syntax::ExprKind::pcast:
            case syntax::ExprKind::bcast:
            case syntax::ExprKind::ptr_addr:
            case syntax::ExprKind::paddr:
            case syntax::ExprKind::slice_data:
            case syntax::ExprKind::slice_len:
            case syntax::ExprKind::str_data:
            case syntax::ExprKind::str_byte_len:
            case syntax::ExprKind::str_is_valid_utf8:
            case syntax::ExprKind::str_from_utf8_checked:
                this->push_expression(
                    expr.cast_expr, RequestedUse::owned, task.start, task.continuation, task.environment);
                break;
            case syntax::ExprKind::invalid:
            case syntax::ExprKind::integer_literal:
            case syntax::ExprKind::float_literal:
            case syntax::ExprKind::bool_literal:
            case syntax::ExprKind::null_literal:
            case syntax::ExprKind::string_literal:
            case syntax::ExprKind::c_string_literal:
            case syntax::ExprKind::raw_string_literal:
            case syntax::ExprKind::byte_string_literal:
            case syntax::ExprKind::byte_literal:
            case syntax::ExprKind::char_literal:
            case syntax::ExprKind::size_of:
            case syntax::ExprKind::align_of:
                this->add_edge(task.start, task.continuation);
                break;
        }
    }

    void build_name_expression(
        const BuildTask& task, const SemanticAnalyzerCore::ExprView& expr, const OwnedUseMode mode)
    {
        if (expr.scope_name.empty()) {
            const auto found = task.environment.find(expr.text_id);
            if (found != task.environment.end() && found->second != SEMA_MOVE_INVALID_LOCAL) {
                this->blocks_[task.start].actions.push_back(MoveAction{
                    MoveActionKind::use_local,
                    found->second,
                    task.expr,
                    mode,
                    expr.range,
                    task.requested != RequestedUse::place_only,
                });
            }
        }
        this->add_edge(task.start, task.continuation);
    }

    void build_binary_expression(const BuildTask& task, const SemanticAnalyzerCore::ExprView& expr)
    {
        if (expr.binary_op == syntax::BinaryOp::logical_and || expr.binary_op == syntax::BinaryOp::logical_or) {
            const base::usize decision = this->new_block();
            const base::usize rhs = this->new_block();
            this->push_expression(expr.binary_lhs, RequestedUse::owned, task.start, decision, task.environment);
            this->add_edge(decision, rhs);
            this->add_edge(decision, task.continuation);
            this->push_expression(expr.binary_rhs, RequestedUse::owned, rhs, task.continuation, task.environment);
            return;
        }
        this->push_expression_sequence(
            {
                {expr.binary_lhs, RequestedUse::owned},
                {expr.binary_rhs, RequestedUse::owned},
            },
            task.start, task.continuation, task.environment);
    }

    void build_call_expression(const BuildTask& task, const SemanticAnalyzerCore::ExprView& expr)
    {
        std::vector<std::pair<syntax::ExprId, RequestedUse>> operands;
        operands.emplace_back(expr.callee, RequestedUse::place_only);
        for (const syntax::ExprId arg : expr.args) {
            operands.emplace_back(arg, RequestedUse::owned);
        }
        this->push_expression_sequence(operands, task.start, task.continuation, task.environment);
    }

    void build_try_expression(const BuildTask& task, const SemanticAnalyzerCore::ExprView& expr)
    {
        if (this->is_tracked_resource_type(this->core_.cached_expr_type(expr.try_operand))) {
            this->blocks_[task.start].actions.push_back(MoveAction{MoveActionKind::reject_try_payload,
                SEMA_MOVE_INVALID_LOCAL, task.expr, OwnedUseMode::none, expr.range});
        }
        this->push_expression(expr.try_operand, RequestedUse::owned, task.start, task.continuation, task.environment);
    }

    void build_if_expression(const BuildTask& task, const SemanticAnalyzerCore::ExprView& expr)
    {
        const base::usize decision = this->new_block();
        const base::usize then_entry = this->new_block();
        const base::usize else_entry = this->new_block();
        this->push_expression(expr.condition, RequestedUse::owned, task.start, decision, task.environment);
        this->add_edge(decision, then_entry);
        this->add_edge(decision, else_entry);
        this->push_expression(expr.then_expr, task.requested, then_entry, task.continuation, task.environment);
        this->push_expression(expr.else_expr, task.requested, else_entry, task.continuation, task.environment);
    }

    void build_block_expression(const BuildTask& task, const SemanticAnalyzerCore::ExprView& expr)
    {
        const base::usize result = this->new_block();
        const MoveEnvironment result_environment = this->block_result_environment(expr.block, task.environment);
        this->push_statement_list(expr.block, task.start, result, task.environment);
        this->push_expression(expr.block_result, task.requested, result, task.continuation, result_environment);
    }

    void build_match_expression(const BuildTask& task, const SemanticAnalyzerCore::ExprView& expr)
    {
        const base::usize dispatch = this->new_block();
        if (this->is_tracked_resource_type(this->core_.cached_expr_type(expr.match_value))) {
            for (const syntax::MatchArm& arm : expr.match_arms) {
                if (this->pattern_has_bindings(arm.pattern)) {
                    this->blocks_[task.start].actions.push_back(MoveAction{MoveActionKind::reject_pattern_payload,
                        SEMA_MOVE_INVALID_LOCAL, syntax::INVALID_EXPR_ID, OwnedUseMode::none, arm.range});
                }
            }
        }
        this->push_expression(expr.match_value, RequestedUse::owned, task.start, dispatch, task.environment);
        for (const syntax::MatchArm& arm : expr.match_arms) {
            const base::usize arm_entry = this->new_block();
            this->add_edge(dispatch, arm_entry);
            if (syntax::is_valid(arm.guard)) {
                const base::usize value = this->new_block();
                this->push_expression(arm.guard, RequestedUse::owned, arm_entry, value, task.environment);
                this->push_expression(arm.value, task.requested, value, task.continuation, task.environment);
            } else {
                this->push_expression(arm.value, task.requested, arm_entry, task.continuation, task.environment);
            }
        }
    }

    void build_array_expression(const BuildTask& task, const SemanticAnalyzerCore::ExprView& expr)
    {
        std::vector<std::pair<syntax::ExprId, RequestedUse>> elements;
        for (const syntax::ExprId element : expr.array_elements) {
            elements.emplace_back(element, RequestedUse::owned);
        }
        if (syntax::is_valid(expr.array_repeat_value)) {
            elements.emplace_back(expr.array_repeat_value, RequestedUse::owned);
        }
        if (syntax::is_valid(expr.array_repeat_count)) {
            elements.emplace_back(expr.array_repeat_count, RequestedUse::owned);
        }
        this->push_expression_sequence(elements, task.start, task.continuation, task.environment);
    }

    void build_field_expression(const BuildTask& task, const SemanticAnalyzerCore::ExprView& expr)
    {
        if (task.requested == RequestedUse::owned
            && this->is_tracked_resource_type(this->core_.cached_expr_type(task.expr))) {
            this->blocks_[task.start].actions.push_back(MoveAction{MoveActionKind::reject_partial_field_move,
                SEMA_MOVE_INVALID_LOCAL, task.expr, OwnedUseMode::none, expr.range});
        }
        this->push_expression(
            expr.object, RequestedUse::initialized_place, task.start, task.continuation, task.environment);
    }

    void build_index_expression(const BuildTask& task, const SemanticAnalyzerCore::ExprView& expr)
    {
        if (task.requested == RequestedUse::owned
            && this->is_tracked_resource_type(this->core_.cached_expr_type(task.expr))) {
            this->blocks_[task.start].actions.push_back(MoveAction{MoveActionKind::reject_indexed_move,
                SEMA_MOVE_INVALID_LOCAL, task.expr, OwnedUseMode::none, expr.range});
        }
        this->push_expression_sequence(
            {
                {expr.object, RequestedUse::initialized_place},
                {expr.index, RequestedUse::owned},
            },
            task.start, task.continuation, task.environment);
    }

    void build_slice_expression(const BuildTask& task, const SemanticAnalyzerCore::ExprView& expr)
    {
        std::vector<std::pair<syntax::ExprId, RequestedUse>> operands{
            {expr.object, RequestedUse::initialized_place},
        };
        if (syntax::is_valid(expr.slice_start)) {
            operands.emplace_back(expr.slice_start, RequestedUse::owned);
        }
        if (syntax::is_valid(expr.slice_end)) {
            operands.emplace_back(expr.slice_end, RequestedUse::owned);
        }
        this->push_expression_sequence(operands, task.start, task.continuation, task.environment);
    }

    [[nodiscard]] RequestedUse unary_operand_use(const syntax::UnaryOp op) const noexcept
    {
        if (op == syntax::UnaryOp::address_of) {
            return RequestedUse::shared_borrow;
        }
        if (op == syntax::UnaryOp::address_of_mut) {
            return RequestedUse::mutable_borrow;
        }
        return RequestedUse::owned;
    }

    [[nodiscard]] base::usize declared_local(const syntax::StmtId stmt) const noexcept
    {
        const auto found = this->declared_locals_.find(stmt.value);
        return found == this->declared_locals_.end() ? SEMA_MOVE_INVALID_LOCAL : found->second;
    }

    [[nodiscard]] std::optional<base::usize> whole_local(
        const syntax::ExprId expr, const MoveEnvironment& environment) const
    {
        if (!syntax::is_valid(expr) || expr.value >= this->core_.ctx_.module.exprs.size()
            || this->core_.ctx_.module.exprs.kind(expr.value) != syntax::ExprKind::name) {
            return std::nullopt;
        }
        const syntax::NameExprPayload* const name = this->core_.ctx_.module.exprs.name_payload(expr.value);
        if (name == nullptr || !name->scope_name.empty()) {
            return std::nullopt;
        }
        const auto found = environment.find(name->text_id);
        return found == environment.end() || found->second == SEMA_MOVE_INVALID_LOCAL
            ? std::nullopt
            : std::optional<base::usize>{found->second};
    }

    [[nodiscard]] bool pattern_has_bindings(const syntax::PatternId root) const
    {
        std::vector<syntax::PatternId> pending{root};
        while (!pending.empty()) {
            const syntax::PatternId pattern_id = pending.back();
            pending.pop_back();
            if (!syntax::is_valid(pattern_id) || pattern_id.value >= this->core_.ctx_.module.patterns.size()) {
                continue;
            }
            const syntax::PatternNode* const pattern = this->core_.ctx_.module.patterns.ptr(pattern_id.value);
            if (pattern == nullptr) {
                continue;
            }
            if (pattern->kind == syntax::PatternKind::binding) {
                return true;
            }
            pending.insert(pending.end(), pattern->payload_patterns.begin(), pattern->payload_patterns.end());
            pending.insert(pending.end(), pattern->elements.begin(), pattern->elements.end());
            pending.insert(pending.end(), pattern->alternatives.begin(), pattern->alternatives.end());
            for (const syntax::FieldPattern& field : pattern->field_patterns) {
                pending.push_back(field.pattern);
            }
        }
        return false;
    }

    [[nodiscard]] MoveState initial_state() const
    {
        MoveState state;
        state.definitely_initialized.assign(this->locals_.size(), false);
        state.maybe_initialized.assign(this->locals_.size(), false);
        state.consume_origins.resize(this->locals_.size());
        for (base::usize i = 0; i < this->parameter_locals_.size(); ++i) {
            if (this->parameter_locals_[i]) {
                state.definitely_initialized[i] = true;
                state.maybe_initialized[i] = true;
            }
        }
        return state;
    }

    [[nodiscard]] MoveState join_predecessors(const base::usize block) const
    {
        if (block == SEMA_MOVE_ENTRY_BLOCK) {
            return this->initial_state();
        }
        const std::vector<base::usize>& predecessors = this->blocks_[block].predecessors;
        std::optional<MoveState> state;
        for (const base::usize predecessor : predecessors) {
            if (!this->processed_[predecessor]) {
                continue;
            }
            if (!state.has_value()) {
                state = this->out_states_[predecessor];
                continue;
            }
            const MoveState& incoming = this->out_states_[predecessor];
            for (base::usize local = 0; local < state->definitely_initialized.size(); ++local) {
                state->definitely_initialized[local] =
                    state->definitely_initialized[local] && incoming.definitely_initialized[local];
                state->maybe_initialized[local] = state->maybe_initialized[local] || incoming.maybe_initialized[local];
                if (!state->consume_origins[local].present && incoming.consume_origins[local].present) {
                    state->consume_origins[local] = incoming.consume_origins[local];
                }
            }
        }
        return state.value_or(this->initial_state());
    }

    void apply_action(MoveState& state, const MoveAction& action, const bool report) const
    {
        if (action.kind == MoveActionKind::initialize_local) {
            if (action.local < state.definitely_initialized.size()) {
                state.definitely_initialized[action.local] = true;
                state.maybe_initialized[action.local] = true;
                state.consume_origins[action.local] = {};
            }
            return;
        }
        if (action.kind != MoveActionKind::use_local) {
            if (report) {
                this->report_rejection(action);
            }
            return;
        }
        if (action.local >= state.definitely_initialized.size()) {
            return;
        }
        if (action.requires_initialized && !state.maybe_initialized[action.local]) {
            if (report) {
                this->report_moved_use(action, state.consume_origins[action.local], false);
            }
            return;
        }
        if (action.requires_initialized && !state.definitely_initialized[action.local]) {
            if (report) {
                this->report_moved_use(action, state.consume_origins[action.local], true);
            }
            return;
        }
        if (action.mode == OwnedUseMode::owned_consume) {
            state.definitely_initialized[action.local] = false;
            state.maybe_initialized[action.local] = false;
            state.consume_origins[action.local] = ConsumeOrigin{action.range, true};
        }
    }

    void solve_dataflow()
    {
        const MoveState empty = this->initial_state();
        this->in_states_.assign(this->blocks_.size(), empty);
        this->out_states_.assign(this->blocks_.size(), empty);
        this->reachable_.assign(this->blocks_.size(), false);
        this->processed_.assign(this->blocks_.size(), false);
        std::deque<base::usize> worklist;
        worklist.push_back(SEMA_MOVE_ENTRY_BLOCK);
        this->reachable_[SEMA_MOVE_ENTRY_BLOCK] = true;
        while (!worklist.empty()) {
            const base::usize block = worklist.front();
            worklist.pop_front();
            MoveState input = this->join_predecessors(block);
            MoveState output = input;
            for (const MoveAction& action : this->blocks_[block].actions) {
                this->apply_action(output, action, false);
            }
            const bool changed = !this->processed_[block] || !same_state(this->in_states_[block], input)
                || !same_state(this->out_states_[block], output);
            this->in_states_[block] = std::move(input);
            this->out_states_[block] = std::move(output);
            this->processed_[block] = true;
            if (!changed) {
                continue;
            }
            for (const base::usize successor : this->blocks_[block].successors) {
                this->reachable_[successor] = true;
                worklist.push_back(successor);
            }
        }
    }

    void emit_diagnostics() const
    {
        for (base::usize block = 0; block < this->blocks_.size(); ++block) {
            if (!this->reachable_[block]) {
                continue;
            }
            MoveState state = this->in_states_[block];
            for (const MoveAction& action : this->blocks_[block].actions) {
                this->apply_action(state, action, true);
            }
        }
    }

    void report_moved_use(const MoveAction& action, const ConsumeOrigin origin, const bool possibly_moved) const
    {
        const std::string_view name =
            action.local < this->locals_.size() ? this->locals_[action.local].name : std::string_view{};
        this->core_.report_general(action.range,
            possibly_moved ? sema_use_of_possibly_moved_value_message(name) : sema_use_of_moved_value_message(name));
        if (origin.present) {
            this->core_.report_note(
                origin.range, SemanticDiagnosticKind::general, sema_value_consumed_here_message(name));
        }
    }

    void report_rejection(const MoveAction& action) const
    {
        switch (action.kind) {
            case MoveActionKind::reject_partial_field_move:
                this->core_.report_unsupported(action.range, std::string(SEMA_MOVE_PARTIAL_FIELD_UNSUPPORTED));
                break;
            case MoveActionKind::reject_indexed_move:
                this->core_.report_unsupported(action.range, std::string(SEMA_MOVE_INDEXED_ELEMENT_UNSUPPORTED));
                break;
            case MoveActionKind::reject_pattern_payload:
                this->core_.report_unsupported(action.range, std::string(SEMA_MOVE_PATTERN_PAYLOAD_UNSUPPORTED));
                break;
            case MoveActionKind::reject_try_payload:
                this->core_.report_unsupported(action.range, std::string(SEMA_MOVE_TRY_PAYLOAD_UNSUPPORTED));
                break;
            case MoveActionKind::use_local:
            case MoveActionKind::initialize_local:
                break;
        }
    }

    SemanticAnalyzerCore& core_;
    const syntax::ItemNode& function_;
    const FunctionSignature& signature_;
    ResourceSemanticsClassifier resources_;
    std::unordered_map<base::u32, ResourceSemanticsSummary> resource_cache_;
    std::unordered_map<base::u32, base::usize> declared_locals_;
    std::vector<MoveLocal> locals_;
    std::vector<bool> parameter_locals_;
    std::vector<MoveBlock> blocks_;
    std::vector<BuildTask> tasks_;
    std::vector<MoveState> in_states_;
    std::vector<MoveState> out_states_;
    std::vector<bool> reachable_;
    std::vector<bool> processed_;
};

SemanticAnalyzerCore::BodyMoveAnalyzer::BodyMoveAnalyzer(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

void SemanticAnalyzerCore::BodyMoveAnalyzer::analyze(
    const syntax::ItemNode& function, const FunctionSignature& signature)
{
    BodyMoveAnalysis(this->core_, function, signature).run();
}

void SemanticAnalyzerCore::analyze_body_moves(const syntax::ItemNode& function, const FunctionSignature& signature)
{
    BodyMoveAnalyzer(*this).analyze(function, signature);
}

} // namespace aurex::sema
