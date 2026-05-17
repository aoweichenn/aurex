#include <aurex/sema/sema.hpp>

#include <aurex/sema/sema_messages.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace aurex::sema {

namespace {

constexpr std::string_view SEMA_GENERIC_INSTANCE_SEPARATOR = "$";
constexpr std::string_view SEMA_GENERIC_KEY_ARG_PREFIX = "t";
constexpr std::string_view SEMA_GENERIC_KEY_ARG_SEPARATOR = ".";
constexpr std::string_view SEMA_GENERIC_ABI_SUFFIX_PREFIX = "__aurexg";
constexpr std::string_view SEMA_GENERIC_ABI_ARG_PREFIX = "_t";
constexpr std::string_view SEMA_CAPABILITY_COPY = "Copy";
constexpr std::string_view SEMA_CAPABILITY_DROP = "Drop";
constexpr std::string_view SEMA_GENERIC_PARAM_IDENTITY_MARKER = "generic-param";
constexpr std::string_view SEMA_GENERIC_TEMPLATE_INCREMENTAL_TAG = "|generic_template";
constexpr base::u64 SEMA_GENERIC_PARAM_IDENTITY_OFFSET = 14695981039346656037ULL;
constexpr base::u64 SEMA_GENERIC_PARAM_IDENTITY_PRIME = 1099511628211ULL;
constexpr base::u32 SEMA_GENERIC_PARAM_IDENTITY_U64_BITS = 64;
constexpr base::u32 SEMA_GENERIC_PARAM_IDENTITY_BYTE_BITS = 8;
constexpr base::u64 SEMA_GENERIC_PARAM_IDENTITY_BYTE_MASK = 0xFFU;
constexpr base::usize SEMA_DECIMAL_U64_MAX_DIGITS = 20;
constexpr base::usize SEMA_GENERIC_KEY_ARG_SIZE_ESTIMATE = 12;
constexpr base::usize SEMA_GENERIC_ABI_ARG_SIZE_ESTIMATE = 14;
constexpr base::usize SEMA_GENERIC_SPAN_LINEAR_DEDUP_LIMIT = 64;

[[nodiscard]] GenericSideTables make_generic_side_tables(
    const GenericNodeSpan expr,
    const GenericNodeSpan pattern,
    const GenericNodeSpan type,
    const GenericNodeSpan stmt,
    const std::span<const base::u32> expr_ids,
    const std::span<const base::u32> pattern_ids,
    const std::span<const base::u32> type_ids,
    const std::span<const base::u32> stmt_ids
) {
    GenericSideTables side_tables;
    side_tables.configure_local_dense(expr, pattern, type, stmt, expr_ids, pattern_ids, type_ids, stmt_ids);
    return side_tables;
}

template <typename Info>
[[nodiscard]] GenericSideTables make_generic_side_tables(const Info& info) {
    return make_generic_side_tables(
        info.expr_span,
        info.pattern_span,
        info.type_span,
        info.stmt_span,
        info.expr_node_ids,
        info.pattern_node_ids,
        info.type_node_ids,
        info.stmt_node_ids
    );
}

[[nodiscard]] base::u64 mix_generic_identity_byte(base::u64 hash, const unsigned char byte) noexcept {
    hash ^= static_cast<base::u64>(byte);
    hash *= SEMA_GENERIC_PARAM_IDENTITY_PRIME;
    return hash;
}

[[nodiscard]] base::u64 mix_generic_identity_text(
    base::u64 hash,
    const std::string_view text
) noexcept {
    for (const unsigned char byte : text) {
        hash = mix_generic_identity_byte(hash, byte);
    }
    return hash;
}

[[nodiscard]] base::u64 mix_generic_identity_u64(
    base::u64 hash,
    const base::u64 value
) noexcept {
    for (base::u32 shift = 0;
         shift < SEMA_GENERIC_PARAM_IDENTITY_U64_BITS;
         shift += SEMA_GENERIC_PARAM_IDENTITY_BYTE_BITS) {
        hash = mix_generic_identity_byte(
            hash,
            static_cast<unsigned char>((value >> shift) & SEMA_GENERIC_PARAM_IDENTITY_BYTE_MASK)
        );
    }
    return hash;
}

[[nodiscard]] GenericParamIdentity finish_generic_param_identity(base::u64 hash) noexcept {
    if (hash == 0) {
        hash = SEMA_GENERIC_PARAM_IDENTITY_OFFSET;
    }
    return GenericParamIdentity {hash};
}

struct GenericNodeSpanBuilder {
    explicit GenericNodeSpanBuilder(const syntax::AstModule& ast_module)
        : module(ast_module) {}

    const syntax::AstModule& module;
    std::vector<syntax::ExprId> exprs;
    std::vector<syntax::PatternId> patterns;
    std::vector<syntax::TypeId> types;
    std::vector<syntax::StmtId> stmts;
    std::unordered_set<base::u32> seen_exprs;
    std::unordered_set<base::u32> seen_patterns;
    std::unordered_set<base::u32> seen_types;
    std::unordered_set<base::u32> seen_stmts;

    void add_type(const syntax::TypeId type) {
        this->add_id(type, this->module.types.size(), this->seen_types, this->types);
    }

    void add_expr(const syntax::ExprId expr) {
        this->add_id(expr, this->module.exprs.size(), this->seen_exprs, this->exprs);
    }

    void add_pattern(const syntax::PatternId pattern) {
        this->add_id(pattern, this->module.patterns.size(), this->seen_patterns, this->patterns);
    }

    void add_stmt(const syntax::StmtId stmt) {
        this->add_id(stmt, this->module.stmts.size(), this->seen_stmts, this->stmts);
    }

    void collect(const syntax::ItemNode& item) {
        this->reserve_initial_worklists(item);
        for (const syntax::ParamDecl& param : item.params) {
            this->add_type(param.type);
        }
        this->add_type(item.return_type);
        this->add_type(item.impl_type);
        this->add_stmt(item.body);
        this->drain();
    }

    [[nodiscard]] GenericNodeSpan expr_span() {
        return make_span(this->exprs);
    }

    [[nodiscard]] GenericNodeSpan pattern_span() {
        return make_span(this->patterns);
    }

    [[nodiscard]] GenericNodeSpan type_span() {
        return make_span(this->types);
    }

    [[nodiscard]] GenericNodeSpan stmt_span() {
        return make_span(this->stmts);
    }

    [[nodiscard]] std::vector<base::u32> expr_ids() const {
        return make_sparse_ids(this->exprs);
    }

    [[nodiscard]] std::vector<base::u32> pattern_ids() const {
        return make_sparse_ids(this->patterns);
    }

    [[nodiscard]] std::vector<base::u32> type_ids() const {
        return make_sparse_ids(this->types);
    }

    [[nodiscard]] std::vector<base::u32> stmt_ids() const {
        return make_sparse_ids(this->stmts);
    }

private:
    void reserve_initial_worklists(const syntax::ItemNode& item) {
        this->types.reserve(item.params.size() + 2U);
        this->stmts.reserve(syntax::is_valid(item.body) ? 1U : 0U);
        this->exprs.reserve(0U);
        this->patterns.reserve(0U);
    }

    template <typename Id>
    void add_id(
        const Id id,
        const base::usize node_count,
        std::unordered_set<base::u32>& seen,
        std::vector<Id>& ids
    ) {
        if (!syntax::is_valid(id) || id.value >= node_count) {
            return;
        }
        if (seen.empty()) {
            const auto duplicate = std::ranges::find(ids, id.value, &Id::value);
            if (duplicate != ids.end()) {
                return;
            }
            if (ids.size() < SEMA_GENERIC_SPAN_LINEAR_DEDUP_LIMIT) {
                ids.push_back(id);
                return;
            }
            seen.reserve(ids.size() + 1U);
            for (const Id existing : ids) {
                seen.insert(existing.value);
            }
        }
        if (!seen.insert(id.value).second) {
            return;
        }
        ids.push_back(id);
    }

    template <typename Id>
    [[nodiscard]] static std::vector<base::u32> make_sparse_ids(const std::vector<Id>& ids) {
        if (ids.empty()) {
            return {};
        }
        std::vector<base::u32> values;
        values.reserve(ids.size());
        bool contiguous = true;
        base::u32 expected = ids.front().value;
        for (const Id id : ids) {
            contiguous = contiguous && id.value == expected;
            values.push_back(id.value);
            ++expected;
        }
        if (contiguous) {
            values.clear();
        }
        return values;
    }

    template <typename Id>
    [[nodiscard]] static GenericNodeSpan make_span(std::vector<Id>& ids) {
        std::ranges::sort(ids, {}, &Id::value);
        const auto last = std::ranges::unique(ids, {}, &Id::value).begin();
        ids.erase(last, ids.end());
        if (ids.empty()) {
            return {};
        }
        const base::u32 begin = ids.front().value;
        const base::u32 end = ids.back().value + 1U;
        return GenericNodeSpan {begin, end - begin};
    }

    void drain() {
        base::usize type_cursor = 0;
        base::usize expr_cursor = 0;
        base::usize pattern_cursor = 0;
        base::usize stmt_cursor = 0;
        while (type_cursor < this->types.size() ||
               expr_cursor < this->exprs.size() ||
               pattern_cursor < this->patterns.size() ||
               stmt_cursor < this->stmts.size()) {
            while (type_cursor < this->types.size()) {
                this->visit_type(this->types[type_cursor]);
                ++type_cursor;
            }
            while (expr_cursor < this->exprs.size()) {
                this->visit_expr(this->exprs[expr_cursor]);
                ++expr_cursor;
            }
            while (pattern_cursor < this->patterns.size()) {
                this->visit_pattern(this->patterns[pattern_cursor]);
                ++pattern_cursor;
            }
            while (stmt_cursor < this->stmts.size()) {
                this->visit_stmt(this->stmts[stmt_cursor]);
                ++stmt_cursor;
            }
        }
    }

    void visit_type(const syntax::TypeId type_id) {
        if (!syntax::is_valid(type_id) || type_id.value >= this->module.types.size()) {
            return;
        }
        const syntax::TypeNode type = this->module.types[type_id.value];
        for (const syntax::TypeId arg : type.type_args) {
            this->add_type(arg);
        }
        this->add_type(type.pointee);
        this->add_type(type.array_element);
        this->add_type(type.slice_element);
        for (const syntax::TypeId element : type.tuple_elements) {
            this->add_type(element);
        }
        for (const syntax::TypeId param : type.function_params) {
            this->add_type(param);
        }
        this->add_type(type.function_return);
    }

