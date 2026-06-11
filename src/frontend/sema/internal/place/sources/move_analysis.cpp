#include <aurex/frontend/sema/resource_semantics.hpp>
#include <aurex/frontend/sema/sema_messages.hpp>

#include <algorithm>
#include <charconv>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <frontend/sema/internal/place/private/move_analysis.hpp>
#include <frontend/sema/internal/core/private/sema_array_repeat_semantics.hpp>

namespace aurex::sema {
namespace {

constexpr base::usize SEMA_MOVE_INVALID_LOCAL = static_cast<base::usize>(-1);
constexpr base::usize SEMA_MOVE_ENTRY_BLOCK = 0;
constexpr base::usize SEMA_MOVE_EXIT_BLOCK = 1;
constexpr base::usize SEMA_MOVE_INITIAL_BLOCK_CAPACITY = 16;
constexpr base::usize SEMA_MOVE_INITIAL_TASK_CAPACITY = 32;
constexpr int SEMA_MOVE_TUPLE_FIELD_DECIMAL_BASE = 10;

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
    syntax::StmtId stmt = syntax::INVALID_STMT_ID;
    syntax::PatternId pattern = syntax::INVALID_PATTERN_ID;
    TypeHandle tracked_type = INVALID_TYPE_HANDLE;
    query::StableFingerprint128 resource_fingerprint;
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
using BorrowEnvironment = std::unordered_map<IdentId, base::usize, IdentIdHash>;

struct DeferredExpression {
    syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    MoveEnvironment environment;
    BorrowEnvironment borrow_environment;
};

using CleanupStack = std::vector<std::vector<DeferredExpression>>;

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
    base::usize cleanup_keep_depth = 0;
    base::usize break_cleanup_depth = 0;
    base::usize continue_cleanup_depth = 0;
    RequestedUse requested = RequestedUse::owned;
    RequestedUse completion_requested = RequestedUse::owned;
    CleanupStack cleanup_scopes;
    MoveEnvironment environment;
    MoveEnvironment completion_environment;
    BorrowEnvironment borrow_environment;
    BorrowEnvironment completion_borrow_environment;
    syntax::ExprId completion_expr = syntax::INVALID_EXPR_ID;
    bool emit_cleanup_on_completion = true;
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

[[nodiscard]] std::optional<base::u32> parse_move_tuple_field_index(const std::string_view field_name) noexcept
{
    if (field_name.empty()) {
        return std::nullopt;
    }
    base::u32 value = 0;
    const char* const begin = field_name.data();
    const char* const end = begin + field_name.size();
    const auto result = std::from_chars(begin, end, value, SEMA_MOVE_TUPLE_FIELD_DECIMAL_BASE);
    if (result.ec != std::errc{} || result.ptr != end) {
        return std::nullopt;
    }
    return value;
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
        this->reset_move_rejection_facts();
        MoveEnvironment environment;
        BorrowEnvironment borrow_environment;
        environment.reserve(this->function_.params.size());
        borrow_environment.reserve(this->function_.params.size());
        for (base::usize i = 0; i < this->function_.params.size(); ++i) {
            const syntax::ParamDecl& param = this->function_.params[i];
            const TypeHandle type =
                i < this->signature_.param_types.size() ? this->signature_.param_types[i] : INVALID_TYPE_HANDLE;
            const base::usize local = this->add_tracked_local(param.name, type, param.range, true);
            this->bind_environment(environment, param.name_id, local);
            this->bind_borrow_environment(borrow_environment, param.name_id, SEMA_MOVE_INVALID_LOCAL);
        }

        if (this->try_record_modes_without_move_state()) {
            return;
        }

        this->initialize_cfg_storage();
        this->push_scoped_statement_list(this->function_.body, SEMA_MOVE_ENTRY_BLOCK, SEMA_MOVE_EXIT_BLOCK, environment,
            borrow_environment, CleanupStack{});
        this->build_cfg();
        this->solve_dataflow();
        this->emit_diagnostics();
        this->commit_move_rejection_facts();
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

    void reset_move_rejection_facts()
    {
        this->move_rejections_ = {};
        this->move_rejections_.function = this->signature_.semantic_key;
        this->move_rejections_.part_index = this->signature_.part_index;
        if (is_valid(this->signature_.semantic_key)) {
            this->core_.state_.checked.move_rejection_facts.erase(this->signature_.semantic_key);
        }
    }

    void commit_move_rejection_facts()
    {
        if (!is_valid(this->move_rejections_.function) || this->move_rejections_.rejections.empty()) {
            return;
        }
        this->move_rejections_.fingerprint = function_move_rejection_facts_fingerprint(this->move_rejections_);
        this->core_.state_.checked.move_rejection_facts[this->move_rejections_.function] = this->move_rejections_;
    }

    [[nodiscard]] bool is_tracked_resource_type(const TypeHandle type)
    {
        return is_valid(type) && this->core_.is_valid_storage_type(type)
            && !resource_is_copy(this->resource_summary(type));
    }

    [[nodiscard]] bool type_can_contain_borrow(const TypeHandle type) const
    {
        if (!is_valid(type)) {
            return false;
        }
        std::vector<TypeHandle> pending{type};
        std::unordered_set<base::u32> visited;
        while (!pending.empty()) {
            const TypeHandle current = pending.back();
            pending.pop_back();
            if (!is_valid(current) || current.value >= this->core_.state_.checked.types.size()
                || !visited.insert(current.value).second) {
                continue;
            }
            const TypeInfo& info = this->core_.state_.checked.types.get(current);
            switch (info.kind) {
                case TypeKind::builtin:
                    if (info.builtin == BuiltinType::str) {
                        return true;
                    }
                    break;
                case TypeKind::reference:
                case TypeKind::slice:
                    return true;
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
                    if (const auto* const cases = this->core_.find_enum_cases_by_type(current); cases != nullptr) {
                        for (const EnumCaseInfo* const enum_case : *cases) {
                            if (enum_case != nullptr) {
                                pending.insert(
                                    pending.end(), enum_case->payload_types.begin(), enum_case->payload_types.end());
                            }
                        }
                    }
                    break;
                }
                case TypeKind::generic_param:
                case TypeKind::associated_projection:
                case TypeKind::pointer:
                case TypeKind::function:
                case TypeKind::opaque_struct:
                case TypeKind::trait_object:
                    break;
            }
        }
        return false;
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

    void bind_borrow_environment(BorrowEnvironment& environment, const IdentId name, const base::usize local) const
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

    [[nodiscard]] base::usize declare_plain_local(const syntax::StmtId stmt_id, const syntax::StmtNode& stmt,
        MoveEnvironment& environment, BorrowEnvironment& borrow_environment)
    {
        if (stmt.kind != syntax::StmtKind::let && stmt.kind != syntax::StmtKind::var) {
            return SEMA_MOVE_INVALID_LOCAL;
        }
        if (syntax::is_valid(stmt.pattern) || !is_valid(stmt.name_id)) {
            return SEMA_MOVE_INVALID_LOCAL;
        }
        const auto existing = this->declared_locals_.find(stmt_id.value);
        const TypeHandle local_type = this->core_.cached_stmt_local_type(stmt_id);
        const base::usize borrowed_origin = this->type_can_contain_borrow(local_type)
            ? this->borrow_origin_local(stmt.init, environment, borrow_environment)
            : SEMA_MOVE_INVALID_LOCAL;
        if (existing != this->declared_locals_.end()) {
            this->bind_environment(environment, stmt.name_id, existing->second);
            this->bind_borrow_environment(borrow_environment, stmt.name_id, borrowed_origin);
            return existing->second;
        }
        const base::usize local = this->add_tracked_local(stmt.name, local_type, stmt.range, false);
        this->declared_locals_.emplace(stmt_id.value, local);
        this->bind_environment(environment, stmt.name_id, local);
        this->bind_borrow_environment(borrow_environment, stmt.name_id, borrowed_origin);
        return local;
    }

    struct PatternBorrowFrame {
        syntax::PatternId pattern = syntax::INVALID_PATTERN_ID;
        TypeHandle type = INVALID_TYPE_HANDLE;
        syntax::ExprId source = syntax::INVALID_EXPR_ID;
    };

    void declare_pattern_borrow_aliases(const syntax::StmtId stmt_id, const syntax::StmtNode& stmt,
        const MoveEnvironment& environment, BorrowEnvironment& borrow_environment)
    {
        if ((stmt.kind != syntax::StmtKind::let && stmt.kind != syntax::StmtKind::var)
            || !syntax::is_valid(stmt.pattern)) {
            return;
        }
        std::vector<PatternBorrowFrame> pending{{stmt.pattern, this->core_.cached_stmt_local_type(stmt_id), stmt.init}};
        while (!pending.empty()) {
            const PatternBorrowFrame frame = pending.back();
            pending.pop_back();
            if (!syntax::is_valid(frame.pattern) || frame.pattern.value >= this->core_.ctx_.module.patterns.size()) {
                continue;
            }
            const syntax::PatternNode* const pattern = this->core_.ctx_.module.patterns.ptr(frame.pattern.value);
            if (pattern == nullptr) {
                continue;
            }
            switch (pattern->kind) {
                case syntax::PatternKind::binding:
                    this->bind_pattern_borrow_alias(
                        *pattern, frame.type, frame.source, environment, borrow_environment);
                    break;
                case syntax::PatternKind::tuple:
                    this->push_tuple_pattern_borrow_frames(pending, *pattern, frame.type, frame.source);
                    break;
                case syntax::PatternKind::slice:
                    this->push_slice_pattern_borrow_frames(pending, *pattern, frame.type, frame.source);
                    break;
                case syntax::PatternKind::struct_:
                    this->push_struct_pattern_borrow_frames(pending, *pattern, frame.type, frame.source);
                    break;
                case syntax::PatternKind::enum_case:
                    this->push_enum_pattern_borrow_frames(pending, *pattern, frame.type, frame.source);
                    break;
                case syntax::PatternKind::or_pattern:
                    for (const syntax::PatternId alternative : pattern->alternatives) {
                        pending.push_back(PatternBorrowFrame{alternative, frame.type, frame.source});
                    }
                    break;
                case syntax::PatternKind::wildcard:
                case syntax::PatternKind::literal:
                case syntax::PatternKind::const_:
                    break;
            }
        }
    }

    void bind_pattern_borrow_alias(const syntax::PatternNode& pattern, const TypeHandle type,
        const syntax::ExprId source, const MoveEnvironment& environment, BorrowEnvironment& borrow_environment)
    {
        if (!this->type_can_contain_borrow(type)) {
            this->bind_borrow_environment(borrow_environment, pattern.binding_name_id, SEMA_MOVE_INVALID_LOCAL);
            return;
        }
        const base::usize origin = this->borrow_origin_local(source, environment, borrow_environment);
        this->bind_borrow_environment(borrow_environment, pattern.binding_name_id, origin);
    }

    void push_tuple_pattern_borrow_frames(std::vector<PatternBorrowFrame>& pending, const syntax::PatternNode& pattern,
        const TypeHandle type, const syntax::ExprId source) const
    {
        const syntax::AstArenaVector<syntax::ExprId>* const tuple_source =
            syntax::is_valid(source) && source.value < this->core_.ctx_.module.exprs.size()
            ? this->core_.ctx_.module.exprs.tuple_elements(source.value)
            : nullptr;
        const TypeInfo* const tuple_type =
            this->core_.state_.checked.types.is_tuple(type) ? &this->core_.state_.checked.types.get(type) : nullptr;
        for (base::usize index = pattern.elements.size(); index > 0; --index) {
            const base::usize element_index = index - 1;
            pending.push_back(PatternBorrowFrame{
                pattern.elements[element_index],
                tuple_type != nullptr && element_index < tuple_type->tuple_elements.size()
                    ? tuple_type->tuple_elements[element_index]
                    : INVALID_TYPE_HANDLE,
                tuple_source != nullptr && element_index < tuple_source->size() ? (*tuple_source)[element_index]
                                                                                : source,
            });
        }
    }

    void push_slice_pattern_borrow_frames(std::vector<PatternBorrowFrame>& pending, const syntax::PatternNode& pattern,
        const TypeHandle type, const syntax::ExprId source) const
    {
        const syntax::ArrayExprPayload* const array_source =
            syntax::is_valid(source) && source.value < this->core_.ctx_.module.exprs.size()
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
            pending.push_back(PatternBorrowFrame{
                pattern.elements[element_index],
                element_type,
                array_source != nullptr && element_index < array_source->elements.size()
                    ? array_source->elements[element_index]
                    : source,
            });
        }
    }