    void visit_expr(const syntax::ExprId expr_id) {
        if (!syntax::is_valid(expr_id) || expr_id.value >= this->module.exprs.size()) {
            return;
        }
        switch (this->module.exprs.kind(expr_id.value)) {
        case syntax::ExprKind::name:
            if (const syntax::NameExprPayload* const payload = this->module.exprs.name_payload(expr_id.value);
                payload != nullptr) {
                for (const syntax::TypeId arg : payload->type_args) {
                    this->add_type(arg);
                }
            }
            break;
        case syntax::ExprKind::generic_apply:
            if (const syntax::GenericApplyExprPayload* const payload = this->module.exprs.generic_apply_payload(expr_id.value);
                payload != nullptr) {
                this->add_expr(payload->callee);
                for (const syntax::TypeId arg : payload->type_args) {
                    this->add_type(arg);
                }
            }
            break;
        case syntax::ExprKind::unary:
        case syntax::ExprKind::try_expr:
            if (const syntax::UnaryExprPayload* const payload = this->module.exprs.unary_payload(expr_id.value);
                payload != nullptr) {
                this->add_expr(payload->operand);
            }
            break;
        case syntax::ExprKind::binary:
            if (const syntax::BinaryExprPayload* const payload = this->module.exprs.binary_payload(expr_id.value);
                payload != nullptr) {
                this->add_expr(payload->lhs);
                this->add_expr(payload->rhs);
            }
            break;
        case syntax::ExprKind::call:
        case syntax::ExprKind::str_from_bytes_unchecked:
            if (const syntax::CallExprPayload* const payload = this->module.exprs.call_payload(expr_id.value);
                payload != nullptr) {
                this->add_expr(payload->callee);
                for (const syntax::ExprId arg : payload->args) {
                    this->add_expr(arg);
                }
            }
            break;
        case syntax::ExprKind::if_expr:
            if (const syntax::IfExprPayload* const payload = this->module.exprs.if_payload(expr_id.value);
                payload != nullptr) {
                this->add_expr(payload->condition);
                this->add_pattern(payload->condition_pattern);
                this->add_expr(payload->then_expr);
                this->add_expr(payload->else_expr);
            }
            break;
        case syntax::ExprKind::block_expr:
        case syntax::ExprKind::unsafe_block:
            if (const syntax::BlockExprPayload* const payload = this->module.exprs.block_payload(expr_id.value);
                payload != nullptr) {
                this->add_stmt(payload->block);
                this->add_expr(payload->result);
            }
            break;
        case syntax::ExprKind::match_expr:
            if (const syntax::MatchExprPayload* const payload = this->module.exprs.match_payload(expr_id.value);
                payload != nullptr) {
                this->add_expr(payload->value);
                for (const syntax::MatchArm& arm : payload->arms) {
                    this->add_pattern(arm.pattern);
                    this->add_expr(arm.guard);
                    this->add_expr(arm.value);
                }
            }
            break;
        case syntax::ExprKind::array_literal:
            if (const syntax::ArrayExprPayload* const payload = this->module.exprs.array_payload(expr_id.value);
                payload != nullptr) {
                for (const syntax::ExprId element : payload->elements) {
                    this->add_expr(element);
                }
                this->add_expr(payload->repeat_value);
                this->add_expr(payload->repeat_count);
            }
            break;
        case syntax::ExprKind::tuple_literal:
            if (const syntax::AstArenaVector<syntax::ExprId>* const payload =
                    this->module.exprs.tuple_elements(expr_id.value);
                payload != nullptr) {
                for (const syntax::ExprId element : *payload) {
                    this->add_expr(element);
                }
            }
            break;
        case syntax::ExprKind::field:
            if (const syntax::FieldExprPayload* const payload = this->module.exprs.field_payload(expr_id.value);
                payload != nullptr) {
                this->add_expr(payload->object);
            }
            break;
        case syntax::ExprKind::index:
            if (const syntax::IndexExprPayload* const payload = this->module.exprs.index_payload(expr_id.value);
                payload != nullptr) {
                this->add_expr(payload->object);
                this->add_expr(payload->index);
            }
            break;
        case syntax::ExprKind::slice:
            if (const syntax::SliceExprPayload* const payload = this->module.exprs.slice_payload(expr_id.value);
                payload != nullptr) {
                this->add_expr(payload->object);
                this->add_expr(payload->start);
                this->add_expr(payload->end);
            }
            break;
        case syntax::ExprKind::struct_literal:
            if (const syntax::StructLiteralExprPayload* const payload =
                    this->module.exprs.struct_literal_payload(expr_id.value);
                payload != nullptr) {
                this->add_expr(payload->object);
                for (const syntax::TypeId arg : payload->type_args) {
                    this->add_type(arg);
                }
                for (const syntax::FieldInit& init : payload->field_inits) {
                    this->add_expr(init.value);
                }
            }
            break;
        case syntax::ExprKind::cast:
        case syntax::ExprKind::pcast:
        case syntax::ExprKind::bcast:
        case syntax::ExprKind::size_of:
        case syntax::ExprKind::align_of:
        case syntax::ExprKind::ptr_addr:
        case syntax::ExprKind::paddr:
        case syntax::ExprKind::slice_data:
        case syntax::ExprKind::slice_len:
        case syntax::ExprKind::str_data:
        case syntax::ExprKind::str_byte_len:
        case syntax::ExprKind::str_is_valid_utf8:
        case syntax::ExprKind::str_from_utf8_checked:
            if (const syntax::CastExprPayload* const payload = this->module.exprs.cast_payload(expr_id.value);
                payload != nullptr) {
                this->add_type(payload->type);
                this->add_expr(payload->expr);
            }
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
            break;
        }
    }

    void visit_pattern(const syntax::PatternId pattern_id) {
        if (!syntax::is_valid(pattern_id) || pattern_id.value >= this->module.patterns.size()) {
            return;
        }
        const syntax::PatternNode pattern = this->module.patterns[pattern_id.value];
        this->add_type(pattern.enum_type);
        for (const syntax::PatternId payload : pattern.payload_patterns) {
            this->add_pattern(payload);
        }
        for (const syntax::PatternId element : pattern.elements) {
            this->add_pattern(element);
        }
        for (const syntax::FieldPattern& field : pattern.field_patterns) {
            this->add_pattern(field.pattern);
        }
        for (const syntax::PatternId alternative : pattern.alternatives) {
            this->add_pattern(alternative);
        }
    }

    void visit_stmt(const syntax::StmtId stmt_id) {
        if (!syntax::is_valid(stmt_id) || stmt_id.value >= this->module.stmts.size()) {
            return;
        }
        const syntax::StmtNode stmt = this->module.stmts[stmt_id.value];
        this->add_pattern(stmt.pattern);
        this->add_type(stmt.declared_type);
        this->add_expr(stmt.init);
        this->add_expr(stmt.lhs);
        this->add_expr(stmt.rhs);
        this->add_expr(stmt.condition);
        this->add_expr(stmt.range_start);
        this->add_expr(stmt.range_end);
        this->add_expr(stmt.range_step);
        this->add_stmt(stmt.then_block);
        this->add_stmt(stmt.else_block);
        this->add_stmt(stmt.else_if);
        this->add_stmt(stmt.body);
        this->add_stmt(stmt.for_init);
        this->add_stmt(stmt.for_update);
        this->add_expr(stmt.return_value);
        for (const syntax::StmtId child : stmt.statements) {
            this->add_stmt(child);
        }
    }
};

[[nodiscard]] std::optional<CapabilityKind> parse_capability_kind(const std::string_view name) noexcept {
    if (name == capability_name(CapabilityKind::sized)) {
        return CapabilityKind::sized;
    }
    if (name == capability_name(CapabilityKind::eq)) {
        return CapabilityKind::eq;
    }
    if (name == capability_name(CapabilityKind::ord)) {
        return CapabilityKind::ord;
    }
    if (name == capability_name(CapabilityKind::hash)) {
        return CapabilityKind::hash;
    }
    return std::nullopt;
}

[[nodiscard]] bool is_resource_capability(const std::string_view name) noexcept {
    return name == SEMA_CAPABILITY_COPY || name == SEMA_CAPABILITY_DROP;
}

void append_decimal(std::string& output, const base::u64 value) {
    char buffer[SEMA_DECIMAL_U64_MAX_DIGITS] {};
    const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (error == std::errc()) {
        output.append(buffer, end);
    }
}

void append_generic_instance_key_suffix(std::string& output, const std::vector<TypeHandle>& args) {
    output += SEMA_GENERIC_KEY_ARG_PREFIX;
    for (base::usize i = 0; i < args.size(); ++i) {
        if (i != 0) {
            output += SEMA_GENERIC_KEY_ARG_SEPARATOR;
        }
        append_decimal(output, args[i].value);
    }
}

void assign_node_ids(SemaIndexTable& target, const std::vector<base::u32>& source) {
    target.assign(source.begin(), source.end());
}

} // namespace

struct SemanticAnalyzer::GenericAnalysisScope {
    GenericAnalysisScope(
        SemanticAnalyzer& analyzer,
        const syntax::ModuleId module,
        GenericContext* const generic_context,
        GenericSideTables* const side_tables = nullptr,
        const bool cache_syntax_types = false
    )
        : analyzer(analyzer),
          previous_module(analyzer.current_module_),
          previous_generic_context(analyzer.current_generic_context_),
          previous_side_tables(analyzer.current_side_tables_) {
        this->analyzer.current_module_ = module;
        this->analyzer.current_generic_context_ = generic_context;
        if (side_tables != nullptr) {
            this->analyzer.current_side_tables_.side_tables = side_tables;
        }
        this->analyzer.current_side_tables_.cache_syntax_types = cache_syntax_types;
    }

    GenericAnalysisScope(const GenericAnalysisScope&) = delete;
    GenericAnalysisScope& operator=(const GenericAnalysisScope&) = delete;

    ~GenericAnalysisScope() {
        this->analyzer.current_module_ = this->previous_module;
        this->analyzer.current_generic_context_ = this->previous_generic_context;
        this->analyzer.current_side_tables_ = this->previous_side_tables;
    }

    SemanticAnalyzer& analyzer;
    syntax::ModuleId previous_module;
    GenericContext* previous_generic_context = nullptr;
    GenericSideTableScope previous_side_tables {};
};

std::string_view capability_name(const CapabilityKind capability) noexcept {
    switch (capability) {
    case CapabilityKind::sized:
        return "Sized";
    case CapabilityKind::eq:
        return "Eq";
    case CapabilityKind::ord:
        return "Ord";
    case CapabilityKind::hash:
        return "Hash";
    }
    return "<invalid>";
}

bool SemanticAnalyzer::has_generic_params(const syntax::ItemNode& item) const noexcept {
    return !item.generic_params.empty();
}

bool SemanticAnalyzer::has_generic_constraints(const syntax::ItemNode& item) const noexcept {
    return !item.where_constraints.empty();
}

void SemanticAnalyzer::validate_generic_parameter_list(const syntax::ItemNode& item) {
    std::unordered_map<IdentId, base::SourceRange, IdentIdHash> seen;
    seen.reserve(item.generic_params.size());
    for (const syntax::GenericParamDecl& param : item.generic_params) {
        const auto [first, inserted] = seen.emplace(param.name_id, param.range);
        if (!inserted) {
            this->report_duplicate(param.range, sema_duplicate_generic_parameter_message(param.name));
            this->report_note(
                first->second,
                SemanticDiagnosticKind::duplicate,
                sema_first_generic_parameter_message(param.name)
            );
        }
    }
}