    void push_struct_pattern_borrow_frames(std::vector<PatternBorrowFrame>& pending, const syntax::PatternNode& pattern,
        const TypeHandle type, const syntax::ExprId source) const
    {
        const StructInfo* const structure = this->core_.find_struct(type);
        const syntax::StructLiteralExprPayload* const struct_source =
            syntax::is_valid(source) && source.value < this->core_.ctx_.module.exprs.size()
            ? this->core_.ctx_.module.exprs.struct_literal_payload(source.value)
            : nullptr;
        for (const syntax::FieldPattern& field : pattern.field_patterns) {
            const StructFieldInfo* const field_info =
                structure == nullptr ? nullptr : this->pattern_struct_field(*structure, field.name_id);
            pending.push_back(PatternBorrowFrame{
                field.pattern,
                field_info == nullptr ? INVALID_TYPE_HANDLE : field_info->type,
                this->struct_field_source(struct_source, field.name_id, source),
            });
        }
    }

    void push_enum_pattern_borrow_frames(std::vector<PatternBorrowFrame>& pending, const syntax::PatternNode& pattern,
        const TypeHandle type, const syntax::ExprId source) const
    {
        const EnumCaseInfo* const enum_case =
            this->core_.find_enum_case_by_type_and_case(type, pattern.case_name_id, pattern.case_name);
        for (base::usize index = pattern.payload_patterns.size(); index > 0; --index) {
            const base::usize payload_index = index - 1;
            pending.push_back(PatternBorrowFrame{
                pattern.payload_patterns[payload_index],
                enum_case == nullptr || payload_index >= enum_case->payload_types.size()
                    ? INVALID_TYPE_HANDLE
                    : enum_case->payload_types[payload_index],
                source,
            });
        }
    }

    [[nodiscard]] syntax::ExprId struct_field_source(const syntax::StructLiteralExprPayload* const source,
        const IdentId field_name, const syntax::ExprId fallback) const noexcept
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

    [[nodiscard]] const StructFieldInfo* pattern_struct_field(
        const StructInfo& structure, const IdentId field_name) const noexcept
    {
        for (const StructFieldInfo& field : structure.fields) {
            if (field.name_id == field_name) {
                return &field;
            }
        }
        return nullptr;
    }