void SemanticAnalyzer::validate_generic_constraints(
    const syntax::ItemNode& item,
    GenericTemplateInfo& info
) {
    info.constraints.reserve(item.generic_params.size());
    std::unordered_set<IdentId, IdentIdHash> params;
    params.reserve(item.generic_params.size());
    for (const syntax::GenericParamDecl& param : item.generic_params) {
        params.insert(param.name_id);
    }

    for (const syntax::GenericConstraintDecl& constraint : item.where_constraints) {
        if (!params.contains(constraint.param_name_id)) {
            this->report_lookup(
                constraint.param_range,
                sema_unknown_generic_constraint_param_message(constraint.param_name)
            );
            continue;
        }
        CapabilitySet& capabilities = this->capability_bucket(info.constraints, constraint.param_name_id);
        for (base::usize i = 0; i < constraint.capability_names.size(); ++i) {
            const std::string_view capability_name = constraint.capability_names[i];
            const base::SourceRange capability_range =
                i < constraint.capability_ranges.size() ? constraint.capability_ranges[i] : constraint.range;
            if (is_resource_capability(capability_name)) {
                this->report_unsupported(
                    capability_range,
                    std::string(SEMA_GENERIC_RESOURCE_CAPABILITY_UNSUPPORTED)
                );
                continue;
            }
            const std::optional<CapabilityKind> capability = parse_capability_kind(capability_name);
            if (!capability.has_value()) {
                this->report_capability(capability_range, sema_unknown_capability_message(capability_name));
                continue;
            }
            if (!capabilities.insert(*capability).second) {
                this->report_capability(
                    capability_range,
                    sema_duplicate_capability_message(constraint.param_name, capability_name)
                );
            }
        }
    }
}

bool SemanticAnalyzer::generic_param_has_capability(
    const std::string_view param,
    const CapabilityKind capability
) const {
    if (this->current_generic_context_ == nullptr) {
        return false;
    }
    const IdentId param_id = this->module_.find_identifier(param);
    const auto identity = this->current_generic_context_->param_identities.find(param_id);
    if (identity != this->current_generic_context_->param_identities.end()) {
        const auto by_identity = this->current_generic_context_->constraints_by_identity.find(identity->second);
        if (by_identity != this->current_generic_context_->constraints_by_identity.end()) {
            return by_identity->second.contains(capability);
        }
    }
    const auto found = this->current_generic_context_->constraints.find(param_id);
    return found != this->current_generic_context_->constraints.end() &&
           found->second.contains(capability);
}

bool SemanticAnalyzer::generic_param_has_capability(
    const TypeHandle param,
    const CapabilityKind capability
) const {
    if (this->current_generic_context_ == nullptr || !is_valid(param)) {
        return false;
    }
    const TypeInfo& info = this->checked_.types.get(param);
    if (info.kind != TypeKind::generic_param) {
        return false;
    }
    const GenericParamIdentity identity = this->generic_param_identity(info);
    if (is_valid(identity)) {
        const auto found = this->current_generic_context_->constraints_by_identity.find(identity);
        if (found != this->current_generic_context_->constraints_by_identity.end()) {
            return found->second.contains(capability);
        }
    }
    return this->generic_param_has_capability(info.name, capability);
}

bool SemanticAnalyzer::type_satisfies_capability(
    const TypeHandle type,
    const CapabilityKind capability
) const {
    if (!is_valid(type)) {
        return false;
    }
    const TypeInfo& info = this->checked_.types.get(type);
    if (info.kind == TypeKind::generic_param) {
        return this->generic_param_has_capability(type, capability);
    }
    if (capability == CapabilityKind::sized) {
        return this->is_valid_storage_type(type);
    }
    if (capability == CapabilityKind::eq) {
        return this->type_satisfies_equality_capability(type);
    }
    if (capability == CapabilityKind::ord) {
        return this->type_satisfies_ordering_capability(type);
    }
    if (capability == CapabilityKind::hash) {
        return this->type_supports_hash_capability(type);
    }
    return false;
}

bool SemanticAnalyzer::type_satisfies_equality_capability(const TypeHandle type) const {
    if (!is_valid(type)) {
        return false;
    }
    const TypeInfo& info = this->checked_.types.get(type);
    return this->checked_.types.is_bool(type) ||
           this->checked_.types.is_char(type) ||
           this->checked_.types.is_integer(type) ||
           this->checked_.types.is_pointer(type) ||
           (info.kind == TypeKind::enum_ && !is_valid(info.enum_payload_storage));
}

bool SemanticAnalyzer::type_satisfies_ordering_capability(const TypeHandle type) const {
    return this->checked_.types.is_integer(type);
}

bool SemanticAnalyzer::type_supports_equality_operator(const TypeHandle type) const {
    if (!is_valid(type)) {
        return false;
    }
    const TypeInfo& info = this->checked_.types.get(type);
    return this->checked_.types.is_bool(type) ||
           this->checked_.types.is_char(type) ||
           this->checked_.types.is_integer(type) ||
           this->checked_.types.is_float(type) ||
           this->checked_.types.is_pointer(type) ||
           (info.kind == TypeKind::enum_ && !is_valid(info.enum_payload_storage));
}

bool SemanticAnalyzer::type_supports_ordering_operator(const TypeHandle type) const {
    return this->checked_.types.is_integer(type) || this->checked_.types.is_float(type);
}

bool SemanticAnalyzer::type_supports_hash_capability(const TypeHandle type) const {
    if (!is_valid(type)) {
        return false;
    }
    return this->checked_.types.is_bool(type) ||
           this->checked_.types.is_char(type) ||
           this->checked_.types.is_integer(type) ||
           this->checked_.types.is_pointer(type);
}

bool SemanticAnalyzer::validate_generic_arguments(
    const GenericTemplateInfo& info,
    const std::vector<TypeHandle>& args,
    const base::SourceRange& use_range
) {
    bool ok = true;
    for (base::usize i = 0; i < info.params.size() && i < args.size(); ++i) {
        const IdentId param_id = info.params[i];
        const auto found = info.constraints.find(param_id);
        if (found == info.constraints.end()) {
            continue;
        }
        for (const CapabilityKind capability : found->second) {
            if (!this->type_satisfies_capability(args[i], capability)) {
                this->report_capability(
                    use_range,
                    sema_generic_capability_not_satisfied_message(
                        this->checked_.types.display_name(args[i]),
                        capability_name(capability)
                    )
                );
                ok = false;
            }
        }
    }
    return ok;
}

void SemanticAnalyzer::populate_generic_template_node_spans(
    GenericTemplateInfo& info,
    const syntax::ItemNode& item
) const {
    GenericNodeSpanBuilder builder(this->module_);
    builder.collect(item);
    info.expr_span = builder.expr_span();
    info.pattern_span = builder.pattern_span();
    info.type_span = builder.type_span();
    info.stmt_span = builder.stmt_span();
    assign_node_ids(info.expr_node_ids, builder.expr_ids());
    assign_node_ids(info.pattern_node_ids, builder.pattern_ids());
    assign_node_ids(info.type_node_ids, builder.type_ids());
    assign_node_ids(info.stmt_node_ids, builder.stmt_ids());
}

bool SemanticAnalyzer::GenericTemplateInfo::has_sparse_node_ids() const noexcept {
    return !this->expr_node_ids.empty() ||
           !this->pattern_node_ids.empty() ||
           !this->type_node_ids.empty() ||
           !this->stmt_node_ids.empty();
}

GenericSideTables SemanticAnalyzer::make_generic_instance_side_tables(const GenericTemplateInfo& info) {
    if (!info.has_sparse_node_ids()) {
        return make_generic_side_tables(info);
    }
    const base::usize layout_index = this->generic_side_table_layout_index(info);
    const GenericSideTableLayout* const layout = this->checked_.generic_side_table_layout(layout_index);
    GenericSideTables side_tables;
    if (layout != nullptr) {
        side_tables.configure_local_dense(*layout);
    }
    return side_tables;
}

base::usize SemanticAnalyzer::generic_side_table_layout_index(const GenericTemplateInfo& info) {
    if (info.checked_side_table_layout_index != SEMA_GENERIC_SIDE_TABLE_INVALID_LAYOUT_INDEX) {
        return info.checked_side_table_layout_index;
    }
    info.checked_side_table_layout_index = this->checked_.append_generic_side_table_layout(
        info.expr_span,
        info.pattern_span,
        info.type_span,
        info.stmt_span,
        info.expr_node_ids,
        info.pattern_node_ids,
        info.type_node_ids,
        info.stmt_node_ids
    );
    return info.checked_side_table_layout_index;
}

void SemanticAnalyzer::register_generic_template(
    const syntax::ItemNode& item,
    const syntax::ItemId item_id
) {
    this->validate_generic_parameter_list(item);
    const syntax::ModuleId owner = this->item_module(item_id);
    GenericTemplateInfo info = this->make_generic_template_info();
    info.item = item_id;
    info.module = owner;
    info.name = this->source_name_text(item.name_id, item.name);
    info.name_id = item.name_id;
    info.key = this->module_lookup_key(owner, item.name_id);
    info.function_key = this->function_lookup_key(owner, item.name_id);
    info.visibility = item.visibility;
    info.stable_id = this->stable_definition_id(
        owner,
        StableSymbolKind::generic_template,
        item.name_id,
        item.name
    );
    info.incremental_key = this->stable_incremental_key(
        info.stable_id,
        std::string(item.name) + std::string(SEMA_GENERIC_TEMPLATE_INCREMENTAL_TAG)
    );
    info.params.reserve(item.generic_params.size());
    for (const syntax::GenericParamDecl& param : item.generic_params) {
        info.params.push_back(param.name_id);
    }
    this->populate_generic_template_node_spans(info, item);
    this->populate_generic_param_identities(info);
    this->validate_generic_constraints(item, info);

    if (item.kind == syntax::ItemKind::struct_decl) {
        if (this->named_types_.contains(info.key) ||
            this->checked_.type_aliases.contains(info.key) ||
            this->generic_struct_templates_.contains(info.key) ||
            this->generic_enum_templates_.contains(info.key) ||
            this->generic_type_alias_templates_.contains(info.key)) {
            this->report_duplicate(
                item.range,
                sema_duplicate_type_definition_message(this->module_name(owner), item.name)
            );
            return;
        }
        const auto inserted = this->generic_struct_templates_.emplace(info.key, std::move(info));
        if (inserted.second) {
            this->index_generic_struct_template(inserted.first->second);
        }
        return;
    }

    if (item.kind == syntax::ItemKind::enum_decl) {
        if (this->named_types_.contains(info.key) ||
            this->checked_.type_aliases.contains(info.key) ||
            this->generic_struct_templates_.contains(info.key) ||
            this->generic_enum_templates_.contains(info.key) ||
            this->generic_type_alias_templates_.contains(info.key)) {
            this->report_duplicate(
                item.range,
                sema_duplicate_type_definition_message(this->module_name(owner), item.name)
            );
            return;
        }
        const auto inserted = this->generic_enum_templates_.emplace(info.key, std::move(info));
        if (inserted.second) {
            this->index_generic_enum_template(inserted.first->second);
        }
        return;
    }

    if (item.kind == syntax::ItemKind::type_alias) {
        if (this->named_types_.contains(info.key) ||
            this->checked_.type_aliases.contains(info.key) ||
            this->generic_struct_templates_.contains(info.key) ||
            this->generic_enum_templates_.contains(info.key) ||
            this->generic_type_alias_templates_.contains(info.key)) {
            this->report_duplicate(
                item.range,
                sema_duplicate_type_definition_message(this->module_name(owner), item.name)
            );
            return;
        }
        const auto inserted = this->generic_type_alias_templates_.emplace(info.key, std::move(info));
        if (inserted.second) {
            this->index_generic_type_alias_template(inserted.first->second);
        }
        return;
    }

    if (item.kind != syntax::ItemKind::fn_decl) {
        if (item.kind != syntax::ItemKind::impl_block) {
            this->report_unsupported(item.range, std::string(SEMA_GENERIC_PARAMS_UNSUPPORTED_ON_ITEM));
        }
        return;
    }

    if (item.visibility == syntax::Visibility::public_ && !syntax::is_valid(item.return_type)) {
        this->report_general(item.range, std::string(SEMA_PUBLIC_FUNCTION_RETURN_TYPE_EXPLICIT));
    }
    if (item.is_extern_c || item.is_export_c || item.is_prototype) {
        this->report_unsupported(item.range, std::string(SEMA_GENERIC_C_ABI_OR_PROTOTYPE_UNSUPPORTED));
    }
    if (syntax::is_valid(item.impl_type)) {
        GenericContext generic_context = this->make_generic_context();
        this->populate_generic_placeholder_context(info, generic_context);
        {
            GenericAnalysisScope scope(*this, owner, &generic_context);
            info.impl_type_pattern = this->resolve_type(item.impl_type);
        }
        if (!is_valid(info.impl_type_pattern)) {
            return;
        }
        info.function_key = this->method_function_lookup_key(owner, info.impl_type_pattern, item.name_id);
        this->populate_generic_param_identities(info);
        const TypeKind impl_type_kind = this->checked_.types.get(info.impl_type_pattern).kind;
        if (impl_type_kind != TypeKind::struct_ &&
            impl_type_kind != TypeKind::enum_ &&
            impl_type_kind != TypeKind::opaque_struct) {
            this->report_general(item.range, std::string(SEMA_IMPL_TARGET_NAMED_TYPE));
            return;
        }

        std::unordered_set<GenericParamIdentity, GenericParamIdentityHash> owner_params;
        std::vector<TypeHandle> pending;
        pending.push_back(info.impl_type_pattern);
        while (!pending.empty()) {
            const TypeHandle current = pending.back();
            pending.pop_back();
            if (!is_valid(current)) {
                continue;
            }
            const TypeInfo& type_info = this->checked_.types.get(current);
            if (type_info.kind == TypeKind::generic_param) {
                owner_params.insert(this->generic_param_identity(type_info));
                continue;
            }
            if (type_info.kind == TypeKind::pointer || type_info.kind == TypeKind::reference) {
                pending.push_back(type_info.pointee);
            } else if (type_info.kind == TypeKind::slice) {
                pending.push_back(type_info.slice_element);
            } else if (type_info.kind == TypeKind::array) {
                pending.push_back(type_info.array_element);
            } else if (type_info.kind == TypeKind::tuple) {
                for (const TypeHandle element : type_info.tuple_elements) {
                    pending.push_back(element);
                }
            } else if (type_info.kind == TypeKind::function) {
                pending.push_back(type_info.function_return);
                for (const TypeHandle param : type_info.function_params) {
                    pending.push_back(param);
                }
            } else if (type_info.kind == TypeKind::struct_ || type_info.kind == TypeKind::enum_) {
                for (const TypeHandle arg : type_info.generic_args) {
                    pending.push_back(arg);
                }
            }
        }
        bool method_local_generic = false;
        for (base::usize i = 0; i < info.params.size(); ++i) {
            if (!owner_params.contains(this->generic_param_identity(info, i))) {
                method_local_generic = true;
            }
        }
        if (method_local_generic) {
            this->report_unsupported(item.range, std::string(SEMA_GENERIC_METHODS_UNSUPPORTED));
            return;
        }
        if (this->type_member_name_exists(info.impl_type_pattern, item.name_id, item.name)) {
            this->report_duplicate(
                item.range,
                sema_duplicate_type_member_message(
                    this->checked_.types.display_name(info.impl_type_pattern),
                    item.name
                )
            );
            return;
        }
        const auto inserted = this->generic_method_templates_.emplace(info.function_key, std::move(info));
        if (inserted.second) {
            this->index_generic_method_template(inserted.first->second);
        }
        return;
    }
    if (this->checked_.functions.contains(info.function_key) || this->generic_function_templates_.contains(info.key)) {
        this->report_duplicate(
            item.range,
            sema_duplicate_function_definition_message(this->module_name(owner), item.name)
        );
        return;
    }
    const auto inserted = this->generic_function_templates_.emplace(info.key, std::move(info));
    if (inserted.second) {
        this->index_generic_function_template(inserted.first->second);
    }
}

void SemanticAnalyzer::populate_generic_param_identities(GenericTemplateInfo& info) {
    info.param_identities.clear();
    info.param_identities.reserve(info.params.size());
    for (base::usize index = 0; index < info.params.size(); ++index) {
        info.param_identities.push_back(this->make_generic_param_identity(info, index));
    }
}

GenericParamIdentity SemanticAnalyzer::make_generic_param_identity(
    const GenericTemplateInfo& info,
    const base::usize index
) const {
    base::u64 hash = SEMA_GENERIC_PARAM_IDENTITY_OFFSET;
    hash = mix_generic_identity_text(hash, SEMA_GENERIC_PARAM_IDENTITY_MARKER);
    hash = mix_generic_identity_u64(hash, info.key.module);
    const std::string_view template_name = this->module_.identifier_text(info.name_id).empty()
        ? info.name.view()
        : this->module_.identifier_text(info.name_id);
    hash = mix_generic_identity_text(hash, template_name);
    hash = mix_generic_identity_u64(hash, index);
    if (index < info.params.size()) {
        hash = mix_generic_identity_text(hash, this->generic_param_name(info, index));
    }
    if (syntax::is_valid(info.item) && info.item.value < this->module_.items.size()) {
        const syntax::ItemNode item = this->module_.items[info.item.value];
        if (index < item.generic_params.size()) {
            const base::SourceRange range = item.generic_params[index].range;
            hash = mix_generic_identity_u64(hash, range.source.value);
            hash = mix_generic_identity_u64(hash, range.begin);
            hash = mix_generic_identity_u64(hash, range.end);
        }
    }
    return finish_generic_param_identity(hash);
}

std::string_view SemanticAnalyzer::generic_param_name(
    const GenericTemplateInfo& info,
    const base::usize index
) const {
    if (index >= info.params.size()) {
        return {};
    }
    const std::string_view interned_name = this->module_.identifier_text(info.params[index]);
    if (!interned_name.empty()) {
        return interned_name;
    }
    if (syntax::is_valid(info.item) && info.item.value < this->module_.items.size()) {
        const syntax::ItemNode item = this->module_.items[info.item.value];
        if (index < item.generic_params.size()) {
            return item.generic_params[index].name;
        }
    }
    return {};
}

GenericParamIdentity SemanticAnalyzer::generic_param_identity(
    const GenericTemplateInfo& info,
    const base::usize index
) const {
    if (index < info.param_identities.size()) {
        return info.param_identities[index];
    }
    return this->make_generic_param_identity(info, index);
}

GenericParamIdentity SemanticAnalyzer::generic_param_identity(const TypeInfo& info) const {
    if (is_valid(info.generic_identity)) {
        return info.generic_identity;
    }
    return generic_param_identity_from_text(info.name);
}

TypeHandle SemanticAnalyzer::generic_param_placeholder(
    const GenericTemplateInfo& info,
    const base::usize index
) {
    if (index >= info.params.size()) {
        return INVALID_TYPE_HANDLE;
    }
    return this->checked_.types.generic_param(
        this->generic_param_identity(info, index),
        this->generic_param_name(info, index)
    );
}

void SemanticAnalyzer::populate_generic_placeholder_context(
    const GenericTemplateInfo& info,
    GenericContext& context
) {
    context.params.clear();
    context.param_identities.clear();
    this->copy_capability_map(context.constraints, info.constraints);
    context.constraints_by_identity.clear();
    context.params.reserve(info.params.size());
    context.param_identities.reserve(info.params.size());
    context.constraints_by_identity.reserve(info.params.size());
    for (base::usize i = 0; i < info.params.size(); ++i) {
        const GenericParamIdentity identity = this->generic_param_identity(info, i);
        const IdentId param_id = info.params[i];
        context.params.emplace(param_id, this->generic_param_placeholder(info, i));
        context.param_identities.emplace(param_id, identity);
        if (const auto constraints = info.constraints.find(param_id); constraints != info.constraints.end()) {
            context.constraints_by_identity.emplace(identity, this->copy_capability_set(constraints->second));
        }
    }
}

void SemanticAnalyzer::populate_generic_concrete_context(
    const GenericTemplateInfo& info,
    const std::vector<TypeHandle>& args,
    GenericContext& context
) const
{
    context.params.clear();
    context.param_identities.clear();
    this->copy_capability_map(context.constraints, info.constraints);
    context.constraints_by_identity.clear();
    context.params.reserve(info.params.size());
    context.param_identities.reserve(info.params.size());
    context.constraints_by_identity.reserve(info.params.size());
    for (base::usize i = 0; i < info.params.size() && i < args.size(); ++i) {
        const GenericParamIdentity identity = this->generic_param_identity(info, i);
        const IdentId param_id = info.params[i];
        context.params.emplace(param_id, args[i]);
        context.param_identities.emplace(param_id, identity);
        const auto constraints = info.constraints.find(param_id);
        if (constraints == info.constraints.end()) {
            continue;
        }
        context.constraints_by_identity.emplace(identity, this->copy_capability_set(constraints->second));
        if (is_valid(args[i])) {
            const TypeInfo& arg_info = this->checked_.types.get(args[i]);
            if (arg_info.kind == TypeKind::generic_param) {
                CapabilitySet& inherited = this->capability_bucket(
                    context.constraints_by_identity,
                    this->generic_param_identity(arg_info)
                );
                inherited.insert(constraints->second.begin(), constraints->second.end());
            }
        }
    }
}

std::string SemanticAnalyzer::generic_instance_key_suffix(const std::vector<TypeHandle>& args) const {
    std::string suffix;
    suffix.reserve(SEMA_GENERIC_KEY_ARG_PREFIX.size() + args.size() * SEMA_GENERIC_KEY_ARG_SIZE_ESTIMATE);
    append_generic_instance_key_suffix(suffix, args);
    return suffix;
}

std::string SemanticAnalyzer::generic_instance_abi_suffix(const std::vector<TypeHandle>& args) const {
    std::string suffix(SEMA_GENERIC_ABI_SUFFIX_PREFIX);
    suffix.reserve(SEMA_GENERIC_ABI_SUFFIX_PREFIX.size() + args.size() * SEMA_GENERIC_ABI_ARG_SIZE_ESTIMATE);
    for (const TypeHandle arg : args) {
        suffix += SEMA_GENERIC_ABI_ARG_PREFIX;
        append_decimal(suffix, arg.value);
    }
    return suffix;
}

std::string SemanticAnalyzer::generic_instance_key(
    const GenericTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    const std::string template_key = this->generic_template_key_prefix(info.module, info.name_id, info.name);
    std::string key;
    key.reserve(
        template_key.size() +
        SEMA_GENERIC_INSTANCE_SEPARATOR.size() +
        SEMA_GENERIC_KEY_ARG_PREFIX.size() +
        args.size() * SEMA_GENERIC_KEY_ARG_SIZE_ESTIMATE
    );
    key += template_key;
    key += SEMA_GENERIC_INSTANCE_SEPARATOR;
    append_generic_instance_key_suffix(key, args);
    return key;
}

std::string SemanticAnalyzer::generic_struct_instance_key(
    const GenericTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    return this->generic_instance_key(info, args);
}

std::string SemanticAnalyzer::generic_enum_instance_key(
    const GenericTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    return this->generic_instance_key(info, args);
}

std::string SemanticAnalyzer::generic_type_alias_instance_key(
    const GenericTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    return this->generic_instance_key(info, args);
}