    [[nodiscard]] MoveEnvironment block_result_environment(
        const syntax::StmtId block, MoveEnvironment environment, BorrowEnvironment& borrow_environment)
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
            const syntax::StmtNode& stmt = this->core_.ctx_.module.stmts[stmt_id.value];
            static_cast<void>(this->declare_plain_local(stmt_id, stmt, environment, borrow_environment));
            this->declare_pattern_borrow_aliases(stmt_id, stmt, environment, borrow_environment);
        }
        return environment;
    }

    [[nodiscard]] base::usize block_result_borrow_origin(
        const syntax::BlockExprPayload& block, MoveEnvironment environment, BorrowEnvironment borrow_environment)
    {
        if (!syntax::is_valid(block.block) || block.block.value >= this->core_.ctx_.module.stmts.size()) {
            return this->borrow_origin_local(block.result, environment, borrow_environment);
        }
        const syntax::AstArenaVector<syntax::StmtId>* const statements =
            this->core_.ctx_.module.stmts.block_statements(block.block.value);
        if (statements == nullptr) {
            return this->borrow_origin_local(block.result, environment, borrow_environment);
        }
        for (const syntax::StmtId stmt_id : *statements) {
            if (!syntax::is_valid(stmt_id) || stmt_id.value >= this->core_.ctx_.module.stmts.size()) {
                continue;
            }
            const syntax::StmtNode& stmt = this->core_.ctx_.module.stmts[stmt_id.value];
            if (stmt.kind == syntax::StmtKind::assign) {
                const MoveEnvironment before_environment = environment;
                const BorrowEnvironment before_borrow_environment = borrow_environment;
                this->assign_borrow_alias_if_needed(
                    stmt, before_environment, before_borrow_environment, borrow_environment);
                continue;
            }
            static_cast<void>(this->declare_plain_local(stmt_id, stmt, environment, borrow_environment));
            this->declare_pattern_borrow_aliases(stmt_id, stmt, environment, borrow_environment);
        }
        return this->borrow_origin_local(block.result, environment, borrow_environment);
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

    void push_initialize_action(
        const base::usize block, const base::usize local, const syntax::ExprId expr, const base::SourceRange& range)
    {
        if (block >= this->blocks_.size()) {
            return;
        }
        MoveAction action;
        action.kind = MoveActionKind::initialize_local;
        action.local = local;
        action.expr = expr;
        action.range = range;
        this->blocks_[block].actions.push_back(action);
    }

    void push_use_action(const base::usize block, const base::usize local, const syntax::ExprId expr,
        const OwnedUseMode mode, const base::SourceRange& range, const bool requires_initialized)
    {
        if (block >= this->blocks_.size()) {
            return;
        }
        MoveAction action;
        action.kind = MoveActionKind::use_local;
        action.local = local;
        action.expr = expr;
        action.mode = mode;
        action.range = range;
        action.requires_initialized = requires_initialized;
        this->blocks_[block].actions.push_back(action);
    }

    void push_rejection_action(const base::usize block, const MoveActionKind kind, const syntax::ExprId expr,
        const syntax::StmtId stmt, const syntax::PatternId pattern, const TypeHandle tracked_type,
        const base::SourceRange& range)
    {
        if (block >= this->blocks_.size()) {
            return;
        }
        MoveAction action;
        action.kind = kind;
        action.expr = expr;
        action.stmt = stmt;
        action.pattern = pattern;
        action.tracked_type = tracked_type;
        action.resource_fingerprint = resource_semantics_fingerprint(this->resource_summary(tracked_type));
        action.range = range;
        action.requires_initialized = false;
        this->blocks_[block].actions.push_back(action);
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

    [[nodiscard]] bool field_name_matches(const StructFieldInfo& field, const SemanticAnalyzerCore::ExprView& expr) const
        noexcept
    {
        if (is_valid(expr.field_name_id) && field.name_id == expr.field_name_id) {
            return true;
        }
        return field.name == expr.field_name;
    }

    [[nodiscard]] bool field_move_can_consume_whole_object(const SemanticAnalyzerCore::ExprView& expr) const
    {
        const TypeHandle object_type = this->core_.cached_expr_type(expr.object);
        if (!is_valid(object_type) || object_type.value >= this->core_.state_.checked.types.size()) {
            return false;
        }
        const TypeInfo& object_info = this->core_.state_.checked.types.get(object_type);
        if (object_info.kind == TypeKind::tuple) {
            const std::optional<base::u32> field_index = parse_move_tuple_field_index(expr.field_name);
            return object_info.tuple_elements.size() == 1U && field_index.has_value() && *field_index == 0U;
        }
        if (object_info.kind == TypeKind::struct_) {
            const auto found = this->core_.state_.types.struct_infos_by_type.find(object_type.value);
            if (found == this->core_.state_.types.struct_infos_by_type.end() || found->second == nullptr) {
                return false;
            }
            const std::span<const StructFieldInfo> fields = found->second->fields;
            return fields.size() == 1 && this->field_name_matches(fields.front(), expr);
        }
        return false;
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

    void push_scoped_statement_list(const syntax::StmtId block, const base::usize start, const base::usize continuation,
        MoveEnvironment environment, BorrowEnvironment borrow_environment, CleanupStack cleanup_scopes,
        const base::usize break_target = SEMA_MOVE_EXIT_BLOCK, const base::usize continue_target = SEMA_MOVE_EXIT_BLOCK,
        const base::usize break_cleanup_depth = 0, const base::usize continue_cleanup_depth = 0,
        const bool emit_cleanup_on_completion = true)
    {
        const base::usize cleanup_keep_depth = cleanup_scopes.size();
        cleanup_scopes.emplace_back();
        this->push_statement_list(block, start, continuation, std::move(environment), std::move(borrow_environment),
            std::move(cleanup_scopes), cleanup_keep_depth, break_target, continue_target, break_cleanup_depth,
            continue_cleanup_depth, 0, emit_cleanup_on_completion);
    }

    void push_statement_list(const syntax::StmtId block, const base::usize start, const base::usize continuation,
        MoveEnvironment environment, BorrowEnvironment borrow_environment, CleanupStack cleanup_scopes,
        const base::usize cleanup_keep_depth, const base::usize break_target = SEMA_MOVE_EXIT_BLOCK,
        const base::usize continue_target = SEMA_MOVE_EXIT_BLOCK, const base::usize break_cleanup_depth = 0,
        const base::usize continue_cleanup_depth = 0, const base::usize statement_index = 0,
        const bool emit_cleanup_on_completion = true, const syntax::ExprId completion_expr = syntax::INVALID_EXPR_ID,
        const RequestedUse completion_requested = RequestedUse::owned, MoveEnvironment completion_environment = {},
        BorrowEnvironment completion_borrow_environment = {})
    {
        BuildTask task;
        task.kind = BuildTaskKind::statement_list;
        task.stmt = block;
        task.statement_index = statement_index;
        task.start = start;
        task.continuation = continuation;
        task.break_target = break_target;
        task.continue_target = continue_target;
        task.cleanup_keep_depth = cleanup_keep_depth;
        task.break_cleanup_depth = break_cleanup_depth;
        task.continue_cleanup_depth = continue_cleanup_depth;
        task.completion_requested = completion_requested;
        task.cleanup_scopes = std::move(cleanup_scopes);
        task.environment = std::move(environment);
        task.borrow_environment = std::move(borrow_environment);
        task.completion_environment = std::move(completion_environment);
        task.completion_borrow_environment = std::move(completion_borrow_environment);
        task.completion_expr = completion_expr;
        task.emit_cleanup_on_completion = emit_cleanup_on_completion;
        this->tasks_.push_back(std::move(task));
    }

    void push_statement(const syntax::StmtId stmt, const base::usize start, const base::usize continuation,
        MoveEnvironment environment, BorrowEnvironment borrow_environment, CleanupStack cleanup_scopes,
        const base::usize break_target = SEMA_MOVE_EXIT_BLOCK, const base::usize continue_target = SEMA_MOVE_EXIT_BLOCK,
        const base::usize break_cleanup_depth = 0, const base::usize continue_cleanup_depth = 0)
    {
        BuildTask task;
        task.kind = BuildTaskKind::statement;
        task.stmt = stmt;
        task.start = start;
        task.continuation = continuation;
        task.break_target = break_target;
        task.continue_target = continue_target;
        task.break_cleanup_depth = break_cleanup_depth;
        task.continue_cleanup_depth = continue_cleanup_depth;
        task.cleanup_scopes = std::move(cleanup_scopes);
        task.environment = std::move(environment);
        task.borrow_environment = std::move(borrow_environment);
        this->tasks_.push_back(std::move(task));
    }

    void push_expression(const syntax::ExprId expr, const RequestedUse requested, const base::usize start,
        const base::usize continuation, MoveEnvironment environment, BorrowEnvironment borrow_environment,
        CleanupStack cleanup_scopes, const base::usize break_target = SEMA_MOVE_EXIT_BLOCK,
        const base::usize continue_target = SEMA_MOVE_EXIT_BLOCK, const base::usize break_cleanup_depth = 0,
        const base::usize continue_cleanup_depth = 0)
    {
        BuildTask task;
        task.kind = BuildTaskKind::expression;
        task.expr = expr;
        task.start = start;
        task.continuation = continuation;
        task.break_target = break_target;
        task.continue_target = continue_target;
        task.break_cleanup_depth = break_cleanup_depth;
        task.continue_cleanup_depth = continue_cleanup_depth;
        task.requested = requested;
        task.cleanup_scopes = std::move(cleanup_scopes);
        task.environment = std::move(environment);
        task.borrow_environment = std::move(borrow_environment);
        this->tasks_.push_back(std::move(task));
    }

    void push_expression_sequence(const std::vector<std::pair<syntax::ExprId, RequestedUse>>& expressions,
        const base::usize start, const base::usize continuation, const MoveEnvironment& environment,
        const BorrowEnvironment& borrow_environment, const CleanupStack& cleanup_scopes,
        const base::usize break_target = SEMA_MOVE_EXIT_BLOCK, const base::usize continue_target = SEMA_MOVE_EXIT_BLOCK,
        const base::usize break_cleanup_depth = 0, const base::usize continue_cleanup_depth = 0)
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
            this->push_expression(expressions[i - 1].first, expressions[i - 1].second, boundaries[i - 1], boundaries[i],
                environment, borrow_environment, cleanup_scopes, break_target, continue_target, break_cleanup_depth,
                continue_cleanup_depth);
        }
    }

    void register_deferred_expression(CleanupStack& cleanup_scopes, const syntax::ExprId expr,
        const MoveEnvironment& environment, const BorrowEnvironment& borrow_environment) const
    {
        if (cleanup_scopes.empty()) {
            return;
        }
        cleanup_scopes.back().push_back(DeferredExpression{expr, environment, borrow_environment});
    }

    void push_cleanup_scopes(const CleanupStack& cleanup_scopes, const base::usize keep_depth, const base::usize start,
        const base::usize continuation)
    {
        const base::usize clamped_keep_depth = std::min(keep_depth, cleanup_scopes.size());
        std::vector<DeferredExpression> deferred;
        for (base::usize depth = cleanup_scopes.size(); depth > clamped_keep_depth; --depth) {
            const std::vector<DeferredExpression>& scope = cleanup_scopes[depth - 1];
            for (base::usize i = scope.size(); i > 0; --i) {
                deferred.push_back(scope[i - 1]);
            }
        }
        if (deferred.empty()) {
            this->add_edge(start, continuation);
            return;
        }
        CleanupStack cleanup_context = cleanup_scopes;
        cleanup_context.resize(clamped_keep_depth);
        std::vector<base::usize> boundaries(deferred.size() + 1, continuation);
        boundaries.front() = start;
        for (base::usize i = 1; i < deferred.size(); ++i) {
            boundaries[i] = this->new_block();
        }
        for (base::usize i = deferred.size(); i > 0; --i) {
            const DeferredExpression& action = deferred[i - 1];
            this->push_expression(action.expr, RequestedUse::owned, boundaries[i - 1], boundaries[i],
                action.environment, action.borrow_environment, cleanup_context);
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
        const syntax::StmtNode& stmt = this->core_.ctx_.module.stmts[stmt_id.value];
        switch (stmt.kind) {
            case syntax::StmtKind::let:
            case syntax::StmtKind::var:
                this->push_mode_block(tasks, stmt.else_block);
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
                if (this->pattern_payload_requires_full_move_analysis(stmt.pattern, stmt.condition)) {
                    return false;
                }
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
                if (this->pattern_payload_requires_full_move_analysis(stmt.pattern, stmt.condition)) {
                    return false;
                }
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
                break;
            case syntax::StmtKind::defer:
                this->push_mode_expression(tasks, stmt.init, RequestedUse::owned);
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
        this->core_.record_expr_owned_use_mode(expr_id, this->effective_mode(expr_id, requested));
        this->push_mode_expression_children(tasks, expr_id, expr, requested);
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
            case syntax::ExprKind::if_expr:
                return this->pattern_payload_requires_full_move_analysis(expr.condition_pattern, expr.condition);
            case syntax::ExprKind::index:
                return requested == RequestedUse::owned
                    && this->is_tracked_resource_type(this->core_.cached_expr_type(expr_id));
            case syntax::ExprKind::field:
                return false;
            default:
                break;
        }
        return false;
    }

    [[nodiscard]] bool pattern_payload_requires_full_move_analysis(
        const syntax::PatternId pattern, const syntax::ExprId value)
    {
        return syntax::is_valid(pattern) && this->pattern_has_bindings(pattern)
            && this->is_tracked_resource_type(this->core_.cached_expr_type(value));
    }

    void reject_pattern_payload_if_needed(const syntax::PatternId pattern, const syntax::ExprId value,
        const base::usize block, const base::SourceRange& range, const syntax::StmtId stmt = syntax::INVALID_STMT_ID,
        const syntax::ExprId expr = syntax::INVALID_EXPR_ID)
    {
        if (block >= this->blocks_.size() || !this->pattern_payload_requires_full_move_analysis(pattern, value)) {
            return;
        }
        this->push_rejection_action(block, MoveActionKind::reject_pattern_payload, expr, stmt, pattern,
            this->core_.cached_expr_type(value), range);
    }

    void push_mode_expression_children(std::vector<ModeTask>& tasks, const syntax::ExprId expr_id,
        const SemanticAnalyzerCore::ExprView& expr, const RequestedUse requested)
    {
        switch (expr.kind) {
            case syntax::ExprKind::name:
            case syntax::ExprKind::lambda:
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
                if (array_repeat_value_should_be_visited(array_repeat_runtime_semantics(
                        this->core_.ctx_.module, this->core_.state_.checked.types,
                        this->core_.cached_expr_type(expr_id), expr_id))) {
                    this->push_mode_expression(tasks, expr.array_repeat_value, RequestedUse::owned);
                }
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

    void complete_statement_list(const BuildTask& task)
    {
        if (task.emit_cleanup_on_completion || !syntax::is_valid(task.completion_expr)) {
            this->push_cleanup_scopes(task.cleanup_scopes, task.cleanup_keep_depth, task.start, task.continuation);
            return;
        }
        const base::usize cleanup = this->new_block();
        this->push_expression(task.completion_expr, task.completion_requested, task.start, cleanup,
            task.completion_environment, task.completion_borrow_environment, task.cleanup_scopes, task.break_target,
            task.continue_target, task.break_cleanup_depth, task.continue_cleanup_depth);
        this->push_cleanup_scopes(task.cleanup_scopes, task.cleanup_keep_depth, cleanup, task.continuation);
    }

    void build_statement_list(BuildTask task)
    {
        if (!syntax::is_valid(task.stmt) || task.stmt.value >= this->core_.ctx_.module.stmts.size()) {
            this->complete_statement_list(task);
            return;
        }
        const syntax::AstArenaVector<syntax::StmtId>* const statements =
            this->core_.ctx_.module.stmts.block_statements(task.stmt.value);
        if (statements == nullptr || task.statement_index >= statements->size()) {
            this->complete_statement_list(task);
            return;
        }

        const syntax::StmtId stmt_id = (*statements)[task.statement_index];
        const base::usize next = this->new_block();
        MoveEnvironment next_environment = task.environment;
        BorrowEnvironment next_borrow_environment = task.borrow_environment;
        CleanupStack next_cleanup_scopes = task.cleanup_scopes;
        if (syntax::is_valid(stmt_id) && stmt_id.value < this->core_.ctx_.module.stmts.size()) {
            const syntax::StmtNode& stmt = this->core_.ctx_.module.stmts[stmt_id.value];
            static_cast<void>(this->declare_plain_local(stmt_id, stmt, next_environment, next_borrow_environment));
            this->declare_pattern_borrow_aliases(stmt_id, stmt, task.environment, next_borrow_environment);
            this->assign_borrow_alias_if_needed(
                stmt, task.environment, task.borrow_environment, next_borrow_environment);
            if (stmt.kind == syntax::StmtKind::defer) {
                this->register_deferred_expression(
                    next_cleanup_scopes, stmt.init, task.environment, task.borrow_environment);
            }
        }
        this->push_statement_list(task.stmt, next, task.continuation, std::move(next_environment),
            std::move(next_borrow_environment), std::move(next_cleanup_scopes), task.cleanup_keep_depth,
            task.break_target, task.continue_target, task.break_cleanup_depth, task.continue_cleanup_depth,
            task.statement_index + 1, task.emit_cleanup_on_completion, task.completion_expr, task.completion_requested,
            std::move(task.completion_environment), std::move(task.completion_borrow_environment));
        this->push_statement(stmt_id, task.start, next, std::move(task.environment), std::move(task.borrow_environment),
            std::move(task.cleanup_scopes), task.break_target, task.continue_target, task.break_cleanup_depth,
            task.continue_cleanup_depth);
    }

    void assign_borrow_alias_if_needed(const syntax::StmtNode& stmt, const MoveEnvironment& environment,
        const BorrowEnvironment& borrow_environment, BorrowEnvironment& next_borrow_environment)
    {
        if (stmt.kind != syntax::StmtKind::assign || stmt.assign_op != syntax::AssignOp::assign
            || !this->type_can_contain_borrow(this->core_.cached_expr_type(stmt.lhs)) || !syntax::is_valid(stmt.lhs)
            || stmt.lhs.value >= this->core_.ctx_.module.exprs.size()
            || this->core_.ctx_.module.exprs.kind(stmt.lhs.value) != syntax::ExprKind::name) {
            return;
        }
        const syntax::NameExprPayload* const name = this->core_.ctx_.module.exprs.name_payload(stmt.lhs.value);
        if (name == nullptr || !name->scope_name.empty()) {
            return;
        }
        this->bind_borrow_environment(next_borrow_environment, name->text_id,
            this->borrow_origin_local(stmt.rhs, environment, borrow_environment));
    }

    void build_statement(BuildTask task)
    {
        if (!syntax::is_valid(task.stmt) || task.stmt.value >= this->core_.ctx_.module.stmts.size()) {
            this->add_edge(task.start, task.continuation);
            return;
        }
        const syntax::StmtNode& stmt = this->core_.ctx_.module.stmts[task.stmt.value];
        switch (stmt.kind) {
            case syntax::StmtKind::let:
            case syntax::StmtKind::var:
                this->build_local_declaration(stmt, task);
                break;
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
                    const base::usize cleanup = this->new_block();
                    this->push_expression(stmt.return_value, RequestedUse::owned, task.start, cleanup, task.environment,
                        task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
                        task.break_cleanup_depth, task.continue_cleanup_depth);
                    this->push_cleanup_scopes(task.cleanup_scopes, 0, cleanup, SEMA_MOVE_EXIT_BLOCK);
                } else {
                    this->push_cleanup_scopes(task.cleanup_scopes, 0, task.start, SEMA_MOVE_EXIT_BLOCK);
                }
                break;
            case syntax::StmtKind::expr:
                this->push_expression(stmt.init, RequestedUse::owned, task.start, task.continuation, task.environment,
                    task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
                    task.break_cleanup_depth, task.continue_cleanup_depth);
                break;
            case syntax::StmtKind::block:
                this->push_scoped_statement_list(task.stmt, task.start, task.continuation, task.environment,
                    task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
                    task.break_cleanup_depth, task.continue_cleanup_depth);
                break;
            case syntax::StmtKind::break_:
                this->push_cleanup_scopes(task.cleanup_scopes, task.break_cleanup_depth, task.start, task.break_target);
                break;
            case syntax::StmtKind::continue_:
                this->push_cleanup_scopes(
                    task.cleanup_scopes, task.continue_cleanup_depth, task.start, task.continue_target);
                break;
            case syntax::StmtKind::defer:
                this->add_edge(task.start, task.continuation);
                break;
        }
    }

    void build_local_declaration(const syntax::StmtNode& stmt, const BuildTask& task)
    {
        const base::usize finish = this->new_block();
        const base::usize initialized = this->declared_local(task.stmt);
        if (initialized != SEMA_MOVE_INVALID_LOCAL) {
            this->push_initialize_action(finish, initialized, syntax::INVALID_EXPR_ID, stmt.range);
        } else if (syntax::is_valid(stmt.pattern)
            && this->is_tracked_resource_type(this->core_.cached_expr_type(stmt.init))) {
            this->push_rejection_action(finish, MoveActionKind::reject_pattern_payload, stmt.init, task.stmt,
                stmt.pattern, this->core_.cached_expr_type(stmt.init), stmt.range);
        }
        this->add_edge(finish, task.continuation);
        if (syntax::is_valid(stmt.pattern) && syntax::is_valid(stmt.else_block)) {
            const base::usize failure = this->new_block();
            this->add_edge(finish, failure);
            this->push_scoped_statement_list(stmt.else_block, failure, task.continuation, task.environment,
                task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
                task.break_cleanup_depth, task.continue_cleanup_depth);
        }
        this->push_expression(stmt.init, RequestedUse::owned, task.start, finish, task.environment,
            task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
            task.break_cleanup_depth, task.continue_cleanup_depth);
    }

    void build_assignment(const syntax::StmtNode& stmt, const BuildTask& task)
    {
        const base::usize finish = this->new_block();
        const std::optional<base::usize> whole_local = this->whole_local(stmt.lhs, task.environment);
        if (whole_local.has_value()) {
            this->push_initialize_action(finish, *whole_local, syntax::INVALID_EXPR_ID, stmt.range);
        }
        this->add_edge(finish, task.continuation);
        this->push_expression_sequence(
            {
                {stmt.lhs, stmt.assign_op == syntax::AssignOp::assign ? RequestedUse::place_only : RequestedUse::owned},
                {stmt.rhs, RequestedUse::owned},
            },
            task.start, finish, task.environment, task.borrow_environment, task.cleanup_scopes, task.break_target,
            task.continue_target, task.break_cleanup_depth, task.continue_cleanup_depth);
    }

    void build_if_statement(const syntax::StmtNode& stmt, const BuildTask& task)
    {
        const base::usize condition = this->new_block();
        const base::usize then_entry = this->new_block();
        const base::usize else_entry = this->new_block();
        this->reject_pattern_payload_if_needed(stmt.pattern, stmt.condition, task.start, stmt.range, task.stmt);
        this->push_expression(stmt.condition, RequestedUse::owned, task.start, condition, task.environment,
            task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
            task.break_cleanup_depth, task.continue_cleanup_depth);
        this->add_edge(condition, then_entry);
        this->add_edge(condition, else_entry);
        this->push_scoped_statement_list(stmt.then_block, then_entry, task.continuation, task.environment,
            task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
            task.break_cleanup_depth, task.continue_cleanup_depth);
        if (syntax::is_valid(stmt.else_if)) {
            this->push_statement(stmt.else_if, else_entry, task.continuation, task.environment, task.borrow_environment,
                task.cleanup_scopes, task.break_target, task.continue_target, task.break_cleanup_depth,
                task.continue_cleanup_depth);
        } else if (syntax::is_valid(stmt.else_block)) {
            this->push_scoped_statement_list(stmt.else_block, else_entry, task.continuation, task.environment,
                task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
                task.break_cleanup_depth, task.continue_cleanup_depth);
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
        this->reject_pattern_payload_if_needed(stmt.pattern, stmt.condition, condition, stmt.range, task.stmt);
        this->push_expression(stmt.condition, RequestedUse::owned, condition, decision, task.environment,
            task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
            task.break_cleanup_depth, task.continue_cleanup_depth);
        this->add_edge(decision, body);
        this->add_edge(decision, task.continuation);
        const base::usize loop_cleanup_depth = task.cleanup_scopes.size();
        this->push_scoped_statement_list(stmt.body, body, condition, task.environment, task.borrow_environment,
            task.cleanup_scopes, task.continuation, condition, loop_cleanup_depth, loop_cleanup_depth);
    }

    void build_for_statement(const syntax::StmtNode& stmt, const BuildTask& task)
    {
        MoveEnvironment loop_environment = task.environment;
        BorrowEnvironment loop_borrow_environment = task.borrow_environment;
        CleanupStack loop_cleanup_scopes = task.cleanup_scopes;
        const base::usize loop_keep_depth = loop_cleanup_scopes.size();
        loop_cleanup_scopes.emplace_back();
        if (syntax::is_valid(stmt.for_init) && stmt.for_init.value < this->core_.ctx_.module.stmts.size()) {
            const syntax::StmtNode init = this->core_.ctx_.module.stmts[stmt.for_init.value];
            static_cast<void>(
                this->declare_plain_local(stmt.for_init, init, loop_environment, loop_borrow_environment));
        }
        const base::usize condition = this->new_block();
        const base::usize decision = this->new_block();
        const base::usize body = this->new_block();
        const base::usize update = this->new_block();
        const base::usize loop_exit = this->new_block();
        const base::usize loop_cleanup_depth = loop_cleanup_scopes.size();
        if (syntax::is_valid(stmt.for_init)) {
            this->push_statement(stmt.for_init, task.start, condition, task.environment, task.borrow_environment,
                loop_cleanup_scopes, task.break_target, task.continue_target, task.break_cleanup_depth,
                task.continue_cleanup_depth);
        } else {
            this->add_edge(task.start, condition);
        }
        if (syntax::is_valid(stmt.condition)) {
            this->push_expression(stmt.condition, RequestedUse::owned, condition, decision, loop_environment,
                loop_borrow_environment, loop_cleanup_scopes, task.break_target, task.continue_target,
                task.break_cleanup_depth, task.continue_cleanup_depth);
        } else {
            this->add_edge(condition, decision);
        }
        this->add_edge(decision, body);
        this->add_edge(decision, loop_exit);
        this->push_cleanup_scopes(loop_cleanup_scopes, loop_keep_depth, loop_exit, task.continuation);
        this->push_scoped_statement_list(stmt.body, body, update, loop_environment, loop_borrow_environment,
            loop_cleanup_scopes, loop_exit, update, loop_cleanup_depth, loop_cleanup_depth);
        if (syntax::is_valid(stmt.for_update)) {
            this->push_statement(stmt.for_update, update, condition, loop_environment, loop_borrow_environment,
                loop_cleanup_scopes, loop_exit, update, loop_cleanup_depth, loop_cleanup_depth);
        } else {
            this->add_edge(update, condition);
        }
    }

    void build_for_range_statement(const syntax::StmtNode& stmt, const BuildTask& task)
    {
        CleanupStack loop_cleanup_scopes = task.cleanup_scopes;
        const base::usize loop_keep_depth = loop_cleanup_scopes.size();
        loop_cleanup_scopes.emplace_back();
        const base::usize loop_cleanup_depth = loop_cleanup_scopes.size();
        const base::usize header = this->new_block();
        const base::usize body = this->new_block();
        const base::usize loop_exit = this->new_block();
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
        this->push_expression_sequence(bounds, task.start, header, task.environment, task.borrow_environment,
            loop_cleanup_scopes, task.break_target, task.continue_target, task.break_cleanup_depth,
            task.continue_cleanup_depth);
        this->add_edge(header, body);
        this->add_edge(header, loop_exit);
        this->push_cleanup_scopes(loop_cleanup_scopes, loop_keep_depth, loop_exit, task.continuation);
        this->push_scoped_statement_list(stmt.body, body, header, task.environment, task.borrow_environment,
            loop_cleanup_scopes, loop_exit, header, loop_cleanup_depth, loop_cleanup_depth);
    }

    void build_expression(BuildTask task)
    {
        if (!syntax::is_valid(task.expr) || task.expr.value >= this->core_.ctx_.module.exprs.size()) {
            this->add_edge(task.start, task.continuation);
            return;
        }
        const SemanticAnalyzerCore::ExprView expr = this->core_.expr_view(task.expr);
        const OwnedUseMode mode = this->effective_mode(task.expr, task.requested);
        this->core_.record_expr_owned_use_mode(task.expr, mode);
        switch (expr.kind) {
            case syntax::ExprKind::name:
                this->build_name_expression(task, expr, mode);
                break;
            case syntax::ExprKind::generic_apply:
                this->push_expression(expr.callee, RequestedUse::place_only, task.start, task.continuation,
                    task.environment, task.borrow_environment, task.cleanup_scopes, task.break_target,
                    task.continue_target, task.break_cleanup_depth, task.continue_cleanup_depth);
                break;
            case syntax::ExprKind::unary:
                this->push_expression(expr.unary_operand, this->unary_operand_use(expr.unary_op), task.start,
                    task.continuation, task.environment, task.borrow_environment, task.cleanup_scopes,
                    task.break_target, task.continue_target, task.break_cleanup_depth, task.continue_cleanup_depth);
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
                this->push_expression_sequence(elements, task.start, task.continuation, task.environment,
                    task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
                    task.break_cleanup_depth, task.continue_cleanup_depth);
                break;
            }
            case syntax::ExprKind::struct_literal: {
                std::vector<std::pair<syntax::ExprId, RequestedUse>> fields;
                for (const syntax::FieldInit& field : expr.field_inits) {
                    fields.emplace_back(field.value, RequestedUse::owned);
                }
                this->push_expression_sequence(fields, task.start, task.continuation, task.environment,
                    task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
                    task.break_cleanup_depth, task.continue_cleanup_depth);
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
                this->push_expression(expr.cast_expr, RequestedUse::owned, task.start, task.continuation,
                    task.environment, task.borrow_environment, task.cleanup_scopes, task.break_target,
                    task.continue_target, task.break_cleanup_depth, task.continue_cleanup_depth);
                break;
            case syntax::ExprKind::invalid:
            case syntax::ExprKind::lambda:
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
                this->push_use_action(task.start, found->second, task.expr, mode, expr.range,
                    task.requested != RequestedUse::place_only);
            } else if (const auto borrowed = task.borrow_environment.find(expr.text_id);
                borrowed != task.borrow_environment.end() && borrowed->second != SEMA_MOVE_INVALID_LOCAL) {
                this->push_use_action(task.start, borrowed->second, task.expr, OwnedUseMode::shared_borrow, expr.range,
                    task.requested != RequestedUse::place_only);
            }
        }
        this->add_edge(task.start, task.continuation);
    }

    void build_binary_expression(const BuildTask& task, const SemanticAnalyzerCore::ExprView& expr)
    {
        if (expr.binary_op == syntax::BinaryOp::logical_and || expr.binary_op == syntax::BinaryOp::logical_or) {
            const base::usize decision = this->new_block();
            const base::usize rhs = this->new_block();
            this->push_expression(expr.binary_lhs, RequestedUse::owned, task.start, decision, task.environment,
                task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
                task.break_cleanup_depth, task.continue_cleanup_depth);
            this->add_edge(decision, rhs);
            this->add_edge(decision, task.continuation);
            this->push_expression(expr.binary_rhs, RequestedUse::owned, rhs, task.continuation, task.environment,
                task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
                task.break_cleanup_depth, task.continue_cleanup_depth);
            return;
        }
        this->push_expression_sequence(
            {
                {expr.binary_lhs, RequestedUse::owned},
                {expr.binary_rhs, RequestedUse::owned},
            },
            task.start, task.continuation, task.environment, task.borrow_environment, task.cleanup_scopes,
            task.break_target, task.continue_target, task.break_cleanup_depth, task.continue_cleanup_depth);
    }

    void build_call_expression(const BuildTask& task, const SemanticAnalyzerCore::ExprView& expr)
    {
        std::vector<std::pair<syntax::ExprId, RequestedUse>> operands;
        operands.emplace_back(expr.callee, RequestedUse::place_only);
        for (const syntax::ExprId arg : expr.args) {
            operands.emplace_back(arg, RequestedUse::owned);
        }
        this->push_expression_sequence(operands, task.start, task.continuation, task.environment,
            task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
            task.break_cleanup_depth, task.continue_cleanup_depth);
    }

    void build_try_expression(const BuildTask& task, const SemanticAnalyzerCore::ExprView& expr)
    {
        const TypeHandle operand_type = this->core_.cached_expr_type(expr.try_operand);
        if (this->is_tracked_resource_type(operand_type)) {
            this->push_rejection_action(task.start, MoveActionKind::reject_try_payload, task.expr,
                syntax::INVALID_STMT_ID, syntax::INVALID_PATTERN_ID, operand_type, expr.range);
        }
        const base::usize dispatch = this->new_block();
        const base::usize failure = this->new_block();
        this->push_expression(expr.try_operand, RequestedUse::owned, task.start, dispatch, task.environment,
            task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
            task.break_cleanup_depth, task.continue_cleanup_depth);
        this->add_edge(dispatch, task.continuation);
        this->add_edge(dispatch, failure);
        this->push_cleanup_scopes(task.cleanup_scopes, 0, failure, SEMA_MOVE_EXIT_BLOCK);
    }

    void build_if_expression(const BuildTask& task, const SemanticAnalyzerCore::ExprView& expr)
    {
        const base::usize decision = this->new_block();
        const base::usize then_entry = this->new_block();
        const base::usize else_entry = this->new_block();
        this->reject_pattern_payload_if_needed(
            expr.condition_pattern, expr.condition, task.start, expr.range, syntax::INVALID_STMT_ID, task.expr);
        this->push_expression(expr.condition, RequestedUse::owned, task.start, decision, task.environment,
            task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
            task.break_cleanup_depth, task.continue_cleanup_depth);
        this->add_edge(decision, then_entry);
        this->add_edge(decision, else_entry);
        this->push_expression(expr.then_expr, task.requested, then_entry, task.continuation, task.environment,
            task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
            task.break_cleanup_depth, task.continue_cleanup_depth);
        this->push_expression(expr.else_expr, task.requested, else_entry, task.continuation, task.environment,
            task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
            task.break_cleanup_depth, task.continue_cleanup_depth);
    }

    void build_block_expression(const BuildTask& task, const SemanticAnalyzerCore::ExprView& expr)
    {
        BorrowEnvironment result_borrow_environment = task.borrow_environment;
        const MoveEnvironment result_environment =
            this->block_result_environment(expr.block, task.environment, result_borrow_environment);
        CleanupStack block_cleanup_scopes = task.cleanup_scopes;
        const base::usize block_keep_depth = block_cleanup_scopes.size();
        block_cleanup_scopes.emplace_back();
        this->push_statement_list(expr.block, task.start, task.continuation, task.environment, task.borrow_environment,
            block_cleanup_scopes, block_keep_depth, task.break_target, task.continue_target, task.break_cleanup_depth,
            task.continue_cleanup_depth, 0, false, expr.block_result, task.requested, result_environment,
            result_borrow_environment);
    }

    void build_match_expression(const BuildTask& task, const SemanticAnalyzerCore::ExprView& expr)
    {
        const base::usize dispatch = this->new_block();
        const TypeHandle match_type = this->core_.cached_expr_type(expr.match_value);
        if (this->is_tracked_resource_type(match_type)) {
            for (const syntax::MatchArm& arm : expr.match_arms) {
                if (this->pattern_has_bindings(arm.pattern)) {
                    this->push_rejection_action(task.start, MoveActionKind::reject_pattern_payload, task.expr,
                        syntax::INVALID_STMT_ID, arm.pattern, match_type, arm.range);
                }
            }
        }
        this->push_expression(expr.match_value, RequestedUse::owned, task.start, dispatch, task.environment,
            task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
            task.break_cleanup_depth, task.continue_cleanup_depth);
        std::vector<base::usize> arm_entries;
        arm_entries.reserve(expr.match_arms.size());
        for (base::usize i = 0; i < expr.match_arms.size(); ++i) {
            arm_entries.push_back(this->new_block());
            this->add_edge(dispatch, arm_entries.back());
        }
        for (base::usize i = 0; i < expr.match_arms.size(); ++i) {
            const syntax::MatchArm& arm = expr.match_arms[i];
            const base::usize arm_entry = arm_entries[i];
            if (syntax::is_valid(arm.guard)) {
                const base::usize guard_decision = this->new_block();
                const base::usize value_entry = this->new_block();
                this->push_expression(arm.guard, RequestedUse::owned, arm_entry, guard_decision, task.environment,
                    task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
                    task.break_cleanup_depth, task.continue_cleanup_depth);
                this->add_edge(guard_decision, value_entry);
                if (i + 1 < arm_entries.size() && this->core_.pattern_is_irrefutable(arm.pattern, match_type)) {
                    this->add_edge(guard_decision, arm_entries[i + 1]);
                }
                this->push_expression(arm.value, task.requested, value_entry, task.continuation, task.environment,
                    task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
                    task.break_cleanup_depth, task.continue_cleanup_depth);
            } else {
                this->push_expression(arm.value, task.requested, arm_entry, task.continuation, task.environment,
                    task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
                    task.break_cleanup_depth, task.continue_cleanup_depth);
            }
        }
    }

    void build_array_expression(const BuildTask& task, const SemanticAnalyzerCore::ExprView& expr)
    {
        std::vector<std::pair<syntax::ExprId, RequestedUse>> elements;
        for (const syntax::ExprId element : expr.array_elements) {
            elements.emplace_back(element, RequestedUse::owned);
        }
        if (array_repeat_value_should_be_visited(array_repeat_runtime_semantics(
                this->core_.ctx_.module, this->core_.state_.checked.types, this->core_.cached_expr_type(task.expr),
                task.expr))) {
            elements.emplace_back(expr.array_repeat_value, RequestedUse::owned);
        }
        if (syntax::is_valid(expr.array_repeat_count)) {
            elements.emplace_back(expr.array_repeat_count, RequestedUse::owned);
        }
        this->push_expression_sequence(elements, task.start, task.continuation, task.environment,
            task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
            task.break_cleanup_depth, task.continue_cleanup_depth);
    }

    void build_field_expression(const BuildTask& task, const SemanticAnalyzerCore::ExprView& expr)
    {
        const bool moves_tracked_field = task.requested == RequestedUse::owned
            && this->is_tracked_resource_type(this->core_.cached_expr_type(task.expr));
        const bool consume_whole_object = moves_tracked_field && this->field_move_can_consume_whole_object(expr);
        const RequestedUse object_use = consume_whole_object ? RequestedUse::owned : RequestedUse::initialized_place;
        this->push_expression(expr.object, object_use, task.start, task.continuation,
            task.environment, task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
            task.break_cleanup_depth, task.continue_cleanup_depth);
    }

    void build_index_expression(const BuildTask& task, const SemanticAnalyzerCore::ExprView& expr)
    {
        const TypeHandle indexed_type = this->core_.cached_expr_type(task.expr);
        if (task.requested == RequestedUse::owned
            && this->is_tracked_resource_type(indexed_type)) {
            this->push_rejection_action(task.start, MoveActionKind::reject_indexed_move, task.expr,
                syntax::INVALID_STMT_ID, syntax::INVALID_PATTERN_ID, indexed_type, expr.range);
        }
        this->push_expression_sequence(
            {
                {expr.object, RequestedUse::initialized_place},
                {expr.index, RequestedUse::owned},
            },
            task.start, task.continuation, task.environment, task.borrow_environment, task.cleanup_scopes,
            task.break_target, task.continue_target, task.break_cleanup_depth, task.continue_cleanup_depth);
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
        this->push_expression_sequence(operands, task.start, task.continuation, task.environment,
            task.borrow_environment, task.cleanup_scopes, task.break_target, task.continue_target,
            task.break_cleanup_depth, task.continue_cleanup_depth);
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

    [[nodiscard]] base::usize borrowed_alias_local(
        const syntax::ExprId expr, const BorrowEnvironment& borrow_environment) const
    {
        if (!syntax::is_valid(expr) || expr.value >= this->core_.ctx_.module.exprs.size()
            || this->core_.ctx_.module.exprs.kind(expr.value) != syntax::ExprKind::name) {
            return SEMA_MOVE_INVALID_LOCAL;
        }
        const syntax::NameExprPayload* const name = this->core_.ctx_.module.exprs.name_payload(expr.value);
        if (name == nullptr || !name->scope_name.empty()) {
            return SEMA_MOVE_INVALID_LOCAL;
        }
        const auto found = borrow_environment.find(name->text_id);
        return found == borrow_environment.end() ? SEMA_MOVE_INVALID_LOCAL : found->second;
    }

    [[nodiscard]] base::usize borrow_origin_place(const syntax::ExprId expr, const MoveEnvironment& environment,
        const BorrowEnvironment& borrow_environment) const
    {
        syntax::ExprId current = expr;
        while (syntax::is_valid(current) && current.value < this->core_.ctx_.module.exprs.size()) {
            const syntax::ExprKind kind = this->core_.ctx_.module.exprs.kind(current.value);
            if (kind == syntax::ExprKind::name) {
                if (const base::usize alias = this->borrowed_alias_local(current, borrow_environment);
                    alias != SEMA_MOVE_INVALID_LOCAL) {
                    return alias;
                }
                const std::optional<base::usize> local = this->whole_local(current, environment);
                return local.value_or(SEMA_MOVE_INVALID_LOCAL);
            }
            if (const syntax::FieldExprPayload* const field =
                    this->core_.ctx_.module.exprs.field_payload(current.value);
                kind == syntax::ExprKind::field && field != nullptr) {
                current = field->object;
                continue;
            }
            if (const syntax::IndexExprPayload* const index =
                    this->core_.ctx_.module.exprs.index_payload(current.value);
                kind == syntax::ExprKind::index && index != nullptr) {
                current = index->object;
                continue;
            }
            if (const syntax::UnaryExprPayload* const unary =
                    this->core_.ctx_.module.exprs.unary_payload(current.value);
                kind == syntax::ExprKind::unary && unary != nullptr && unary->op == syntax::UnaryOp::dereference) {
                return this->borrowed_alias_local(unary->operand, borrow_environment);
            }
            break;
        }
        return SEMA_MOVE_INVALID_LOCAL;
    }

    [[nodiscard]] base::usize borrow_origin_local(
        const syntax::ExprId expr, const MoveEnvironment& environment, const BorrowEnvironment& borrow_environment)
    {
        std::vector<syntax::ExprId> pending{expr};
        std::unordered_set<base::u32> visited;
        while (!pending.empty()) {
            const syntax::ExprId current = pending.back();
            pending.pop_back();
            if (!syntax::is_valid(current) || current.value >= this->core_.ctx_.module.exprs.size()
                || !visited.insert(current.value).second) {
                continue;
            }
            const syntax::ExprKind kind = this->core_.ctx_.module.exprs.kind(current.value);
            if (kind == syntax::ExprKind::name) {
                if (const base::usize alias = this->borrowed_alias_local(current, borrow_environment);
                    alias != SEMA_MOVE_INVALID_LOCAL) {
                    return alias;
                }
                continue;
            }
            if (const syntax::CallExprPayload* const call = this->core_.ctx_.module.exprs.call_payload(current.value);
                kind == syntax::ExprKind::call && call != nullptr) {
                if (const base::usize receiver =
                        this->method_receiver_borrow_origin(call->callee, environment, borrow_environment);
                    receiver != SEMA_MOVE_INVALID_LOCAL) {
                    return receiver;
                }
                for (const syntax::ExprId arg : call->args) {
                    pending.push_back(arg);
                }
                continue;
            }
            if (const syntax::UnaryExprPayload* const unary =
                    this->core_.ctx_.module.exprs.unary_payload(current.value);
                kind == syntax::ExprKind::unary && unary != nullptr
                && (unary->op == syntax::UnaryOp::address_of || unary->op == syntax::UnaryOp::address_of_mut)) {
                return this->borrow_origin_place(unary->operand, environment, borrow_environment);
            }
            if (const syntax::SliceExprPayload* const slice =
                    this->core_.ctx_.module.exprs.slice_payload(current.value);
                kind == syntax::ExprKind::slice && slice != nullptr) {
                return this->borrow_origin_place(slice->object, environment, borrow_environment);
            }
            if (const syntax::CastExprPayload* const cast = this->core_.ctx_.module.exprs.cast_payload(current.value);
                cast != nullptr && kind == syntax::ExprKind::str_from_utf8_checked) {
                pending.push_back(cast->expr);
                continue;
            }
            if (const syntax::IfExprPayload* const if_expr = this->core_.ctx_.module.exprs.if_payload(current.value);
                kind == syntax::ExprKind::if_expr && if_expr != nullptr) {
                pending.push_back(if_expr->else_expr);
                pending.push_back(if_expr->then_expr);
                continue;
            }
            if (const syntax::MatchExprPayload* const match =
                    this->core_.ctx_.module.exprs.match_payload(current.value);
                kind == syntax::ExprKind::match_expr && match != nullptr) {
                for (const syntax::MatchArm& arm : match->arms) {
                    pending.push_back(arm.value);
                }
                continue;
            }
            if (const syntax::BlockExprPayload* const block =
                    this->core_.ctx_.module.exprs.block_payload(current.value);
                block != nullptr && (kind == syntax::ExprKind::block_expr || kind == syntax::ExprKind::unsafe_block)) {
                const base::usize origin = this->block_result_borrow_origin(*block, environment, borrow_environment);
                if (origin != SEMA_MOVE_INVALID_LOCAL) {
                    return origin;
                }
                continue;
            }
            if (const syntax::ArrayExprPayload* const array =
                    this->core_.ctx_.module.exprs.array_payload(current.value);
                kind == syntax::ExprKind::array_literal && array != nullptr) {
                if (array_repeat_value_should_be_visited(array_repeat_runtime_semantics(this->core_.ctx_.module,
                        this->core_.state_.checked.types, this->core_.cached_expr_type(current), current))) {
                    pending.push_back(array->repeat_value);
                }
                for (const syntax::ExprId element : array->elements) {
                    pending.push_back(element);
                }
                continue;
            }
            if (const syntax::AstArenaVector<syntax::ExprId>* const tuple =
                    this->core_.ctx_.module.exprs.tuple_elements(current.value);
                kind == syntax::ExprKind::tuple_literal && tuple != nullptr) {
                for (const syntax::ExprId element : *tuple) {
                    pending.push_back(element);
                }
                continue;
            }
            if (const syntax::StructLiteralExprPayload* const structure =
                    this->core_.ctx_.module.exprs.struct_literal_payload(current.value);
                kind == syntax::ExprKind::struct_literal && structure != nullptr) {
                for (const syntax::FieldInit& field : structure->field_inits) {
                    pending.push_back(field.value);
                }
                continue;
            }
        }
        return SEMA_MOVE_INVALID_LOCAL;
    }

    [[nodiscard]] base::usize method_receiver_borrow_origin(const syntax::ExprId callee,
        const MoveEnvironment& environment, const BorrowEnvironment& borrow_environment) const
    {
        syntax::ExprId current = callee;
        std::unordered_set<base::u32> visited;
        while (syntax::is_valid(current) && current.value < this->core_.ctx_.module.exprs.size()
            && visited.insert(current.value).second) {
            const syntax::ExprKind kind = this->core_.ctx_.module.exprs.kind(current.value);
            if (const syntax::GenericApplyExprPayload* const generic =
                    this->core_.ctx_.module.exprs.generic_apply_payload(current.value);
                kind == syntax::ExprKind::generic_apply && generic != nullptr) {
                current = generic->callee;
                continue;
            }
            if (const syntax::FieldExprPayload* const field =
                    this->core_.ctx_.module.exprs.field_payload(current.value);
                kind == syntax::ExprKind::field && field != nullptr) {
                return this->borrow_origin_place(field->object, environment, borrow_environment);
            }
            break;
        }
        return SEMA_MOVE_INVALID_LOCAL;
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

    void apply_action(MoveState& state, const MoveAction& action, const bool report)
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

    void emit_diagnostics()
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

    void report_rejection(const MoveAction& action)
    {
        switch (action.kind) {
            case MoveActionKind::reject_indexed_move:
                this->core_.report_unsupported(action.range, std::string(SEMA_MOVE_INDEXED_ELEMENT_UNSUPPORTED));
                this->record_rejection_fact(action, MoveRejectionKind::indexed_element);
                break;
            case MoveActionKind::reject_pattern_payload:
                this->core_.report_unsupported(action.range, std::string(SEMA_MOVE_PATTERN_PAYLOAD_UNSUPPORTED));
                this->record_rejection_fact(action, MoveRejectionKind::pattern_payload);
                break;
            case MoveActionKind::reject_try_payload:
                this->core_.report_unsupported(action.range, std::string(SEMA_MOVE_TRY_PAYLOAD_UNSUPPORTED));
                this->record_rejection_fact(action, MoveRejectionKind::try_payload);
                break;
            case MoveActionKind::use_local:
            case MoveActionKind::initialize_local:
                break;
        }
    }

    void record_rejection_fact(const MoveAction& action, const MoveRejectionKind kind)
    {
        MoveRejectionFact fact;
        fact.kind = kind;
        fact.expr = action.expr;
        fact.stmt = action.stmt;
        fact.pattern = action.pattern;
        fact.tracked_type = action.tracked_type;
        fact.resource_fingerprint = action.resource_fingerprint;
        fact.diagnostic_emitted = true;
        fact.range = action.range;
        this->move_rejections_.rejections.push_back(fact);
    }

    SemanticAnalyzerCore& core_;
    const syntax::ItemNode& function_;
    const FunctionSignature& signature_;
    ResourceSemanticsClassifier resources_;
    std::unordered_map<base::u32, ResourceSemanticsSummary> resource_cache_;
    FunctionMoveRejectionFacts move_rejections_;
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