std::string SemanticAnalyzer::generic_function_instance_key(
    const GenericTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    return this->generic_instance_key(info, args);
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_generic_struct_in_visible_modules(
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range,
    const bool report_unknown
) {
    const ModuleLookupKey lookup_key = this->find_module_lookup_key(this->current_module_, name_id);
    if (is_valid(lookup_key)) {
        if (const auto found = this->generic_struct_templates_by_name_.find(lookup_key);
            found != this->generic_struct_templates_by_name_.end()) {
            return found->second;
        }
    }

    if (report_unknown) {
        this->report_lookup(range, sema_unknown_generic_type_message(name));
    }
    return nullptr;
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_generic_struct_in_module(
    const syntax::ModuleId module,
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range,
    const bool report_unknown
) {
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            this->report_lookup(range, sema_unknown_generic_type_message(name));
        }
        return nullptr;
    }
    const GenericTemplateInfo* result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        const GenericTemplateInfo* candidate = nullptr;
        const ModuleLookupKey lookup_key = this->find_module_lookup_key(candidate_module, name_id);
        if (is_valid(lookup_key)) {
            if (const auto found = this->generic_struct_templates_by_name_.find(lookup_key);
                found != this->generic_struct_templates_by_name_.end()) {
                candidate = found->second;
            }
        }
        if (candidate == nullptr) {
            continue;
        }
        if (!this->can_access(candidate_module, candidate->visibility)) {
            if (candidate_module.value == module.value && report_unknown) {
                this->report_visibility(
                    range,
                    sema_private_generic_type_message(this->module_name(candidate_module), name)
                );
                return nullptr;
            }
            continue;
        }
        if (result != nullptr) {
            this->report_lookup(
                range,
                sema_ambiguous_generic_type_name_message(
                    name,
                    this->module_name(result_module),
                    this->module_name(candidate_module)
                )
            );
            return nullptr;
        }
        result = candidate;
        result_module = candidate_module;
    }
    if (result == nullptr && report_unknown) {
        this->report_lookup(range, sema_unknown_generic_type_in_module_message(this->module_name(module), name));
    }
    return result;
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_generic_enum_in_visible_modules(
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range,
    const bool report_unknown
) {
    const ModuleLookupKey lookup_key = this->find_module_lookup_key(this->current_module_, name_id);
    if (is_valid(lookup_key)) {
        if (const auto found = this->generic_enum_templates_by_name_.find(lookup_key);
            found != this->generic_enum_templates_by_name_.end()) {
            return found->second;
        }
    }

    if (report_unknown) {
        this->report_lookup(range, sema_unknown_generic_type_message(name));
    }
    return nullptr;
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_generic_enum_in_module(
    const syntax::ModuleId module,
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range,
    const bool report_unknown
) {
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            this->report_lookup(range, sema_unknown_generic_type_message(name));
        }
        return nullptr;
    }
    const GenericTemplateInfo* result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        const GenericTemplateInfo* candidate = nullptr;
        const ModuleLookupKey lookup_key = this->find_module_lookup_key(candidate_module, name_id);
        if (is_valid(lookup_key)) {
            if (const auto found = this->generic_enum_templates_by_name_.find(lookup_key);
                found != this->generic_enum_templates_by_name_.end()) {
                candidate = found->second;
            }
        }
        if (candidate == nullptr) {
            continue;
        }
        if (!this->can_access(candidate_module, candidate->visibility)) {
            if (candidate_module.value == module.value && report_unknown) {
                this->report_visibility(
                    range,
                    sema_private_generic_type_message(this->module_name(candidate_module), name)
                );
                return nullptr;
            }
            continue;
        }
        if (result != nullptr) {
            this->report_lookup(
                range,
                sema_ambiguous_generic_type_name_message(
                    name,
                    this->module_name(result_module),
                    this->module_name(candidate_module)
                )
            );
            return nullptr;
        }
        result = candidate;
        result_module = candidate_module;
    }
    if (result == nullptr && report_unknown) {
        this->report_lookup(range, sema_unknown_generic_type_in_module_message(this->module_name(module), name));
    }
    return result;
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_generic_type_alias_in_visible_modules(
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range,
    const bool report_unknown
) {
    const ModuleLookupKey lookup_key = this->find_module_lookup_key(this->current_module_, name_id);
    if (is_valid(lookup_key)) {
        if (const auto found = this->generic_type_alias_templates_by_name_.find(lookup_key);
            found != this->generic_type_alias_templates_by_name_.end()) {
            return found->second;
        }
    }

    if (report_unknown) {
        this->report_lookup(range, sema_unknown_generic_type_message(name));
    }
    return nullptr;
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_generic_type_alias_in_module(
    const syntax::ModuleId module,
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range,
    const bool report_unknown
) {
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            this->report_lookup(range, sema_unknown_generic_type_message(name));
        }
        return nullptr;
    }
    const GenericTemplateInfo* result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        const GenericTemplateInfo* candidate = nullptr;
        const ModuleLookupKey lookup_key = this->find_module_lookup_key(candidate_module, name_id);
        if (is_valid(lookup_key)) {
            if (const auto found = this->generic_type_alias_templates_by_name_.find(lookup_key);
                found != this->generic_type_alias_templates_by_name_.end()) {
                candidate = found->second;
            }
        }
        if (candidate == nullptr) {
            continue;
        }
        if (!this->can_access(candidate_module, candidate->visibility)) {
            if (candidate_module.value == module.value && report_unknown) {
                this->report_visibility(
                    range,
                    sema_private_generic_type_message(this->module_name(candidate_module), name)
                );
                return nullptr;
            }
            continue;
        }
        if (result != nullptr) {
            this->report_lookup(
                range,
                sema_ambiguous_generic_type_name_message(
                    name,
                    this->module_name(result_module),
                    this->module_name(candidate_module)
                )
            );
            return nullptr;
        }
        result = candidate;
        result_module = candidate_module;
    }
    if (result == nullptr && report_unknown) {
        this->report_lookup(range, sema_unknown_generic_type_in_module_message(this->module_name(module), name));
    }
    return result;
}

bool SemanticAnalyzer::generic_type_template_exists_in_module(
    const syntax::ModuleId module,
    const IdentId name_id,
    const std::string_view name
) const {
    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        if (const GenericTemplateInfo* const found =
                this->find_any_generic_type_template_in_module(candidate_module, name_id, name);
            found != nullptr && this->can_access(candidate_module, found->visibility)) {
            return true;
        }
    }
    return false;
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_any_generic_type_template_in_module(
    const syntax::ModuleId module,
    const IdentId name_id,
    const std::string_view name
) const {
    if (!syntax::is_valid(module)) {
        return nullptr;
    }
    const ModuleLookupKey lookup_key = this->find_module_lookup_key(module, name_id);
    if (is_valid(lookup_key)) {
        if (const auto found = this->generic_struct_templates_by_name_.find(lookup_key);
            found != this->generic_struct_templates_by_name_.end()) {
            return found->second;
        }
        if (const auto found = this->generic_enum_templates_by_name_.find(lookup_key);
            found != this->generic_enum_templates_by_name_.end()) {
            return found->second;
        }
        if (const auto found = this->generic_type_alias_templates_by_name_.find(lookup_key);
            found != this->generic_type_alias_templates_by_name_.end()) {
            return found->second;
        }
    }
    static_cast<void>(name);
    return nullptr;
}

bool SemanticAnalyzer::report_generic_type_requires_args_if_visible(
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range
) {
    if (const GenericTemplateInfo* const found =
            this->find_any_generic_type_template_in_module(this->current_module_, name_id, name);
        found != nullptr && this->can_access(this->current_module_, found->visibility)) {
        this->report_type(range, sema_generic_type_requires_args_message(name));
        return true;
    }
    return false;
}

void SemanticAnalyzer::report_generic_type_template_in_module(
    const syntax::ModuleId module,
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range
) {
    if (!syntax::is_valid(module)) {
        this->report_lookup(range, sema_unknown_generic_type_message(name));
        return;
    }

    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        const ModuleLookupKey lookup_key = this->find_module_lookup_key(candidate_module, name_id);
        const bool has_struct_template = is_valid(lookup_key) &&
            this->generic_struct_templates_by_name_.contains(lookup_key);
        const bool has_enum_template = is_valid(lookup_key) &&
            this->generic_enum_templates_by_name_.contains(lookup_key);
        const bool has_alias_template = is_valid(lookup_key) &&
            this->generic_type_alias_templates_by_name_.contains(lookup_key);
        if (has_struct_template) {
            if (const GenericTemplateInfo* info =
                    this->find_any_generic_type_template_in_module(candidate_module, name_id, name);
                info != nullptr && this->can_access(candidate_module, info->visibility)) {
                this->report_type(range, sema_generic_type_requires_args_message(name));
                return;
            }
            static_cast<void>(this->find_generic_struct_in_module(module, name_id, name, range, true));
            return;
        }
        if (has_enum_template) {
            if (const GenericTemplateInfo* info =
                    this->find_any_generic_type_template_in_module(candidate_module, name_id, name);
                info != nullptr && this->can_access(candidate_module, info->visibility)) {
                this->report_type(range, sema_generic_type_requires_args_message(name));
                return;
            }
            static_cast<void>(this->find_generic_enum_in_module(module, name_id, name, range, true));
            return;
        }
        if (has_alias_template) {
            if (const GenericTemplateInfo* info =
                    this->find_any_generic_type_template_in_module(candidate_module, name_id, name);
                info != nullptr && this->can_access(candidate_module, info->visibility)) {
                this->report_type(range, sema_generic_type_requires_args_message(name));
                return;
            }
            static_cast<void>(this->find_generic_type_alias_in_module(module, name_id, name, range, true));
            return;
        }
    }
    this->report_lookup(range, sema_unknown_generic_type_in_module_message(this->module_name(module), name));
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_generic_function_in_visible_modules(
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range,
    const bool report_unknown
) {
    const ModuleLookupKey lookup_key = this->find_module_lookup_key(this->current_module_, name_id);
    if (is_valid(lookup_key)) {
        if (const auto found = this->generic_function_templates_by_name_.find(lookup_key);
            found != this->generic_function_templates_by_name_.end()) {
            return found->second;
        }
    }

    if (report_unknown) {
        this->report_lookup(range, sema_unknown_generic_function_message(name));
    }
    return nullptr;
}

const SemanticAnalyzer::GenericTemplateInfo* SemanticAnalyzer::find_generic_function_in_module(
    const syntax::ModuleId module,
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range,
    const bool report_unknown
) {
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            this->report_lookup(range, sema_unknown_generic_function_message(name));
        }
        return nullptr;
    }
    const GenericTemplateInfo* result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    for (const syntax::ModuleId candidate_module : this->module_export_modules(module)) {
        const GenericTemplateInfo* candidate = nullptr;
        const ModuleLookupKey lookup_key = this->find_module_lookup_key(candidate_module, name_id);
        if (is_valid(lookup_key)) {
            if (const auto found = this->generic_function_templates_by_name_.find(lookup_key);
                found != this->generic_function_templates_by_name_.end()) {
                candidate = found->second;
            }
        }
        if (candidate == nullptr) {
            continue;
        }
        if (!this->can_access(candidate_module, candidate->visibility)) {
            if (candidate_module.value == module.value && report_unknown) {
                this->report_visibility(
                    range,
                    sema_private_generic_function_message(this->module_name(candidate_module), name)
                );
                return nullptr;
            }
            continue;
        }
        if (result != nullptr) {
            this->report_lookup(
                range,
                sema_ambiguous_generic_function_name_message(
                    name,
                    this->module_name(result_module),
                    this->module_name(candidate_module)
                )
            );
            return nullptr;
        }
        result = candidate;
        result_module = candidate_module;
    }
    if (result == nullptr && report_unknown) {
        this->report_lookup(range, sema_unknown_generic_function_in_module_message(this->module_name(module), name));
    }
    return result;
}

TypeHandle SemanticAnalyzer::instantiate_generic_struct(
    const GenericTemplateInfo& info,
    const syntax::TypeNode& use_type,
    const syntax::TypeId,
    const std::vector<TypeHandle>& args
) {
    if (args.size() != info.params.size()) {
        this->report_type(
            use_type.range,
            sema_generic_argument_count_message("generic type arguments", info.name, args.size(), info.params.size())
        );
        return INVALID_TYPE_HANDLE;
    }
    for (const TypeHandle arg : args) {
        if (!is_valid(arg)) {
            return INVALID_TYPE_HANDLE;
        }
    }
    if (!this->validate_generic_arguments(info, args, use_type.range)) {
        return INVALID_TYPE_HANDLE;
    }

    const std::string instance_key = this->generic_struct_instance_key(info, args);
    const IdentId instance_key_id = this->intern_generated_key(instance_key);
    if (const auto found = this->generic_struct_instances_.find(instance_key_id); found != this->generic_struct_instances_.end()) {
        return found->second;
    }

    const syntax::ItemNode item = this->module_.items[info.item.value];
    const std::string abi_suffix = this->generic_instance_abi_suffix(args);
    const std::string qualified = this->qualified_name(info.module, item.name);
    const std::string c_name = this->c_symbol_name(
        info.module,
        std::string(item.name) + abi_suffix
    );

    const TypeHandle handle = this->checked_.types.named_struct(qualified, c_name, false);
    this->checked_.types.set_generic_instance(
        handle,
        this->generic_template_key_prefix(info.module, info.name_id, info.name),
        args
    );
    this->generic_struct_instances_[instance_key_id] = handle;

    StructInfo struct_info = this->checked_.make_struct_info();
    struct_info.name = this->source_name_text(item.name_id, item.name);
    struct_info.c_name = this->checked_.intern_text(c_name);
    struct_info.module = info.module;
    struct_info.type = handle;
    struct_info.visibility = info.visibility;
    struct_info.stable_id = sema::stable_definition_id(
        this->stable_module_id(info.module),
        StableSymbolKind::type,
        this->generic_struct_instance_key(info, args)
    );
    struct_info.incremental_key = this->stable_incremental_key(
        struct_info.stable_id,
        this->generic_instance_key(info, args)
    );
    struct_info.is_generic_placeholder = std::ranges::any_of(args,[&](const TypeHandle arg) {
        return is_valid(arg) && this->checked_.types.get(arg).kind == TypeKind::generic_param;
    });

    GenericContext generic_context = this->make_generic_context();
    this->populate_generic_concrete_context(info, args, generic_context);

    bool contains_array = false;
    {
        GenericAnalysisScope scope(*this, info.module, &generic_context);
        std::unordered_set<IdentId, IdentIdHash> seen_fields;
        for (const syntax::FieldDecl& field : item.fields) {
            if (!seen_fields.insert(field.name_id).second) {
                this->report_duplicate(field.range, sema_duplicate_struct_field_message(field.name));
                continue;
            }
            const TypeHandle field_type = this->resolve_type(field.type);
            if (!this->is_valid_storage_type(field_type)) {
                this->report_general(field.range, std::string(SEMA_FIELD_STORAGE));
            }
            if (this->checked_.types.contains_array(field_type)) {
                contains_array = true;
            }
            struct_info.fields.push_back(StructFieldInfo {
                this->source_name_text(field.name_id, field.name),
                field.name_id,
                {},
                info.module,
                field_type,
                field.range,
                field.visibility,
                this->stable_member_key(
                    struct_info.stable_id,
                    StableSymbolKind::struct_field,
                    field.name_id,
                    field.name
                ),
            });
        }
    }

    this->checked_.types.set_record_contains_array(handle, contains_array);
    auto inserted = this->checked_.structs.emplace(ModuleLookupKey {info.module.value, instance_key_id}, std::move(struct_info));
    if (inserted.second) {
        this->struct_infos_by_type_[handle.value] = &inserted.first->second;
    }
    return handle;
}

TypeHandle SemanticAnalyzer::instantiate_generic_enum(
    const GenericTemplateInfo& info,
    const syntax::TypeNode& use_type,
    const syntax::TypeId,
    const std::vector<TypeHandle>& args
) {
    if (args.size() != info.params.size()) {
        this->report_type(
            use_type.range,
            sema_generic_argument_count_message("generic type arguments", info.name, args.size(), info.params.size())
        );
        return INVALID_TYPE_HANDLE;
    }
    for (const TypeHandle arg : args) {
        if (!is_valid(arg)) {
            return INVALID_TYPE_HANDLE;
        }
    }
    if (!this->validate_generic_arguments(info, args, use_type.range)) {
        return INVALID_TYPE_HANDLE;
    }

    const std::string instance_key = this->generic_enum_instance_key(info, args);
    const IdentId instance_key_id = this->intern_generated_key(instance_key);
    if (const auto found = this->generic_enum_instances_.find(instance_key_id); found != this->generic_enum_instances_.end()) {
        return found->second;
    }

    const syntax::ItemNode item = this->module_.items[info.item.value];
    const std::string abi_suffix = this->generic_instance_abi_suffix(args);
    const std::string qualified = this->qualified_name(info.module, item.name);
    const std::string c_name = this->c_symbol_name(
        info.module,
        std::string(item.name) + abi_suffix
    );

    const TypeHandle handle = this->checked_.types.named_enum(qualified, c_name);
    this->checked_.types.set_generic_instance(
        handle,
        this->generic_template_key_prefix(info.module, info.name_id, info.name),
        args
    );
    this->generic_enum_instances_[instance_key_id] = handle;

    GenericContext generic_context = this->make_generic_context();
    this->populate_generic_concrete_context(info, args, generic_context);

    {
        GenericAnalysisScope scope(*this, info.module, &generic_context);
        this->register_enum_cases_for_item(
            item,
            info.module,
            handle,
            std::string(item.name),
            std::string(item.name) + abi_suffix + "_",
            std::string(item.name) + abi_suffix + "_",
            info.visibility
        );
    }
    return handle;
}

TypeHandle SemanticAnalyzer::instantiate_generic_type_alias(
    const GenericTemplateInfo& info,
    const syntax::TypeNode& use_type,
    const syntax::TypeId,
    const std::vector<TypeHandle>& args,
    const bool opaque_allowed_as_pointee
) {
    if (args.size() != info.params.size()) {
        this->report_type(
            use_type.range,
            sema_generic_argument_count_message("generic type arguments", info.name, args.size(), info.params.size())
        );
        return INVALID_TYPE_HANDLE;
    }
    for (const TypeHandle arg : args) {
        if (!is_valid(arg)) {
            return INVALID_TYPE_HANDLE;
        }
    }
    if (!this->validate_generic_arguments(info, args, use_type.range)) {
        return INVALID_TYPE_HANDLE;
    }

    const std::string instance_key = this->generic_type_alias_instance_key(info, args);
    const IdentId instance_key_id = this->intern_generated_key(instance_key);
    if (const auto found = this->resolved_generic_type_aliases_.find(instance_key_id);
        found != this->resolved_generic_type_aliases_.end()) {
        return found->second;
    }
    if (std::ranges::find(this->resolving_type_aliases_, ModuleLookupKey {info.module.value, instance_key_id}) !=
        this->resolving_type_aliases_.end()) {
        this->report_general(use_type.range, sema_cyclic_type_alias_message(info.name));
        this->resolved_generic_type_aliases_[instance_key_id] = INVALID_TYPE_HANDLE;
        return INVALID_TYPE_HANDLE;
    }

    const syntax::ItemNode item = this->module_.items[info.item.value];
    this->resolving_type_aliases_.push_back(ModuleLookupKey {info.module.value, instance_key_id});
    GenericContext generic_context = this->make_generic_context();
    this->populate_generic_concrete_context(info, args, generic_context);

    const TypeHandle resolved = [&] {
        GenericAnalysisScope scope(*this, info.module, &generic_context);
        return this->resolve_type(item.alias_type, opaque_allowed_as_pointee);
    }();
    this->resolving_type_aliases_.pop_back();
    this->resolved_generic_type_aliases_[instance_key_id] = resolved;
    return resolved;
}

bool SemanticAnalyzer::unify_generic_type(
    const TypeHandle pattern,
    const TypeHandle actual,
    std::unordered_map<GenericParamIdentity, TypeHandle, GenericParamIdentityHash>& inferred
) const {
    if (!is_valid(pattern) || !is_valid(actual)) {
        return false;
    }

    std::vector<std::pair<TypeHandle, TypeHandle>> pending;
    pending.emplace_back(pattern, actual);
    while (!pending.empty()) {
        const auto [current_pattern, current_actual] = pending.back();
        pending.pop_back();
        if (!is_valid(current_pattern) || !is_valid(current_actual)) {
            return false;
        }
        const TypeInfo& pattern_info = this->checked_.types.get(current_pattern);
        if (pattern_info.kind == TypeKind::generic_param) {
            const GenericParamIdentity identity = this->generic_param_identity(pattern_info);
            const auto found = inferred.find(identity);
            if (found == inferred.end()) {
                inferred.emplace(identity, current_actual);
                continue;
            }
            if (!this->checked_.types.same(found->second, current_actual)) {
                return false;
            }
            continue;
        }
        const TypeInfo& actual_info = this->checked_.types.get(current_actual);
        if (pattern_info.kind != actual_info.kind) {
            return false;
        }
        switch (pattern_info.kind) {
        case TypeKind::builtin:
        case TypeKind::opaque_struct:
            if (!this->checked_.types.same(current_pattern, current_actual)) {
                return false;
            }
            break;
        case TypeKind::pointer:
        case TypeKind::reference:
            if (pattern_info.pointer_mutability == PointerMutability::mut &&
                actual_info.pointer_mutability != PointerMutability::mut) {
                return false;
            }
            pending.emplace_back(pattern_info.pointee, actual_info.pointee);
            break;
        case TypeKind::slice:
            if (pattern_info.slice_mutability == PointerMutability::mut &&
                actual_info.slice_mutability != PointerMutability::mut) {
                return false;
            }
            pending.emplace_back(pattern_info.slice_element, actual_info.slice_element);
            break;
        case TypeKind::function:
            if (pattern_info.function_call_conv != actual_info.function_call_conv ||
                pattern_info.function_is_unsafe != actual_info.function_is_unsafe ||
                pattern_info.function_is_variadic != actual_info.function_is_variadic ||
                pattern_info.function_params.size() != actual_info.function_params.size()) {
                return false;
            }
            pending.emplace_back(pattern_info.function_return, actual_info.function_return);
            for (base::usize i = 0; i < pattern_info.function_params.size(); ++i) {
                pending.emplace_back(pattern_info.function_params[i], actual_info.function_params[i]);
            }
            break;
        case TypeKind::tuple:
            if (pattern_info.tuple_elements.size() != actual_info.tuple_elements.size()) {
                return false;
            }
            for (base::usize i = 0; i < pattern_info.tuple_elements.size(); ++i) {
                pending.emplace_back(pattern_info.tuple_elements[i], actual_info.tuple_elements[i]);
            }
            break;
        case TypeKind::array:
            if (pattern_info.array_count != actual_info.array_count) {
                return false;
            }
            pending.emplace_back(pattern_info.array_element, actual_info.array_element);
            break;
        case TypeKind::struct_:
        case TypeKind::enum_:
            if (pattern_info.generic_origin_key.empty() ||
                pattern_info.generic_origin_key != actual_info.generic_origin_key ||
                pattern_info.generic_args.size() != actual_info.generic_args.size()) {
                if (!this->checked_.types.same(current_pattern, current_actual)) {
                    return false;
                }
                break;
            }
            for (base::usize i = 0; i < pattern_info.generic_args.size(); ++i) {
                pending.emplace_back(pattern_info.generic_args[i], actual_info.generic_args[i]);
            }
            break;
        case TypeKind::generic_param:
            return false;
        }
    }
    return true;
}

bool SemanticAnalyzer::infer_generic_arguments(
    const GenericTemplateInfo& info,
    const SemanticAnalyzer::ExprView& call,
    std::vector<TypeHandle>& args
) {
    const syntax::ItemNode function = this->module_.items[info.item.value];
    if (call.args.size() != function.params.size()) {
        this->report_type(call.range, sema_argument_count_message(info.name));
        return false;
    }

    GenericContext generic_context = this->make_generic_context();
    this->populate_generic_placeholder_context(info, generic_context);

    std::vector<TypeHandle> pattern_param_types;
    pattern_param_types.reserve(function.params.size());
    {
        GenericAnalysisScope scope(*this, info.module, &generic_context);
        for (const syntax::ParamDecl& param : function.params) {
            pattern_param_types.push_back(this->resolve_type(param.type));
        }
    }

    std::unordered_map<GenericParamIdentity, TypeHandle, GenericParamIdentityHash> inferred;
    for (base::usize i = 0; i < call.args.size(); ++i) {
        const TypeHandle actual = this->analyze_expr(call.args[i], pattern_param_types[i]);
        if (!this->unify_generic_type(pattern_param_types[i], actual, inferred)) {
            this->report_type(
                this->module_.exprs.range(call.args[i].value),
                sema_generic_call_argument_unify_message(info.name)
            );
            return false;
        }
    }

    args.clear();
    args.reserve(info.params.size());
    for (base::usize i = 0; i < info.params.size(); ++i) {
        const auto found = inferred.find(this->generic_param_identity(info, i));
        if (found == inferred.end() || !is_valid(found->second)) {
            this->report_type(
                call.range,
                sema_generic_call_argument_infer_message(this->generic_param_name(info, i), info.name)
            );
            return false;
        }
        args.push_back(found->second);
    }
    if (!this->validate_generic_arguments(info, args, call.range)) {
        return false;
    }
    return true;
}

FunctionSignature* SemanticAnalyzer::instantiate_generic_placeholder_function(
    const GenericTemplateInfo& info,
    const std::vector<TypeHandle>& args,
    const base::SourceRange& use_range
) {
    if (args.size() != info.params.size()) {
        this->report_type(
            use_range,
            sema_generic_argument_count_message("generic function type arguments", info.name, args.size(), info.params.size())
        );
        return nullptr;
    }
    const syntax::ItemNode function = this->module_.items[info.item.value];

    for (base::usize i = 0; i < info.params.size(); ++i) {
        if (!is_valid(args[i])) {
            return nullptr;
        }
    }

    GenericContext generic_context = this->make_generic_context();
    this->populate_generic_concrete_context(info, args, generic_context);

    FunctionSignature signature = this->checked_.make_function_signature();
    {
        GenericAnalysisScope scope(*this, info.module, &generic_context);
        signature.name = this->source_name_text(info.name_id, info.name);
        signature.name_id = info.name_id;
        signature.c_name = signature.name;
        signature.stable_id = info.stable_id;
        signature.generic_args = this->checked_.copy_type_handle_list(args);
        signature.module = info.module;
        signature.return_type = syntax::is_valid(function.return_type)
            ? this->resolve_type(function.return_type)
            : INVALID_TYPE_HANDLE;
        signature.range = function.range;
        signature.is_unsafe = function.is_unsafe;
        signature.has_definition = true;
        signature.visibility = info.visibility;
        signature.definition_item = info.item;
        for (const syntax::ParamDecl& param : function.params) {
            signature.param_types.push_back(this->resolve_type(param.type));
        }
        signature.incremental_key = this->stable_incremental_key(
            signature.stable_id,
            this->function_incremental_fingerprint(
                info.name,
                signature.return_type,
                signature.param_types,
                false,
                signature.is_variadic
            )
        );
    }

    const IdentId key_id = this->intern_generated_key(this->generic_function_instance_key(info, args));
    const FunctionLookupKey key = this->function_lookup_key(info.module, key_id);
    signature.semantic_key = key;
    const auto inserted = this->generic_placeholder_functions_.emplace(key, std::move(signature));
    return &inserted.first->second;
}

bool SemanticAnalyzer::type_contains_generic_param(const TypeHandle type) const {
    if (!is_valid(type)) {
        return false;
    }
    std::vector<TypeHandle> pending;
    pending.push_back(type);
    while (!pending.empty()) {
        const TypeHandle current = pending.back();
        pending.pop_back();
        if (!is_valid(current)) {
            continue;
        }
        const TypeInfo& info = this->checked_.types.get(current);
        switch (info.kind) {
        case TypeKind::generic_param:
            return true;
        case TypeKind::pointer:
        case TypeKind::reference:
            pending.push_back(info.pointee);
            break;
        case TypeKind::slice:
            pending.push_back(info.slice_element);
            break;
        case TypeKind::function:
            pending.push_back(info.function_return);
            for (const TypeHandle param : info.function_params) {
                pending.push_back(param);
            }
            break;
        case TypeKind::tuple:
            for (const TypeHandle element : info.tuple_elements) {
                pending.push_back(element);
            }
            break;
        case TypeKind::array:
            pending.push_back(info.array_element);
            break;
        case TypeKind::struct_:
        case TypeKind::enum_:
            for (const TypeHandle arg : info.generic_args) {
                pending.push_back(arg);
            }
            break;
        case TypeKind::builtin:
        case TypeKind::opaque_struct:
            break;
        }
    }
    return false;
}

FunctionSignature* SemanticAnalyzer::instantiate_generic_function(
    const GenericTemplateInfo& info,
    const std::vector<TypeHandle>& args,
    const base::SourceRange& use_range
) {
    if (args.size() != info.params.size()) {
        this->report_type(
            use_range,
            sema_generic_argument_count_message("generic function type arguments", info.name, args.size(), info.params.size())
        );
        return nullptr;
    }
    for (const TypeHandle arg : args) {
        if (!is_valid(arg)) {
            return nullptr;
        }
    }
    if (!this->validate_generic_arguments(info, args, use_range)) {
        return nullptr;
    }
    if (std::ranges::any_of(args, [&](const TypeHandle arg) {
            return this->type_contains_generic_param(arg);
        })) {
        return this->instantiate_generic_placeholder_function(info, args, use_range);
    }
    const IdentId key_id = this->intern_generated_key(this->generic_function_instance_key(info, args));
    const FunctionLookupKey key = this->function_lookup_key(info.module, key_id);
    if (const auto found = this->generic_function_instances_.find(key);
        found != this->generic_function_instances_.end()) {
        return &this->checked_.generic_function_instances[found->second].signature;
    }
    if (!this->options_.retain_generic_side_tables) {
        if (const auto found = this->checked_.functions.find(key); found != this->checked_.functions.end()) {
            return &found->second;
        }
    }
    const syntax::ItemNode function = this->module_.items[info.item.value];

    GenericContext generic_context = this->make_generic_context();
    this->populate_generic_concrete_context(info, args, generic_context);

    FunctionSignature signature = this->checked_.make_function_signature();
    {
        GenericAnalysisScope scope(*this, info.module, &generic_context);
        signature.name = this->source_name_text(info.name_id, info.name);
        signature.name_id = info.name_id;
        signature.semantic_key = key;
        signature.stable_id = sema::stable_definition_id(
            this->stable_module_id(info.module),
            StableSymbolKind::function,
            this->generic_function_instance_key(info, args)
        );
        signature.c_name = this->checked_.intern_text(this->c_symbol_name(
            info.module,
            std::string(info.name.view()) + this->generic_instance_abi_suffix(args)
        ));
        signature.generic_args = this->checked_.copy_type_handle_list(args);
        signature.module = info.module;
        signature.return_type = syntax::is_valid(function.return_type)
            ? this->resolve_type(function.return_type)
            : INVALID_TYPE_HANDLE;
        signature.range = function.range;
        signature.is_unsafe = function.is_unsafe;
        signature.has_definition = true;
        signature.visibility = info.visibility;
        signature.definition_item = info.item;
        for (const syntax::ParamDecl& param : function.params) {
            signature.param_types.push_back(this->resolve_type(param.type));
        }
        signature.incremental_key = this->stable_incremental_key(
            signature.stable_id,
            this->function_incremental_fingerprint(
                info.name,
                signature.return_type,
                signature.param_types,
                false,
                signature.is_variadic
            )
        );
    }

    if (syntax::is_valid(function.return_type) && is_valid(signature.return_type)) {
        this->validate_function_return_type(function, signature.return_type);
    }

    if (!this->options_.retain_generic_side_tables) {
        const auto function_inserted = this->checked_.functions.emplace(key, signature);
        if (!function_inserted.second) {
            function_inserted.first->second = signature;
        } else {
            this->internal_function_lookup_exclusions_ += 1;
        }
        this->function_body_states_[key] = FunctionBodyState::not_started;
        GenericSideTables transient_side_tables = make_generic_side_tables(info);
        GenericContext body_context = this->make_generic_context();
        this->populate_generic_concrete_context(info, args, body_context);
        {
            GenericAnalysisScope scope(*this, info.module, &body_context, &transient_side_tables);
            this->analyze_function_body_with_signature(
                function,
                key,
                function_inserted.first->second,
                this->function_body_states_[key]
            );
        }
        return &this->checked_.functions.at(key);
    }

    GenericFunctionInstanceInfo instance;
    instance.key = key;
    instance.item = info.item;
    instance.signature = std::move(signature);
    if (info.has_sparse_node_ids()) {
        instance.side_table_layout_index = this->generic_side_table_layout_index(info);
    }
    instance.side_tables = this->make_generic_instance_side_tables(info);
    const base::usize instance_index = this->checked_.generic_function_instances.size();
    this->checked_.generic_function_instances.push_back(std::move(instance));
    if (const GenericSideTableLayout* const layout = this->checked_.generic_side_table_layout(
            this->checked_.generic_function_instances[instance_index].side_table_layout_index
        );
        layout != nullptr) {
        this->checked_.generic_function_instances[instance_index].side_tables.bind_local_dense_layout(*layout);
    }
    this->generic_function_instances_[key] = instance_index;

    FunctionSignature checked_signature = this->checked_.generic_function_instances[instance_index].signature;
    const auto function_inserted = this->checked_.functions.emplace(key, checked_signature);
    if (!function_inserted.second) {
        function_inserted.first->second = checked_signature;
    } else {
        this->internal_function_lookup_exclusions_ += 1;
    }
    this->function_body_states_[key] = FunctionBodyState::not_started;
    GenericContext body_context = this->make_generic_context();
    this->populate_generic_concrete_context(info, args, body_context);
    {
        GenericAnalysisScope scope(
            *this,
            info.module,
            &body_context,
            &this->checked_.generic_function_instances[instance_index].side_tables
        );
        this->analyze_function_body_with_signature(
            function,
            key,
            this->checked_.generic_function_instances[instance_index].signature,
            this->function_body_states_[key]
        );
    }
    this->checked_.generic_function_instances[instance_index].signature = this->checked_.functions.at(key);
    this->checked_.generic_function_instances[instance_index].side_tables.release_analysis_only_storage();
    return &this->checked_.generic_function_instances[instance_index].signature;
}

FunctionSignature* SemanticAnalyzer::instantiate_generic_method(
    const GenericTemplateInfo& info,
    const TypeHandle owner_type,
    const std::vector<TypeHandle>& args,
    const base::SourceRange& use_range
) {
    if (args.size() != info.params.size()) {
        this->report_type(
            use_range,
            sema_generic_argument_count_message("generic method type arguments", info.name, args.size(), info.params.size())
        );
        return nullptr;
    }
    for (const TypeHandle arg : args) {
        if (!is_valid(arg)) {
            return nullptr;
        }
    }
    if (!this->validate_generic_arguments(info, args, use_range)) {
        return nullptr;
    }

    const FunctionLookupKey key = this->method_function_lookup_key(info.module, owner_type, info.name_id);
    if (const auto found = this->checked_.functions.find(key); found != this->checked_.functions.end()) {
        return &found->second;
    }

    const syntax::ItemNode function = this->module_.items[info.item.value];
    GenericContext generic_context = this->make_generic_context();
    this->populate_generic_concrete_context(info, args, generic_context);

    FunctionSignature signature = this->checked_.make_function_signature();
    {
        GenericAnalysisScope scope(*this, info.module, &generic_context);
        signature.name = this->source_name_text(info.name_id, info.name);
        signature.name_id = info.name_id;
        signature.semantic_key = key;
        signature.stable_id = sema::stable_definition_id(
            this->stable_module_id(info.module),
            StableSymbolKind::method,
            this->checked_.types.display_name(owner_type) + "." + std::string(info.name.view())
        );
        signature.c_name = this->checked_.intern_text(this->method_c_symbol_name(owner_type, info.name));
        signature.generic_args = this->checked_.copy_type_handle_list(args);
        signature.module = info.module;
        signature.method_owner_type = owner_type;
        signature.return_type = syntax::is_valid(function.return_type)
            ? this->resolve_type(function.return_type)
            : INVALID_TYPE_HANDLE;
        signature.range = function.range;
        signature.is_unsafe = function.is_unsafe;
        signature.has_definition = true;
        signature.is_method = true;
        signature.has_self_param = !function.params.empty() && function.params.front().name == "self";
        signature.visibility = info.visibility;
        signature.definition_item = info.item;
        for (const syntax::ParamDecl& param : function.params) {
            signature.param_types.push_back(this->resolve_type(param.type));
        }
        signature.incremental_key = this->stable_incremental_key(
            signature.stable_id,
            this->function_incremental_fingerprint(
                info.name,
                signature.return_type,
                signature.param_types,
                true,
                signature.is_variadic
            )
        );
    }

    if (syntax::is_valid(function.return_type) && is_valid(signature.return_type)) {
        this->validate_function_return_type(function, signature.return_type);
    }

    if (!this->options_.retain_generic_side_tables) {
        const auto function_inserted = this->checked_.functions.emplace(key, signature);
        if (!function_inserted.second) {
            function_inserted.first->second = signature;
        }
        this->index_method_lookup(info.module, owner_type, info.name_id, function_inserted.first->second);
        this->index_function_value(function_inserted.first->second);
        this->function_body_states_[key] = FunctionBodyState::not_started;

        GenericSideTables transient_side_tables = make_generic_side_tables(info);
        GenericContext body_context = this->make_generic_context();
        this->populate_generic_concrete_context(info, args, body_context);
        {
            GenericAnalysisScope scope(*this, info.module, &body_context, &transient_side_tables);
            this->analyze_function_body_with_signature(
                function,
                key,
                function_inserted.first->second,
                this->function_body_states_[key]
            );
        }
        return &this->checked_.functions.at(key);
    }

    GenericFunctionInstanceInfo instance;
    instance.key = key;
    instance.item = info.item;
    instance.signature = std::move(signature);
    if (info.has_sparse_node_ids()) {
        instance.side_table_layout_index = this->generic_side_table_layout_index(info);
    }
    instance.side_tables = this->make_generic_instance_side_tables(info);
    const base::usize instance_index = this->checked_.generic_function_instances.size();
    this->checked_.generic_function_instances.push_back(std::move(instance));
    if (const GenericSideTableLayout* const layout = this->checked_.generic_side_table_layout(
            this->checked_.generic_function_instances[instance_index].side_table_layout_index
        );
        layout != nullptr) {
        this->checked_.generic_function_instances[instance_index].side_tables.bind_local_dense_layout(*layout);
    }
    this->generic_function_instances_[key] = instance_index;

    FunctionSignature checked_signature = this->checked_.generic_function_instances[instance_index].signature;
    const auto function_inserted = this->checked_.functions.emplace(key, checked_signature);
    if (!function_inserted.second) {
        function_inserted.first->second = checked_signature;
    }
    this->index_method_lookup(info.module, owner_type, info.name_id, function_inserted.first->second);
    this->index_function_value(function_inserted.first->second);
    this->function_body_states_[key] = FunctionBodyState::not_started;

    GenericContext body_context = this->make_generic_context();
    this->populate_generic_concrete_context(info, args, body_context);
    {
        GenericAnalysisScope scope(
            *this,
            info.module,
            &body_context,
            &this->checked_.generic_function_instances[instance_index].side_tables
        );
        this->analyze_function_body_with_signature(
            function,
            key,
            this->checked_.generic_function_instances[instance_index].signature,
            this->function_body_states_[key]
        );
    }
    this->checked_.generic_function_instances[instance_index].signature = this->checked_.functions.at(key);
    this->checked_.generic_function_instances[instance_index].side_tables.release_analysis_only_storage();
    return &this->checked_.functions.at(key);
}

FunctionSignature* SemanticAnalyzer::find_generic_method_in_visible_modules(
    const TypeHandle owner_type,
    const IdentId name_id,
    const std::string_view name,
    const base::SourceRange& range,
    const bool require_self,
    const bool report_unknown
) {
    FunctionSignature* result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    const std::array<syntax::ModuleId, 2> modules {this->current_module_, this->owner_module(owner_type)};
    std::unordered_set<base::u32> seen_modules;
    for (const syntax::ModuleId module : modules) {
        if (!syntax::is_valid(module)) {
            continue;
        }
        if (!seen_modules.insert(module.value).second) {
            continue;
        }
        std::vector<const GenericTemplateInfo*> candidates;
        const auto append_candidate = [&candidates](const GenericTemplateInfo* const info) {
            if (std::ranges::find(candidates, info) == candidates.end()) {
                candidates.push_back(info);
            }
        };
        const ModuleLookupKey lookup_key = this->find_module_lookup_key(module, name_id);
        if (is_valid(lookup_key)) {
            if (const auto found = this->generic_method_templates_by_name_.find(lookup_key);
                found != this->generic_method_templates_by_name_.end()) {
                candidates.reserve(found->second.size());
                for (const GenericTemplateInfo* const info : found->second) {
                    append_candidate(info);
                }
            }
        }
        for (const GenericTemplateInfo* const candidate_info : candidates) {
            if (candidate_info == nullptr) {
                continue;
            }
            const GenericTemplateInfo& info = *candidate_info;
            if (!this->can_access(module, info.visibility)) {
                continue;
            }
            std::unordered_map<GenericParamIdentity, TypeHandle, GenericParamIdentityHash> inferred;
            if (!this->unify_generic_type(info.impl_type_pattern, owner_type, inferred)) {
                continue;
            }
            std::vector<TypeHandle> args;
            args.reserve(info.params.size());
            bool all_inferred = true;
            for (base::usize i = 0; i < info.params.size(); ++i) {
                const auto found = inferred.find(this->generic_param_identity(info, i));
                if (found == inferred.end() || !is_valid(found->second)) {
                    all_inferred = false;
                    break;
                }
                args.push_back(found->second);
            }
            if (!all_inferred) {
                continue;
            }
            FunctionSignature* candidate = this->instantiate_generic_method(info, owner_type, args, range);
            if (candidate == nullptr || (require_self && !candidate->has_self_param)) {
                continue;
            }
            if (result != nullptr) {
                this->report_lookup(
                    range,
                    sema_ambiguous_method_message(
                        this->checked_.types.display_name(owner_type),
                        name,
                        this->module_name(result_module),
                        this->module_name(module)
                    )
                );
                return nullptr;
            }
            result = candidate;
            result_module = module;
        }
    }
    if (result == nullptr && report_unknown) {
        this->report_lookup(range, sema_unknown_method_message(this->checked_.types.display_name(owner_type), name));
    }
    return result;
}

void SemanticAnalyzer::analyze_generic_function_definition(const GenericTemplateInfo& info) {
    const syntax::ItemNode function = this->module_.items[info.item.value];
    GenericContext generic_context = this->make_generic_context();
    this->populate_generic_placeholder_context(info, generic_context);

    FunctionSignature signature = this->checked_.make_function_signature();
    {
        GenericAnalysisScope scope(*this, info.module, &generic_context);
        signature.name = this->source_name_text(info.name_id, info.name);
        signature.name_id = info.name_id;
        signature.semantic_key = info.function_key;
        signature.stable_id = info.stable_id;
        signature.c_name = signature.name;
        signature.module = info.module;
        signature.return_type = syntax::is_valid(function.return_type)
            ? this->resolve_type(function.return_type)
            : INVALID_TYPE_HANDLE;
        signature.range = function.range;
        signature.is_unsafe = function.is_unsafe;
        signature.has_definition = true;
        signature.visibility = info.visibility;
        for (const syntax::ParamDecl& param : function.params) {
            signature.param_types.push_back(this->resolve_type(param.type));
        }
        signature.incremental_key = this->stable_incremental_key(
            signature.stable_id,
            this->function_incremental_fingerprint(
                info.name,
                signature.return_type,
                signature.param_types,
                false,
                signature.is_variadic
            )
        );
        const auto placeholder_inserted = this->generic_placeholder_functions_.emplace(info.function_key, signature);
        if (!placeholder_inserted.second) {
            placeholder_inserted.first->second = signature;
        }
    }

    FunctionBodyState state = FunctionBodyState::not_started;
    this->analyze_generic_function_body(function, info, signature, state);
}

void SemanticAnalyzer::analyze_generic_function_body(
    const syntax::ItemNode& function,
    const GenericTemplateInfo& info,
    const FunctionSignature& signature,
    FunctionBodyState& state
) {
    GenericContext generic_context = this->make_generic_context();
    this->populate_generic_placeholder_context(info, generic_context);
    GenericSideTables side_tables = make_generic_side_tables(info);
    GenericAnalysisScope scope(*this, info.module, &generic_context, &side_tables);
    this->analyze_function_body_with_signature(function, info.function_key, signature, state);
}

} // namespace aurex::sema
