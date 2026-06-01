#include <aurex/sema/resource_semantics.hpp>
#include <aurex/sema/sema_messages.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <sema/internal/sema_generic_analyzer.hpp>

namespace aurex::sema {

SemanticAnalyzerCore::GenericAnalyzer::GenericAnalyzer(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

namespace {

constexpr std::string_view SEMA_GENERIC_INSTANCE_SEPARATOR = "$";
constexpr std::string_view SEMA_GENERIC_KEY_ARG_PREFIX = "t";
constexpr std::string_view SEMA_GENERIC_KEY_ARG_SEPARATOR = ".";
constexpr std::string_view SEMA_GENERIC_ABI_SUFFIX_PREFIX = "__aurexg";
constexpr std::string_view SEMA_GENERIC_ABI_GLOBAL_ID_PREFIX = "_k";
constexpr std::string_view SEMA_GENERIC_ABI_PRIMARY_PREFIX = "_p";
constexpr std::string_view SEMA_GENERIC_ABI_SECONDARY_PREFIX = "_s";
constexpr std::string_view SEMA_GENERIC_ABI_BYTE_COUNT_PREFIX = "_n";
constexpr std::string_view SEMA_GENERIC_PREDICATE_ID_CONTEXT = "semantic generic trait predicate id";
constexpr std::string_view SEMA_GENERIC_OBLIGATION_ID_CONTEXT = "semantic generic trait obligation id";
constexpr std::string_view SEMA_GENERIC_PARAM_ENV_ID_CONTEXT = "semantic generic param env id";
constexpr std::string_view SEMA_CAPABILITY_DROP = "Drop";
constexpr std::string_view SEMA_GENERIC_PARAM_IDENTITY_MARKER = "generic-param";
constexpr std::string_view SEMA_GENERIC_TEMPLATE_INCREMENTAL_TAG = "|generic_template";
constexpr std::string_view SEMA_GENERIC_TEMPLATE_PARAM_TAG = "|param:";
constexpr std::string_view SEMA_GENERIC_TEMPLATE_CONSTRAINT_TAG = "|constraint:";
constexpr std::string_view SEMA_GENERIC_TEMPLATE_PREDICATE_TAG = "|predicate:";
constexpr std::string_view SEMA_GENERIC_TEMPLATE_FIELD_SEPARATOR = ":";
constexpr base::u64 SEMA_TRAIT_GENERIC_PREDICATE_KEY_MARKER = 0x53454d4154525052ULL;
constexpr base::u8 SEMA_TRAIT_PREDICATE_KIND_BUILTIN = 1;
constexpr base::u8 SEMA_TRAIT_PREDICATE_KIND_DECLARED = 2;
constexpr base::u64 SEMA_GENERIC_PARAM_IDENTITY_OFFSET = 14695981039346656037ULL;
constexpr base::u64 SEMA_GENERIC_PARAM_IDENTITY_PRIME = 1099511628211ULL;
constexpr base::u32 SEMA_GENERIC_PARAM_IDENTITY_U64_BITS = 64;
constexpr base::u32 SEMA_GENERIC_PARAM_IDENTITY_BYTE_BITS = 8;
constexpr base::u64 SEMA_GENERIC_PARAM_IDENTITY_BYTE_MASK = 0xFFU;
constexpr base::usize SEMA_DECIMAL_U64_MAX_DIGITS = 20;
constexpr base::usize SEMA_GENERIC_KEY_ARG_SIZE_ESTIMATE = 12;
constexpr base::usize SEMA_GENERIC_ABI_SUFFIX_SIZE_ESTIMATE = 96;
constexpr base::usize SEMA_GENERIC_SPAN_LINEAR_DEDUP_LIMIT = 64;

[[nodiscard]] GenericSideTables make_generic_side_tables(const GenericSideTableLocalLayoutView& layout)
{
    GenericSideTables side_tables;
    side_tables.configure_local_dense(layout);
    return side_tables;
}

template <typename Info>
[[nodiscard]] GenericSideTables make_generic_side_tables(const Info& info)
{
    return make_generic_side_tables(GenericSideTableLocalLayoutView{
        info.expr_span,
        info.pattern_span,
        info.type_span,
        info.stmt_span,
        info.expr_node_ids,
        info.pattern_node_ids,
        info.type_node_ids,
        info.stmt_node_ids,
    });
}

[[nodiscard]] base::u64 mix_generic_identity_byte(base::u64 hash, const unsigned char byte) noexcept
{
    hash ^= static_cast<base::u64>(byte);
    hash *= SEMA_GENERIC_PARAM_IDENTITY_PRIME;
    return hash;
}

[[nodiscard]] base::u64 mix_generic_identity_text(base::u64 hash, const std::string_view text) noexcept
{
    for (const unsigned char byte : text) {
        hash = mix_generic_identity_byte(hash, byte);
    }
    return hash;
}

[[nodiscard]] base::u64 mix_generic_identity_u64(base::u64 hash, const base::u64 value) noexcept
{
    for (base::u32 shift = 0; shift < SEMA_GENERIC_PARAM_IDENTITY_U64_BITS;
        shift += SEMA_GENERIC_PARAM_IDENTITY_BYTE_BITS) {
        hash = mix_generic_identity_byte(
            hash, static_cast<unsigned char>((value >> shift) & SEMA_GENERIC_PARAM_IDENTITY_BYTE_MASK));
    }
    return hash;
}

[[nodiscard]] GenericParamIdentity finish_generic_param_identity(base::u64 hash) noexcept
{
    if (hash == 0) {
        hash = SEMA_GENERIC_PARAM_IDENTITY_OFFSET;
    }
    return GenericParamIdentity{hash};
}

struct GenericNodeSpanBuilder {
    explicit GenericNodeSpanBuilder(const syntax::AstModule& ast_module) : module(ast_module)
    {
    }

    const syntax::AstModule& module;
    std::vector<syntax::ExprId> exprs;
    std::vector<syntax::PatternId> patterns;
    std::vector<syntax::TypeId> types;
    std::vector<syntax::StmtId> stmts;
    std::unordered_set<base::u32> seen_exprs;
    std::unordered_set<base::u32> seen_patterns;
    std::unordered_set<base::u32> seen_types;
    std::unordered_set<base::u32> seen_stmts;

    void add_type(const syntax::TypeId type)
    {
        this->add_id(type, this->module.types.size(), this->seen_types, this->types);
    }

    void add_expr(const syntax::ExprId expr)
    {
        this->add_id(expr, this->module.exprs.size(), this->seen_exprs, this->exprs);
    }

    void add_pattern(const syntax::PatternId pattern)
    {
        this->add_id(pattern, this->module.patterns.size(), this->seen_patterns, this->patterns);
    }

    void add_stmt(const syntax::StmtId stmt)
    {
        this->add_id(stmt, this->module.stmts.size(), this->seen_stmts, this->stmts);
    }

    void collect(const syntax::ItemNode& item)
    {
        this->reserve_initial_worklists(item);
        for (const syntax::ParamDecl& param : item.params) {
            this->add_type(param.type);
        }
        this->add_type(item.return_type);
        this->add_type(item.impl_type);
        this->add_type(item.trait_type);
        this->add_stmt(item.body);
        this->drain();
    }

    [[nodiscard]] GenericNodeSpan expr_span()
    {
        return make_span(this->exprs);
    }

    [[nodiscard]] GenericNodeSpan pattern_span()
    {
        return make_span(this->patterns);
    }

    [[nodiscard]] GenericNodeSpan type_span()
    {
        return make_span(this->types);
    }

    [[nodiscard]] GenericNodeSpan stmt_span()
    {
        return make_span(this->stmts);
    }

    [[nodiscard]] std::vector<base::u32> expr_ids() const
    {
        return make_sparse_ids(this->exprs);
    }

    [[nodiscard]] std::vector<base::u32> pattern_ids() const
    {
        return make_sparse_ids(this->patterns);
    }

    [[nodiscard]] std::vector<base::u32> type_ids() const
    {
        return make_sparse_ids(this->types);
    }

    [[nodiscard]] std::vector<base::u32> stmt_ids() const
    {
        return make_sparse_ids(this->stmts);
    }

private:
    void reserve_initial_worklists(const syntax::ItemNode& item)
    {
        this->types.reserve(item.params.size() + 2U);
        this->stmts.reserve(syntax::is_valid(item.body) ? 1U : 0U);
        this->exprs.reserve(0U);
        this->patterns.reserve(0U);
    }

    template <typename Id>
    void add_id(const Id id, const base::usize node_count, std::unordered_set<base::u32>& seen, std::vector<Id>& ids)
    {
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
    [[nodiscard]] static std::vector<base::u32> make_sparse_ids(const std::vector<Id>& ids)
    {
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
    [[nodiscard]] static GenericNodeSpan make_span(std::vector<Id>& ids)
    {
        std::ranges::sort(ids, {}, &Id::value);
        const auto last = std::ranges::unique(ids, {}, &Id::value).begin();
        ids.erase(last, ids.end());
        if (ids.empty()) {
            return {};
        }
        const base::u32 begin = ids.front().value;
        const base::u32 end = ids.back().value + 1U;
        return GenericNodeSpan{begin, end - begin};
    }

    void drain()
    {
        base::usize type_cursor = 0;
        base::usize expr_cursor = 0;
        base::usize pattern_cursor = 0;
        base::usize stmt_cursor = 0;
        while (type_cursor < this->types.size() || expr_cursor < this->exprs.size()
            || pattern_cursor < this->patterns.size() || stmt_cursor < this->stmts.size()) {
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

    void visit_type(const syntax::TypeId type_id)
    {
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

    void visit_expr(const syntax::ExprId expr_id)
    {
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
                if (const syntax::GenericApplyExprPayload* const payload =
                        this->module.exprs.generic_apply_payload(expr_id.value);
                    payload != nullptr) {
                    this->add_expr(payload->callee);
                    for (const syntax::TypeId arg : payload->type_args) {
                        this->add_type(arg);
                    }
                }
                break;
            case syntax::ExprKind::unary:
                if (const syntax::UnaryExprPayload* const payload = this->module.exprs.unary_payload(expr_id.value);
                    payload != nullptr) {
                    this->add_expr(payload->operand);
                }
                break;
            case syntax::ExprKind::try_expr:
                if (const syntax::TryExprPayload* const payload = this->module.exprs.try_payload(expr_id.value);
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

    void visit_pattern(const syntax::PatternId pattern_id)
    {
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

    void visit_stmt(const syntax::StmtId stmt_id)
    {
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

[[nodiscard]] std::optional<CapabilityKind> parse_capability_kind(const std::string_view name) noexcept
{
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
    if (name == capability_name(CapabilityKind::copy)) {
        return CapabilityKind::copy;
    }
    return std::nullopt;
}

[[nodiscard]] bool is_resource_capability(const std::string_view name) noexcept
{
    return name == SEMA_CAPABILITY_DROP;
}

void append_decimal(std::string& output, const base::u64 value)
{
    char buffer[SEMA_DECIMAL_U64_MAX_DIGITS]{};
    const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (error == std::errc()) {
        output.append(buffer, end);
    }
}

void append_generic_instance_key_suffix(std::string& output, const std::vector<TypeHandle>& args)
{
    output += SEMA_GENERIC_KEY_ARG_PREFIX;
    for (base::usize i = 0; i < args.size(); ++i) {
        if (i != 0) {
            output += SEMA_GENERIC_KEY_ARG_SEPARATOR;
        }
        append_decimal(output, args[i].value);
    }
}

void assign_node_ids(SemaIndexTable& target, const std::vector<base::u32>& source)
{
    target.assign(source.begin(), source.end());
}

} // namespace

struct SemanticAnalyzerCore::GenericAnalysisScope {
    GenericAnalysisScope(SemanticAnalyzerCore& analyzer, const syntax::ModuleId module,
        GenericContext* const generic_context, GenericSideTables* const side_tables = nullptr,
        const bool cache_syntax_types = false, const syntax::ItemId item = syntax::INVALID_ITEM_ID)
        : analyzer(analyzer), previous_module(analyzer.state_.flow.current_module),
          previous_item(analyzer.state_.flow.current_item),
          previous_generic_context(analyzer.state_.flow.current_generic_context),
          previous_side_tables(analyzer.state_.flow.current_side_tables)
    {
        this->analyzer.state_.flow.current_module = module;
        if (syntax::is_valid(item)) {
            this->analyzer.state_.flow.current_item = item;
        }
        this->analyzer.state_.flow.current_generic_context = generic_context;
        if (side_tables != nullptr) {
            this->analyzer.state_.flow.current_side_tables.side_tables = side_tables;
        }
        this->analyzer.state_.flow.current_side_tables.cache_syntax_types = cache_syntax_types;
    }

    GenericAnalysisScope(const GenericAnalysisScope&) = delete;
    GenericAnalysisScope& operator=(const GenericAnalysisScope&) = delete;

    ~GenericAnalysisScope()
    {
        this->analyzer.state_.flow.current_module = this->previous_module;
        this->analyzer.state_.flow.current_item = this->previous_item;
        this->analyzer.state_.flow.current_generic_context = this->previous_generic_context;
        this->analyzer.state_.flow.current_side_tables = this->previous_side_tables;
    }

    SemanticAnalyzerCore& analyzer;
    syntax::ModuleId previous_module;
    syntax::ItemId previous_item;
    GenericContext* previous_generic_context = nullptr;
    GenericSideTableScope previous_side_tables{};
};

std::string_view capability_name(const CapabilityKind capability) noexcept
{
    switch (capability) {
        case CapabilityKind::sized:
            return "Sized";
        case CapabilityKind::eq:
            return "Eq";
        case CapabilityKind::ord:
            return "Ord";
        case CapabilityKind::hash:
            return "Hash";
        case CapabilityKind::copy:
            return "Copy";
    }
    return "<invalid>";
}

bool SemanticAnalyzerCore::GenericAnalyzer::has_generic_params(const syntax::ItemNode& item) const noexcept
{
    return !item.generic_params.empty();
}

bool SemanticAnalyzerCore::GenericAnalyzer::has_generic_constraints(const syntax::ItemNode& item) const noexcept
{
    return !item.where_constraints.empty();
}

void SemanticAnalyzerCore::GenericAnalyzer::validate_generic_parameter_list(const syntax::ItemNode& item)
{
    std::unordered_map<IdentId, base::SourceRange, IdentIdHash> seen;
    seen.reserve(item.generic_params.size());
    for (const syntax::GenericParamDecl& param : item.generic_params) {
        const auto [first, inserted] = seen.emplace(param.name_id, param.range);
        if (!inserted) {
            this->core_.report_duplicate(param.range, sema_duplicate_generic_parameter_message(param.name));
            this->core_.report_note(
                first->second, SemanticDiagnosticKind::duplicate, sema_first_generic_parameter_message(param.name));
        }
    }
}

query::StableFingerprint128 SemanticAnalyzerCore::generic_trait_predicate_fingerprint(const GenericTemplateInfo& info,
    const base::usize param_index, const TraitPredicateKind kind, const CapabilityKind capability,
    const TraitSignature* const trait) const
{
    query::StableKeyWriter writer;
    writer.write_u64(SEMA_TRAIT_GENERIC_PREDICATE_KEY_MARKER);
    writer.write_u64(info.stable_id.global_id);
    writer.write_u64(static_cast<base::u64>(param_index));
    const GenericParamIdentity identity = param_index < info.param_identities.size()
        ? info.param_identities[param_index]
        : INVALID_GENERIC_PARAM_IDENTITY;
    writer.write_u64(identity.value);
    if (kind == TraitPredicateKind::builtin) {
        writer.write_u8(SEMA_TRAIT_PREDICATE_KIND_BUILTIN);
        writer.write_u8(static_cast<base::u8>(capability));
    } else {
        writer.write_u8(SEMA_TRAIT_PREDICATE_KIND_DECLARED);
        if (trait != nullptr) {
            query::append_stable_key(writer, trait->stable_id);
        }
    }
    return writer.fingerprint();
}

base::u32 SemanticAnalyzerCore::GenericAnalyzer::record_generic_trait_predicate(GenericTemplateInfo& info,
    const syntax::GenericConstraintDecl& constraint, const base::usize param_index, const base::usize capability_index,
    const TraitPredicateKind kind, const CapabilityKind capability, const TraitSignature* const trait)
{
    TraitPredicate predicate = this->core_.state_.checked.make_trait_predicate();
    predicate.index =
        base::checked_u32(this->core_.state_.checked.trait_predicates.size(), SEMA_GENERIC_PREDICATE_ID_CONTEXT);
    predicate.kind = kind;
    predicate.origin = TraitPredicateOrigin::explicit_where;
    predicate.subject_type = this->core_.generic_param_placeholder(info, param_index);
    predicate.subject_param_name_id = constraint.param_name_id;
    predicate.subject_param_identity = param_index < info.param_identities.size() ? info.param_identities[param_index]
                                                                                  : INVALID_GENERIC_PARAM_IDENTITY;
    predicate.subject_param_index = base::checked_u32(param_index, SEMA_GENERIC_PREDICATE_ID_CONTEXT);
    predicate.builtin_capability = capability;
    if (trait != nullptr) {
        predicate.trait_name = this->core_.state_.checked.intern_text(trait->name);
        predicate.trait_name_id = trait->name_id;
        predicate.trait_module = trait->module;
        predicate.trait_stable_id = trait->stable_id;
    } else {
        predicate.trait_name = this->core_.state_.checked.intern_text(capability_name(capability));
    }
    predicate.canonical_fingerprint =
        this->core_.generic_trait_predicate_fingerprint(info, param_index, kind, capability, trait);
    predicate.module = info.module;
    predicate.item = info.item;
    predicate.range = capability_index < constraint.capability_ranges.size()
        ? constraint.capability_ranges[capability_index]
        : constraint.range;
    predicate.part_index = info.part_index;
    const base::u32 predicate_index = predicate.index;
    this->core_.state_.checked.trait_predicates.push_back(std::move(predicate));
    info.predicate_indices.push_back(predicate_index);

    TraitObligation obligation = this->core_.state_.checked.make_trait_obligation();
    obligation.predicate_index = predicate_index;
    obligation.predicate_fingerprint =
        this->core_.state_.checked.trait_predicates[predicate_index].canonical_fingerprint;
    obligation.module = info.module;
    obligation.item = info.item;
    obligation.range = this->core_.state_.checked.trait_predicates[predicate_index].range;
    obligation.part_index = info.part_index;
    const base::u32 obligation_index =
        base::checked_u32(this->core_.state_.checked.trait_obligations.size(), SEMA_GENERIC_OBLIGATION_ID_CONTEXT);
    this->core_.state_.checked.trait_obligations.push_back(obligation);
    info.obligation_indices.push_back(obligation_index);

    TraitEvidence evidence = this->core_.state_.checked.make_trait_evidence();
    evidence.kind = kind == TraitPredicateKind::builtin ? TraitEvidenceKind::builtin : TraitEvidenceKind::param_env;
    evidence.predicate_index = predicate_index;
    evidence.predicate_fingerprint = obligation.predicate_fingerprint;
    evidence.module = info.module;
    evidence.item = info.item;
    evidence.range = obligation.range;
    evidence.part_index = info.part_index;
    this->core_.state_.checked.trait_evidence.push_back(evidence);
    return predicate_index;
}

void SemanticAnalyzerCore::GenericAnalyzer::record_generic_param_env(
    GenericTemplateInfo& info, const syntax::ItemNode& item)
{
    std::vector<std::string> predicates;
    predicates.reserve(info.predicate_indices.size());
    for (const base::u32 predicate_index : info.predicate_indices) {
        if (predicate_index >= this->core_.state_.checked.trait_predicates.size()) {
            continue;
        }
        predicates.push_back(
            query::debug_string(this->core_.state_.checked.trait_predicates[predicate_index].canonical_fingerprint));
    }
    std::ranges::sort(predicates);
    std::vector<std::string_view> predicate_views;
    predicate_views.reserve(predicates.size());
    for (const std::string& predicate : predicates) {
        predicate_views.push_back(predicate);
    }
    info.param_env_key = query::param_env_key(predicate_views);

    ParamEnvInfo param_env = this->core_.state_.checked.make_param_env_info();
    param_env.module = info.module;
    param_env.item = info.item;
    param_env.owner_name = this->core_.state_.checked.intern_text(info.name);
    param_env.owner_name_id = info.name_id;
    param_env.owner_stable_id = info.stable_id;
    param_env.key = info.param_env_key;
    param_env.predicate_indices = this->core_.state_.checked.copy_index_table(info.predicate_indices);
    param_env.range = item.range;
    param_env.part_index = info.part_index;
    info.param_env_index =
        base::checked_u32(this->core_.state_.checked.param_envs.size(), SEMA_GENERIC_PARAM_ENV_ID_CONTEXT);
    this->core_.state_.checked.param_envs.push_back(std::move(param_env));
}

void SemanticAnalyzerCore::GenericAnalyzer::validate_generic_constraints(
    const syntax::ItemNode& item, GenericTemplateInfo& info)
{
    info.constraints.reserve(item.generic_params.size());
    info.predicate_indices.clear();
    info.obligation_indices.clear();
    std::unordered_map<IdentId, base::usize, IdentIdHash> params;
    params.reserve(item.generic_params.size());
    for (base::usize index = 0; index < item.generic_params.size(); ++index) {
        params.emplace(item.generic_params[index].name_id, index);
    }

    GenericContext lookup_context = this->core_.make_generic_context();
    lookup_context.params.reserve(item.generic_params.size());
    lookup_context.param_identities.reserve(item.generic_params.size());
    for (base::usize index = 0; index < item.generic_params.size(); ++index) {
        const syntax::GenericParamDecl& param = item.generic_params[index];
        const GenericParamIdentity identity = index < info.param_identities.size()
            ? info.param_identities[index]
            : this->core_.make_generic_param_identity(info, index);
        lookup_context.params.emplace(param.name_id, this->core_.generic_param_placeholder(info, index));
        lookup_context.param_identities.emplace(param.name_id, identity);
    }
    GenericAnalysisScope scope(this->core_, info.module, &lookup_context, nullptr, false, info.item);
    for (const syntax::GenericConstraintDecl& constraint : item.where_constraints) {
        const auto param = params.find(constraint.param_name_id);
        if (param == params.end()) {
            this->core_.report_lookup(
                constraint.param_range, sema_unknown_generic_constraint_param_message(constraint.param_name));
            continue;
        }
        CapabilitySet& capabilities = this->core_.capability_bucket(info.constraints, constraint.param_name_id);
        const std::vector<syntax::AssociatedTypeConstraintDecl> empty_associated_constraints;
        for (base::usize i = 0; i < constraint.capability_names.size(); ++i) {
            const std::string_view capability_name = constraint.capability_names[i];
            const base::SourceRange capability_range =
                i < constraint.capability_ranges.size() ? constraint.capability_ranges[i] : constraint.range;
            const std::vector<syntax::AssociatedTypeConstraintDecl>& associated_constraints =
                i < constraint.capability_associated_constraints.size()
                ? constraint.capability_associated_constraints[i]
                : empty_associated_constraints;
            if (is_resource_capability(capability_name)) {
                this->core_.report_unsupported(
                    capability_range, std::string(SEMA_GENERIC_RESOURCE_CAPABILITY_UNSUPPORTED));
                continue;
            }
            const std::optional<CapabilityKind> capability = parse_capability_kind(capability_name);
            if (capability.has_value()) {
                if (!capabilities.insert(*capability).second) {
                    this->core_.report_capability(
                        capability_range, sema_duplicate_capability_message(constraint.param_name, capability_name));
                    continue;
                }
                for (const syntax::AssociatedTypeConstraintDecl& associated_constraint : associated_constraints) {
                    this->core_.report_capability(associated_constraint.range,
                        sema_associated_type_constraint_on_builtin_message(
                            capability_name, associated_constraint.name));
                }
                this->record_generic_trait_predicate(
                    info, constraint, param->second, i, TraitPredicateKind::builtin, *capability, nullptr);
                continue;
            }

            const IdentId predicate_name_id =
                i < constraint.capability_name_ids.size() ? constraint.capability_name_ids[i] : INVALID_IDENT_ID;
            const TraitSignature* const trait =
                this->core_.find_trait_in_visible_modules(predicate_name_id, capability_name, capability_range, false);
            if (trait == nullptr) {
                this->core_.report_capability(capability_range, sema_unknown_capability_message(capability_name));
                continue;
            }
            if (!trait->generic_params.empty()) {
                this->core_.report_type(capability_range,
                    sema_generic_argument_count_message(
                        "trait predicate type arguments", trait->name, 0, trait->generic_params.size()));
                continue;
            }
            const bool duplicate_trait =
                std::ranges::any_of(info.predicate_indices, [&](const base::u32 predicate_index) {
                    if (predicate_index >= this->core_.state_.checked.trait_predicates.size()) {
                        return false;
                    }
                    const TraitPredicate& predicate = this->core_.state_.checked.trait_predicates[predicate_index];
                    return predicate.kind == TraitPredicateKind::declared_trait
                        && predicate.subject_param_name_id == constraint.param_name_id
                        && predicate.trait_stable_id == trait->stable_id;
                });
            if (duplicate_trait) {
                this->core_.report_capability(
                    capability_range, sema_duplicate_capability_message(constraint.param_name, capability_name));
                continue;
            }
            const base::u32 predicate_index = this->record_generic_trait_predicate(
                info, constraint, param->second, i, TraitPredicateKind::declared_trait, CapabilityKind::sized, trait);
            lookup_context.predicate_indices.push_back(predicate_index);
            TraitPredicate& predicate = this->core_.state_.checked.trait_predicates[predicate_index];
            std::unordered_map<IdentId, base::SourceRange, IdentIdHash> seen_associated_constraints;
            seen_associated_constraints.reserve(associated_constraints.size());
            for (const syntax::AssociatedTypeConstraintDecl& associated_constraint : associated_constraints) {
                const auto inserted =
                    seen_associated_constraints.emplace(associated_constraint.name_id, associated_constraint.range);
                if (!inserted.second) {
                    this->core_.report_capability(associated_constraint.range,
                        sema_duplicate_associated_type_constraint_message(trait->name, associated_constraint.name));
                    this->core_.report_note(inserted.first->second, SemanticDiagnosticKind::duplicate,
                        sema_previous_declaration_note_message(associated_constraint.name));
                    continue;
                }
                const auto associated_requirement = std::ranges::find_if(
                    trait->associated_types, [&](const TraitAssociatedTypeRequirement& requirement) {
                        return requirement.name_id == associated_constraint.name_id;
                    });
                if (associated_requirement == trait->associated_types.end()) {
                    this->core_.report_capability(associated_constraint.range,
                        sema_unknown_associated_type_constraint_message(trait->name, associated_constraint.name));
                    continue;
                }
                TraitImplAssociatedTypeInfo equality =
                    this->core_.state_.checked.make_trait_impl_associated_type_info();
                equality.name = this->core_.source_name_text(associated_constraint.name_id, associated_constraint.name);
                equality.name_id = associated_constraint.name_id;
                equality.syntax_type = associated_constraint.value_type;
                equality.value_type = this->core_.resolve_type(associated_constraint.value_type);
                equality.member_key = associated_requirement->member_key;
                equality.requirement_ordinal = associated_requirement->ordinal;
                if (is_valid(equality.value_type)
                    && this->core_.state_.checked.types.get(equality.value_type).kind
                        == TypeKind::associated_projection) {
                    const TypeInfo& equality_info = this->core_.state_.checked.types.get(equality.value_type);
                    if (equality_info.associated_member == equality.member_key) {
                        this->core_.report_type(associated_constraint.range,
                            sema_associated_type_projection_cycle_message(trait->name, associated_constraint.name));
                    }
                }
                predicate.associated_type_equalities.push_back(equality);
            }
            if (!predicate.associated_type_equalities.empty()) {
                query::StableHashBuilder hash;
                hash.mix_fingerprint(predicate.canonical_fingerprint);
                for (const TraitImplAssociatedTypeInfo& equality : predicate.associated_type_equalities) {
                    hash.mix_u64(equality.member_key.global_id);
                    hash.mix_string(this->core_.state_.checked.types.display_name(equality.value_type));
                }
                predicate.canonical_fingerprint = hash.finish();
                for (TraitObligation& obligation : this->core_.state_.checked.trait_obligations) {
                    if (obligation.predicate_index == predicate_index) {
                        obligation.predicate_fingerprint = predicate.canonical_fingerprint;
                    }
                }
                for (TraitEvidence& evidence : this->core_.state_.checked.trait_evidence) {
                    if (evidence.predicate_index == predicate_index) {
                        evidence.predicate_fingerprint = predicate.canonical_fingerprint;
                    }
                }
            }
        }
    }
    this->record_generic_param_env(info, item);
}

bool SemanticAnalyzerCore::GenericAnalyzer::generic_param_has_capability(
    const std::string_view param, const CapabilityKind capability) const
{
    if (this->core_.state_.flow.current_generic_context == nullptr) {
        return false;
    }
    const IdentId param_id = this->core_.ctx_.module.find_identifier(param);
    const auto identity = this->core_.state_.flow.current_generic_context->param_identities.find(param_id);
    if (identity != this->core_.state_.flow.current_generic_context->param_identities.end()) {
        const auto by_identity =
            this->core_.state_.flow.current_generic_context->constraints_by_identity.find(identity->second);
        if (by_identity != this->core_.state_.flow.current_generic_context->constraints_by_identity.end()) {
            return by_identity->second.contains(capability);
        }
    }
    const auto found = this->core_.state_.flow.current_generic_context->constraints.find(param_id);
    return found != this->core_.state_.flow.current_generic_context->constraints.end()
        && found->second.contains(capability);
}

bool SemanticAnalyzerCore::GenericAnalyzer::generic_param_has_capability(
    const TypeHandle param, const CapabilityKind capability) const
{
    if (this->core_.state_.flow.current_generic_context == nullptr || !is_valid(param)) {
        return false;
    }
    const TypeInfo& info = this->core_.state_.checked.types.get(param);
    if (info.kind != TypeKind::generic_param) {
        return false;
    }
    const GenericParamIdentity identity = this->core_.generic_param_identity(info);
    if (is_valid(identity)) {
        const auto found = this->core_.state_.flow.current_generic_context->constraints_by_identity.find(identity);
        if (found != this->core_.state_.flow.current_generic_context->constraints_by_identity.end()) {
            return found->second.contains(capability);
        }
    }
    return this->core_.generic_param_has_capability(info.name, capability);
}

bool SemanticAnalyzerCore::GenericAnalyzer::type_satisfies_capability(
    const TypeHandle type, const CapabilityKind capability) const
{
    if (!is_valid(type)) {
        return false;
    }
    const TypeInfo& info = this->core_.state_.checked.types.get(type);
    if (info.kind == TypeKind::generic_param) {
        return this->core_.generic_param_has_capability(type, capability);
    }
    if (capability == CapabilityKind::sized) {
        return this->core_.is_valid_storage_type(type);
    }
    if (capability == CapabilityKind::eq) {
        return this->core_.type_satisfies_equality_capability(type);
    }
    if (capability == CapabilityKind::ord) {
        return this->core_.type_satisfies_ordering_capability(type);
    }
    if (capability == CapabilityKind::hash) {
        return this->core_.type_supports_hash_capability(type);
    }
    if (capability == CapabilityKind::copy) {
        return resource_is_copy(ResourceSemanticsClassifier(this->core_.state_.checked, [this](const TypeHandle param) {
            return this->core_.generic_param_has_capability(param, CapabilityKind::copy);
        }).classify(type));
    }
    return false;
}

bool SemanticAnalyzerCore::GenericAnalyzer::type_satisfies_equality_capability(const TypeHandle type) const
{
    if (!is_valid(type)) {
        return false;
    }
    const TypeInfo& info = this->core_.state_.checked.types.get(type);
    return this->core_.state_.checked.types.is_bool(type) || this->core_.state_.checked.types.is_char(type)
        || this->core_.state_.checked.types.is_integer(type) || this->core_.state_.checked.types.is_pointer(type)
        || (info.kind == TypeKind::enum_ && !is_valid(info.enum_payload_storage));
}

bool SemanticAnalyzerCore::GenericAnalyzer::type_satisfies_ordering_capability(const TypeHandle type) const
{
    return this->core_.state_.checked.types.is_integer(type);
}

bool SemanticAnalyzerCore::GenericAnalyzer::type_supports_equality_operator(const TypeHandle type) const
{
    if (!is_valid(type)) {
        return false;
    }
    const TypeInfo& info = this->core_.state_.checked.types.get(type);
    return this->core_.state_.checked.types.is_bool(type) || this->core_.state_.checked.types.is_char(type)
        || this->core_.state_.checked.types.is_integer(type) || this->core_.state_.checked.types.is_float(type)
        || this->core_.state_.checked.types.is_pointer(type)
        || (info.kind == TypeKind::enum_ && !is_valid(info.enum_payload_storage));
}

bool SemanticAnalyzerCore::GenericAnalyzer::type_supports_ordering_operator(const TypeHandle type) const
{
    return this->core_.state_.checked.types.is_integer(type) || this->core_.state_.checked.types.is_float(type);
}

bool SemanticAnalyzerCore::GenericAnalyzer::type_supports_hash_capability(const TypeHandle type) const
{
    if (!is_valid(type)) {
        return false;
    }
    return this->core_.state_.checked.types.is_bool(type) || this->core_.state_.checked.types.is_char(type)
        || this->core_.state_.checked.types.is_integer(type) || this->core_.state_.checked.types.is_pointer(type);
}

bool SemanticAnalyzerCore::GenericAnalyzer::validate_generic_arguments(
    const GenericTemplateInfo& info, const std::vector<TypeHandle>& args, const base::SourceRange& use_range)
{
    bool ok = true;
    for (base::usize i = 0; i < info.params.size() && i < args.size(); ++i) {
        const IdentId param_id = info.params[i];
        const auto found = info.constraints.find(param_id);
        if (found == info.constraints.end()) {
            continue;
        }
        for (const CapabilityKind capability : found->second) {
            if (!this->core_.type_satisfies_capability(args[i], capability)) {
                this->core_.report_capability(use_range,
                    sema_generic_capability_not_satisfied_message(
                        this->core_.state_.checked.types.display_name(args[i]), capability_name(capability)));
                ok = false;
            }
        }
    }
    for (const base::u32 predicate_index : info.predicate_indices) {
        if (predicate_index >= this->core_.state_.checked.trait_predicates.size()) {
            continue;
        }
        const TraitPredicate& predicate = this->core_.state_.checked.trait_predicates[predicate_index];
        if (predicate.kind != TraitPredicateKind::declared_trait
            || predicate.subject_param_index == SEMA_TRAIT_PREDICATE_INVALID_INDEX
            || predicate.subject_param_index >= args.size()) {
            continue;
        }
        if (!this->type_satisfies_trait_predicate(args[predicate.subject_param_index], predicate, use_range)) {
            ok = false;
        }
    }
    return ok;
}

bool SemanticAnalyzerCore::GenericAnalyzer::generic_param_has_trait_predicate(
    const TypeHandle param, const TraitPredicate& predicate) const
{
    if (this->core_.state_.flow.current_generic_context == nullptr || !is_valid(param)
        || param.value >= this->core_.state_.checked.types.size()) {
        return false;
    }
    const TypeInfo& info = this->core_.state_.checked.types.get(param);
    if (info.kind != TypeKind::generic_param || !is_valid(info.generic_identity)) {
        return false;
    }
    for (const base::u32 predicate_index : this->core_.state_.flow.current_generic_context->predicate_indices) {
        if (predicate_index >= this->core_.state_.checked.trait_predicates.size()) {
            continue;
        }
        const TraitPredicate& candidate = this->core_.state_.checked.trait_predicates[predicate_index];
        if (candidate.kind != TraitPredicateKind::declared_trait
            || candidate.subject_param_identity != info.generic_identity
            || candidate.trait_stable_id != predicate.trait_stable_id) {
            continue;
        }
        bool equalities_satisfied = true;
        for (const TraitImplAssociatedTypeInfo& expected : predicate.associated_type_equalities) {
            const auto found = std::ranges::find_if(
                candidate.associated_type_equalities, [&](const TraitImplAssociatedTypeInfo& actual) {
                    return actual.member_key == expected.member_key
                        && this->core_.state_.checked.types.same(actual.value_type, expected.value_type);
                });
            equalities_satisfied = equalities_satisfied && found != candidate.associated_type_equalities.end();
        }
        if (equalities_satisfied) {
            return true;
        }
    }
    return false;
}

bool SemanticAnalyzerCore::GenericAnalyzer::type_satisfies_trait_predicate(
    const TypeHandle type, const TraitPredicate& predicate, const base::SourceRange& use_range)
{
    if (!is_valid(type) || type.value >= this->core_.state_.checked.types.size()) {
        return false;
    }
    const TypeInfo& info = this->core_.state_.checked.types.get(type);
    if (info.kind == TypeKind::generic_param) {
        const bool satisfied = this->generic_param_has_trait_predicate(type, predicate);
        if (!satisfied) {
            this->core_.report_capability(use_range,
                sema_trait_predicate_not_satisfied_message(
                    this->core_.state_.checked.types.display_name(type), predicate.trait_name));
        }
        return satisfied;
    }

    struct RejectedTraitImplCandidate {
        const TraitImplInfo* impl = nullptr;
        std::string reason;
    };
    std::vector<RejectedTraitImplCandidate> rejected_candidates;
    for (const auto& entry : this->core_.state_.checked.trait_impls) {
        const TraitImplInfo& impl = entry.second;
        if (impl.trait_module.value != predicate.trait_module.value || impl.trait_name_id != predicate.trait_name_id) {
            continue;
        }
        if (!this->core_.state_.checked.types.same(impl.self_type, type)) {
            rejected_candidates.push_back(RejectedTraitImplCandidate{&impl, "self type mismatch"});
            continue;
        }
        if (!impl.trait_args.empty()) {
            rejected_candidates.push_back(RejectedTraitImplCandidate{&impl, "trait arguments differ"});
            continue;
        }
        for (const TraitImplAssociatedTypeInfo& expected : predicate.associated_type_equalities) {
            const auto actual =
                std::ranges::find_if(impl.associated_types, [&](const TraitImplAssociatedTypeInfo& associated_type) {
                    return associated_type.member_key == expected.member_key;
                });
            if (actual == impl.associated_types.end()
                || !this->core_.state_.checked.types.same(actual->value_type, expected.value_type)) {
                const std::string checked_type_name = this->core_.state_.checked.types.display_name(type);
                const std::string expected_type_name =
                    this->core_.state_.checked.types.display_name(expected.value_type);
                const std::string actual_type_name = actual == impl.associated_types.end()
                    ? std::string("<missing>")
                    : this->core_.state_.checked.types.display_name(actual->value_type);
                this->core_.report_capability(use_range,
                    sema_trait_associated_type_equality_not_satisfied_message(
                        predicate.trait_name, checked_type_name, expected.name, expected_type_name, actual_type_name));
                this->core_.report_note(impl.range, SemanticDiagnosticKind::capability,
                    sema_candidate_trait_impl_note_message(predicate.trait_name, checked_type_name));
                const base::SourceRange associated_range = actual != impl.associated_types.end()
                        && syntax::is_valid(actual->item) && actual->item.value < this->core_.ctx_.module.items.size()
                    ? this->core_.ctx_.module.items[actual->item.value].range
                    : impl.range;
                this->core_.report_note(associated_range, SemanticDiagnosticKind::capability,
                    sema_trait_impl_associated_type_note_message(expected.name, actual_type_name));
                return false;
            }
        }
        return true;
    }
    const std::string checked_type_name = this->core_.state_.checked.types.display_name(type);
    this->core_.report_capability(
        use_range, sema_trait_predicate_not_satisfied_message(checked_type_name, predicate.trait_name));
    for (const RejectedTraitImplCandidate& candidate : rejected_candidates) {
        if (candidate.impl == nullptr) {
            continue;
        }
        this->core_.report_note(candidate.impl->range, SemanticDiagnosticKind::capability,
            sema_rejected_trait_impl_note_message(predicate.trait_name,
                this->core_.state_.checked.types.display_name(candidate.impl->self_type), checked_type_name,
                candidate.reason));
    }
    return false;
}

void SemanticAnalyzerCore::GenericAnalyzer::populate_generic_template_node_spans(
    GenericTemplateInfo& info, const syntax::ItemNode& item) const
{
    GenericNodeSpanBuilder builder(this->core_.ctx_.module);
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

bool SemanticAnalyzerCore::GenericTemplateInfo::has_sparse_node_ids() const noexcept
{
    return !this->expr_node_ids.empty() || !this->pattern_node_ids.empty() || !this->type_node_ids.empty()
        || !this->stmt_node_ids.empty();
}

std::string SemanticAnalyzerCore::GenericAnalyzer::generic_template_incremental_fingerprint(
    const syntax::ItemNode& item, const GenericTemplateInfo& info) const
{
    std::string fingerprint;
    fingerprint += item.name;
    fingerprint += SEMA_GENERIC_TEMPLATE_INCREMENTAL_TAG;
    fingerprint += SEMA_GENERIC_TEMPLATE_FIELD_SEPARATOR;
    fingerprint += std::to_string(static_cast<base::u32>(item.kind));
    for (base::usize param_index = 0; param_index < info.params.size(); ++param_index) {
        fingerprint += SEMA_GENERIC_TEMPLATE_PARAM_TAG;
        fingerprint += this->core_.generic_param_name(info, param_index);
        const auto constraints = info.constraints.find(info.params[param_index]);
        if (constraints == info.constraints.end()) {
            continue;
        }
        std::vector<CapabilityKind> capabilities(constraints->second.begin(), constraints->second.end());
        std::ranges::sort(capabilities, [](const CapabilityKind lhs, const CapabilityKind rhs) {
            return static_cast<base::u8>(lhs) < static_cast<base::u8>(rhs);
        });
        for (const CapabilityKind capability : capabilities) {
            fingerprint += SEMA_GENERIC_TEMPLATE_CONSTRAINT_TAG;
            fingerprint += capability_name(capability);
        }
    }
    if (!info.predicate_indices.empty()) {
        std::vector<std::string> predicates;
        predicates.reserve(info.predicate_indices.size());
        for (const base::u32 predicate_index : info.predicate_indices) {
            if (predicate_index >= this->core_.state_.checked.trait_predicates.size()) {
                continue;
            }
            predicates.push_back(query::debug_string(
                this->core_.state_.checked.trait_predicates[predicate_index].canonical_fingerprint));
        }
        std::ranges::sort(predicates);
        for (const std::string& predicate : predicates) {
            fingerprint += SEMA_GENERIC_TEMPLATE_PREDICATE_TAG;
            fingerprint += predicate;
        }
    }
    return fingerprint;
}

void SemanticAnalyzerCore::GenericAnalyzer::record_generic_template_signature(
    const GenericTemplateInfo& info, const query::DefNamespace name_space)
{
    base::u32 constraint_count = static_cast<base::u32>(info.predicate_indices.size());
    if (constraint_count == 0) {
        for (const IdentId param_id : info.params) {
            const auto found = info.constraints.find(param_id);
            if (found == info.constraints.end()) {
                continue;
            }
            constraint_count += static_cast<base::u32>(found->second.size());
        }
    }
    this->core_.state_.checked.generic_template_signatures.push_back(GenericTemplateSignatureInfo{
        this->core_.state_.checked.intern_text(info.name.view()),
        info.name_id,
        info.module,
        info.visibility,
        info.stable_id,
        info.incremental_key,
        name_space,
        static_cast<base::u32>(info.params.size()),
        constraint_count,
        info.part_index,
    });
}

GenericSideTables SemanticAnalyzerCore::GenericAnalyzer::make_generic_instance_side_tables(
    const GenericTemplateInfo& info)
{
    if (!info.has_sparse_node_ids()) {
        return make_generic_side_tables(info);
    }
    const base::usize layout_index = this->core_.generic_side_table_layout_index(info);
    const GenericSideTableLayout* const layout = this->core_.state_.checked.generic_side_table_layout(layout_index);
    GenericSideTables side_tables;
    if (layout != nullptr) {
        side_tables.configure_local_dense(*layout);
    }
    return side_tables;
}

base::usize SemanticAnalyzerCore::GenericAnalyzer::generic_side_table_layout_index(const GenericTemplateInfo& info)
{
    if (info.checked_side_table_layout_index != SEMA_GENERIC_SIDE_TABLE_INVALID_LAYOUT_INDEX) {
        return info.checked_side_table_layout_index;
    }
    info.checked_side_table_layout_index =
        this->core_.state_.checked.append_generic_side_table_layout(GenericSideTableLocalLayoutView{
            info.expr_span,
            info.pattern_span,
            info.type_span,
            info.stmt_span,
            info.expr_node_ids,
            info.pattern_node_ids,
            info.type_node_ids,
            info.stmt_node_ids,
        });
    return info.checked_side_table_layout_index;
}

void SemanticAnalyzerCore::GenericAnalyzer::register_generic_template(
    const syntax::ItemNode& item, const syntax::ItemId item_id)
{
    this->core_.validate_generic_parameter_list(item);
    const syntax::ModuleId owner = this->core_.item_module(item_id);
    GenericTemplateInfo info = this->core_.make_generic_template_info();
    info.item = item_id;
    info.module = owner;
    info.part_index = this->core_.item_part_index(item_id);
    info.name = this->core_.source_name_text(item.name_id, item.name);
    info.name_id = item.name_id;
    info.key = this->core_.module_lookup_key(owner, item.name_id);
    info.function_key = this->core_.function_lookup_key(owner, item.name_id);
    info.visibility = item.visibility;
    info.stable_id =
        this->core_.stable_definition_id(owner, StableSymbolKind::generic_template, item.name_id, item.name);
    info.params.reserve(item.generic_params.size());
    for (const syntax::GenericParamDecl& param : item.generic_params) {
        info.params.push_back(param.name_id);
    }
    this->core_.populate_generic_template_node_spans(info, item);
    this->core_.populate_generic_param_identities(info);
    this->core_.validate_generic_constraints(item, info);
    info.incremental_key = this->core_.stable_incremental_key(
        info.stable_id, this->core_.generic_template_incremental_fingerprint(item, info));
    query::DefNamespace generic_param_owner_namespace = query::DefNamespace::type;
    if (item.kind == syntax::ItemKind::fn_decl) {
        generic_param_owner_namespace =
            syntax::is_valid(item.impl_type) ? query::DefNamespace::member : query::DefNamespace::value;
    }
    this->core_.index_generic_param_query_keys(info, generic_param_owner_namespace);

    if (item.kind == syntax::ItemKind::struct_decl) {
        if (this->core_.state_.types.named_types.contains(info.key)
            || this->core_.state_.checked.type_aliases.contains(info.key)
            || this->core_.state_.checked.traits.contains(info.key)
            || this->core_.state_.generics.struct_templates.contains(info.key)
            || this->core_.state_.generics.enum_templates.contains(info.key)
            || this->core_.state_.generics.type_alias_templates.contains(info.key)) {
            this->core_.report_duplicate(
                item.range, sema_duplicate_type_definition_message(this->core_.module_name(owner), item.name));
            return;
        }
        const auto inserted = this->core_.state_.generics.struct_templates.emplace(info.key, std::move(info));
        if (inserted.second) {
            this->core_.record_generic_template_signature(inserted.first->second, query::DefNamespace::type);
            this->core_.index_generic_struct_template(inserted.first->second);
        }
        return;
    }

    if (item.kind == syntax::ItemKind::enum_decl) {
        if (this->core_.state_.types.named_types.contains(info.key)
            || this->core_.state_.checked.type_aliases.contains(info.key)
            || this->core_.state_.checked.traits.contains(info.key)
            || this->core_.state_.generics.struct_templates.contains(info.key)
            || this->core_.state_.generics.enum_templates.contains(info.key)
            || this->core_.state_.generics.type_alias_templates.contains(info.key)) {
            this->core_.report_duplicate(
                item.range, sema_duplicate_type_definition_message(this->core_.module_name(owner), item.name));
            return;
        }
        const auto inserted = this->core_.state_.generics.enum_templates.emplace(info.key, std::move(info));
        if (inserted.second) {
            this->core_.record_generic_template_signature(inserted.first->second, query::DefNamespace::type);
            this->core_.index_generic_enum_template(inserted.first->second);
        }
        return;
    }

    if (item.kind == syntax::ItemKind::type_alias) {
        if (this->core_.state_.types.named_types.contains(info.key)
            || this->core_.state_.checked.type_aliases.contains(info.key)
            || this->core_.state_.checked.traits.contains(info.key)
            || this->core_.state_.generics.struct_templates.contains(info.key)
            || this->core_.state_.generics.enum_templates.contains(info.key)
            || this->core_.state_.generics.type_alias_templates.contains(info.key)) {
            this->core_.report_duplicate(
                item.range, sema_duplicate_type_definition_message(this->core_.module_name(owner), item.name));
            return;
        }
        const auto inserted = this->core_.state_.generics.type_alias_templates.emplace(info.key, std::move(info));
        if (inserted.second) {
            this->core_.record_generic_template_signature(inserted.first->second, query::DefNamespace::type);
            this->core_.index_generic_type_alias_template(inserted.first->second);
        }
        return;
    }

    if (item.kind != syntax::ItemKind::fn_decl) {
        if (item.kind != syntax::ItemKind::impl_block) {
            this->core_.report_unsupported(item.range, std::string(SEMA_GENERIC_PARAMS_UNSUPPORTED_ON_ITEM));
        }
        return;
    }

    if (syntax::visibility_is_public(item.visibility) && !syntax::is_valid(item.return_type)) {
        this->core_.report_general(item.range, std::string(SEMA_PUBLIC_FUNCTION_RETURN_TYPE_EXPLICIT));
    }
    if (item.is_extern_c || item.is_export_c || item.is_prototype) {
        this->core_.report_unsupported(item.range, std::string(SEMA_GENERIC_C_ABI_OR_PROTOTYPE_UNSUPPORTED));
    }
    if (syntax::is_valid(item.impl_type)) {
        GenericContext generic_context = this->core_.make_generic_context();
        this->core_.populate_generic_placeholder_context(info, generic_context);
        {
            GenericAnalysisScope scope(this->core_, owner, &generic_context, nullptr, false, info.item);
            info.impl_type_pattern = this->core_.resolve_type(item.impl_type);
        }
        if (!is_valid(info.impl_type_pattern)) {
            return;
        }
        info.function_key = this->core_.method_function_lookup_key(owner, info.impl_type_pattern, item.name_id);
        this->core_.populate_generic_param_identities(info);
        const TypeKind impl_type_kind = this->core_.state_.checked.types.get(info.impl_type_pattern).kind;
        if (impl_type_kind != TypeKind::struct_ && impl_type_kind != TypeKind::enum_
            && impl_type_kind != TypeKind::opaque_struct) {
            this->core_.report_general(item.range, std::string(SEMA_IMPL_TARGET_NAMED_TYPE));
            return;
        }

        if (this->core_.type_member_name_exists(info.impl_type_pattern, item.name_id, item.name)) {
            this->core_.report_duplicate(item.range,
                sema_duplicate_type_member_message(
                    this->core_.state_.checked.types.display_name(info.impl_type_pattern), item.name));
            return;
        }
        const auto inserted = this->core_.state_.generics.method_templates.emplace(info.function_key, std::move(info));
        if (inserted.second) {
            this->core_.record_generic_template_signature(inserted.first->second, query::DefNamespace::member);
            this->core_.index_generic_method_template(inserted.first->second);
        }
        return;
    }
    if (this->core_.state_.checked.functions.contains(info.function_key)
        || this->core_.state_.generics.function_templates.contains(info.key)) {
        this->core_.report_duplicate(
            item.range, sema_duplicate_function_definition_message(this->core_.module_name(owner), item.name));
        return;
    }
    const auto inserted = this->core_.state_.generics.function_templates.emplace(info.key, std::move(info));
    if (inserted.second) {
        this->core_.record_generic_template_signature(inserted.first->second, query::DefNamespace::value);
        this->core_.index_generic_function_template(inserted.first->second);
    }
}

void SemanticAnalyzerCore::GenericAnalyzer::populate_generic_param_identities(GenericTemplateInfo& info)
{
    info.param_identities.clear();
    info.param_identities.reserve(info.params.size());
    for (base::usize index = 0; index < info.params.size(); ++index) {
        info.param_identities.push_back(this->core_.make_generic_param_identity(info, index));
    }
}

GenericParamIdentity SemanticAnalyzerCore::GenericAnalyzer::make_generic_param_identity(
    const GenericTemplateInfo& info, const base::usize index) const
{
    base::u64 hash = SEMA_GENERIC_PARAM_IDENTITY_OFFSET;
    hash = mix_generic_identity_text(hash, SEMA_GENERIC_PARAM_IDENTITY_MARKER);
    hash = mix_generic_identity_u64(hash, info.key.module);
    const std::string_view template_name = this->core_.ctx_.module.identifier_text(info.name_id).empty()
        ? info.name.view()
        : this->core_.ctx_.module.identifier_text(info.name_id);
    hash = mix_generic_identity_text(hash, template_name);
    hash = mix_generic_identity_u64(hash, index);
    if (index < info.params.size()) {
        hash = mix_generic_identity_text(hash, this->core_.generic_param_name(info, index));
    }
    if (syntax::is_valid(info.item) && info.item.value < this->core_.ctx_.module.items.size()) {
        const syntax::ItemNode item = this->core_.ctx_.module.items[info.item.value];
        if (index < item.generic_params.size()) {
            const base::SourceRange range = item.generic_params[index].range;
            hash = mix_generic_identity_u64(hash, range.source.value);
            hash = mix_generic_identity_u64(hash, range.begin);
            hash = mix_generic_identity_u64(hash, range.end);
        }
    }
    return finish_generic_param_identity(hash);
}

std::string_view SemanticAnalyzerCore::GenericAnalyzer::generic_param_name(
    const GenericTemplateInfo& info, const base::usize index) const
{
    if (index >= info.params.size()) {
        return {};
    }
    const std::string_view interned_name = this->core_.ctx_.module.identifier_text(info.params[index]);
    if (!interned_name.empty()) {
        return interned_name;
    }
    if (syntax::is_valid(info.item) && info.item.value < this->core_.ctx_.module.items.size()) {
        const syntax::ItemNode item = this->core_.ctx_.module.items[info.item.value];
        if (index < item.generic_params.size()) {
            return item.generic_params[index].name;
        }
    }
    return {};
}

GenericParamIdentity SemanticAnalyzerCore::GenericAnalyzer::generic_param_identity(
    const GenericTemplateInfo& info, const base::usize index) const
{
    if (index < info.param_identities.size()) {
        return info.param_identities[index];
    }
    return this->core_.make_generic_param_identity(info, index);
}

GenericParamIdentity SemanticAnalyzerCore::GenericAnalyzer::generic_param_identity(const TypeInfo& info) const
{
    if (is_valid(info.generic_identity)) {
        return info.generic_identity;
    }
    return generic_param_identity_from_text(info.name);
}

TypeHandle SemanticAnalyzerCore::GenericAnalyzer::generic_param_placeholder(
    const GenericTemplateInfo& info, const base::usize index)
{
    if (index >= info.params.size()) {
        return INVALID_TYPE_HANDLE;
    }
    return this->core_.state_.checked.types.generic_param(
        this->core_.generic_param_identity(info, index), this->core_.generic_param_name(info, index));
}

void SemanticAnalyzerCore::GenericAnalyzer::populate_generic_placeholder_context(
    const GenericTemplateInfo& info, GenericContext& context)
{
    context.params.clear();
    context.param_identities.clear();
    this->core_.copy_capability_map(context.constraints, info.constraints);
    context.constraints_by_identity.clear();
    context.predicate_indices.assign(info.predicate_indices.begin(), info.predicate_indices.end());
    context.obligation_indices.assign(info.obligation_indices.begin(), info.obligation_indices.end());
    context.param_env_key = info.param_env_key;
    context.params.reserve(info.params.size());
    context.param_identities.reserve(info.params.size());
    context.constraints_by_identity.reserve(info.params.size());
    for (base::usize i = 0; i < info.params.size(); ++i) {
        const GenericParamIdentity identity = this->core_.generic_param_identity(info, i);
        const IdentId param_id = info.params[i];
        context.params.emplace(param_id, this->core_.generic_param_placeholder(info, i));
        context.param_identities.emplace(param_id, identity);
        if (const auto constraints = info.constraints.find(param_id); constraints != info.constraints.end()) {
            context.constraints_by_identity.emplace(identity, this->core_.copy_capability_set(constraints->second));
        }
    }
}

void SemanticAnalyzerCore::GenericAnalyzer::populate_generic_concrete_context(
    const GenericTemplateInfo& info, const std::vector<TypeHandle>& args, GenericContext& context) const
{
    context.params.clear();
    context.param_identities.clear();
    this->core_.copy_capability_map(context.constraints, info.constraints);
    context.constraints_by_identity.clear();
    context.predicate_indices.assign(info.predicate_indices.begin(), info.predicate_indices.end());
    context.obligation_indices.assign(info.obligation_indices.begin(), info.obligation_indices.end());
    context.param_env_key = info.param_env_key;
    context.params.reserve(info.params.size());
    context.param_identities.reserve(info.params.size());
    context.constraints_by_identity.reserve(info.params.size());
    for (base::usize i = 0; i < info.params.size() && i < args.size(); ++i) {
        const GenericParamIdentity identity = this->core_.generic_param_identity(info, i);
        const IdentId param_id = info.params[i];
        context.params.emplace(param_id, args[i]);
        context.param_identities.emplace(param_id, identity);
        const auto constraints = info.constraints.find(param_id);
        if (constraints == info.constraints.end()) {
            continue;
        }
        context.constraints_by_identity.emplace(identity, this->core_.copy_capability_set(constraints->second));
        if (is_valid(args[i])) {
            const TypeInfo& arg_info = this->core_.state_.checked.types.get(args[i]);
            if (arg_info.kind == TypeKind::generic_param) {
                CapabilitySet& inherited = this->core_.capability_bucket(
                    context.constraints_by_identity, this->core_.generic_param_identity(arg_info));
                inherited.insert(constraints->second.begin(), constraints->second.end());
            }
        }
    }
}

std::string SemanticAnalyzerCore::GenericAnalyzer::generic_instance_key_suffix(
    const std::vector<TypeHandle>& args) const
{
    std::string suffix;
    suffix.reserve(SEMA_GENERIC_KEY_ARG_PREFIX.size() + args.size() * SEMA_GENERIC_KEY_ARG_SIZE_ESTIMATE);
    append_generic_instance_key_suffix(suffix, args);
    return suffix;
}

std::string SemanticAnalyzerCore::GenericAnalyzer::generic_instance_abi_suffix(
    const query::GenericInstanceKey& key) const
{
    std::string suffix(SEMA_GENERIC_ABI_SUFFIX_PREFIX);
    suffix.reserve(SEMA_GENERIC_ABI_SUFFIX_SIZE_ESTIMATE);
    const query::StableFingerprint128 fingerprint = query::stable_key_fingerprint(key);
    suffix += SEMA_GENERIC_ABI_GLOBAL_ID_PREFIX;
    append_decimal(suffix, key.global_id);
    suffix += SEMA_GENERIC_ABI_PRIMARY_PREFIX;
    append_decimal(suffix, fingerprint.primary);
    suffix += SEMA_GENERIC_ABI_SECONDARY_PREFIX;
    append_decimal(suffix, fingerprint.secondary);
    suffix += SEMA_GENERIC_ABI_BYTE_COUNT_PREFIX;
    append_decimal(suffix, fingerprint.byte_count);
    return suffix;
}

std::string SemanticAnalyzerCore::GenericAnalyzer::generic_instance_key(
    const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const
{
    const std::string template_key = this->core_.generic_template_key_prefix(info.module, info.name_id, info.name);
    std::string key;
    key.reserve(template_key.size() + SEMA_GENERIC_INSTANCE_SEPARATOR.size() + SEMA_GENERIC_KEY_ARG_PREFIX.size()
        + args.size() * SEMA_GENERIC_KEY_ARG_SIZE_ESTIMATE);
    key += template_key;
    key += SEMA_GENERIC_INSTANCE_SEPARATOR;
    append_generic_instance_key_suffix(key, args);
    return key;
}

std::string SemanticAnalyzerCore::GenericAnalyzer::generic_struct_instance_key(
    const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const
{
    return this->core_.generic_instance_key(info, args);
}

std::string SemanticAnalyzerCore::GenericAnalyzer::generic_enum_instance_key(
    const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const
{
    return this->core_.generic_instance_key(info, args);
}

std::string SemanticAnalyzerCore::GenericAnalyzer::generic_type_alias_instance_key(
    const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const
{
    return this->core_.generic_instance_key(info, args);
}

std::string SemanticAnalyzerCore::GenericAnalyzer::generic_function_instance_key(
    const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const
{
    return this->core_.generic_instance_key(info, args);
}

const SemanticAnalyzerCore::GenericTemplateInfo*
SemanticAnalyzerCore::GenericAnalyzer::find_generic_struct_in_visible_modules(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    const ModuleLookupKey lookup_key =
        this->core_.find_module_lookup_key(this->core_.state_.flow.current_module, name_id);
    if (is_valid(lookup_key)) {
        if (const auto found = this->core_.state_.names.generic_struct_templates_by_name.find(lookup_key);
            found != this->core_.state_.names.generic_struct_templates_by_name.end()) {
            return found->second;
        }
    }

    if (report_unknown) {
        this->core_.report_lookup(range, sema_unknown_generic_type_message(name));
    }
    return nullptr;
}

const SemanticAnalyzerCore::GenericTemplateInfo* SemanticAnalyzerCore::GenericAnalyzer::find_generic_struct_in_module(
    const syntax::ModuleId module, const IdentId name_id, const std::string_view name, const base::SourceRange& range,
    const bool report_unknown)
{
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            this->core_.report_lookup(range, sema_unknown_generic_type_message(name));
        }
        return nullptr;
    }
    const GenericTemplateInfo* result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    const auto consider_candidate = [&](const syntax::ModuleId candidate_module, const IdentId lookup_name_id) -> bool {
        const GenericTemplateInfo* candidate = nullptr;
        const ModuleLookupKey lookup_key = this->core_.find_module_lookup_key(candidate_module, lookup_name_id);
        if (is_valid(lookup_key)) {
            if (const auto found = this->core_.state_.names.generic_struct_templates_by_name.find(lookup_key);
                found != this->core_.state_.names.generic_struct_templates_by_name.end()) {
                candidate = found->second;
            }
        }
        if (candidate == nullptr) {
            return false;
        }
        if (!this->core_.can_access_module(candidate_module, candidate->visibility)) {
            if (candidate_module.value == module.value && report_unknown) {
                this->core_.report_visibility(
                    range, sema_private_generic_type_message(this->core_.module_name(candidate_module), name));
                return true;
            }
            return false;
        }
        if (result == candidate) {
            return false;
        }
        if (result != nullptr) {
            this->core_.report_lookup(range,
                sema_ambiguous_generic_type_name_message(
                    name, this->core_.module_name(result_module), this->core_.module_name(candidate_module)));
            result = nullptr;
            return true;
        }
        result = candidate;
        result_module = candidate_module;
        return false;
    };
    const auto consider_exported_modules = [&](const syntax::ModuleId exported_module, const IdentId lookup_name_id) {
        for (const syntax::ModuleId candidate_module : this->core_.accessible_module_export_modules(exported_module)) {
            if (consider_candidate(candidate_module, lookup_name_id)) {
                return true;
            }
        }
        return false;
    };
    if (consider_exported_modules(module, name_id)) {
        return nullptr;
    }
    for (const SelectiveReexportTarget& target : this->core_.accessible_selective_reexports(module, name_id, name)) {
        if (consider_exported_modules(target.module, target.name_id)) {
            return nullptr;
        }
    }
    if (result == nullptr && report_unknown) {
        this->core_.report_lookup(
            range, sema_unknown_generic_type_in_module_message(this->core_.module_name(module), name));
    }
    return result;
}

const SemanticAnalyzerCore::GenericTemplateInfo*
SemanticAnalyzerCore::GenericAnalyzer::find_generic_enum_in_visible_modules(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    const ModuleLookupKey lookup_key =
        this->core_.find_module_lookup_key(this->core_.state_.flow.current_module, name_id);
    if (is_valid(lookup_key)) {
        if (const auto found = this->core_.state_.names.generic_enum_templates_by_name.find(lookup_key);
            found != this->core_.state_.names.generic_enum_templates_by_name.end()) {
            return found->second;
        }
    }

    if (report_unknown) {
        this->core_.report_lookup(range, sema_unknown_generic_type_message(name));
    }
    return nullptr;
}

const SemanticAnalyzerCore::GenericTemplateInfo* SemanticAnalyzerCore::GenericAnalyzer::find_generic_enum_in_module(
    const syntax::ModuleId module, const IdentId name_id, const std::string_view name, const base::SourceRange& range,
    const bool report_unknown)
{
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            this->core_.report_lookup(range, sema_unknown_generic_type_message(name));
        }
        return nullptr;
    }
    const GenericTemplateInfo* result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    const auto consider_candidate = [&](const syntax::ModuleId candidate_module, const IdentId lookup_name_id) -> bool {
        const GenericTemplateInfo* candidate = nullptr;
        const ModuleLookupKey lookup_key = this->core_.find_module_lookup_key(candidate_module, lookup_name_id);
        if (is_valid(lookup_key)) {
            if (const auto found = this->core_.state_.names.generic_enum_templates_by_name.find(lookup_key);
                found != this->core_.state_.names.generic_enum_templates_by_name.end()) {
                candidate = found->second;
            }
        }
        if (candidate == nullptr) {
            return false;
        }
        if (!this->core_.can_access_module(candidate_module, candidate->visibility)) {
            if (candidate_module.value == module.value && report_unknown) {
                this->core_.report_visibility(
                    range, sema_private_generic_type_message(this->core_.module_name(candidate_module), name));
                return true;
            }
            return false;
        }
        if (result == candidate) {
            return false;
        }
        if (result != nullptr) {
            this->core_.report_lookup(range,
                sema_ambiguous_generic_type_name_message(
                    name, this->core_.module_name(result_module), this->core_.module_name(candidate_module)));
            result = nullptr;
            return true;
        }
        result = candidate;
        result_module = candidate_module;
        return false;
    };
    const auto consider_exported_modules = [&](const syntax::ModuleId exported_module, const IdentId lookup_name_id) {
        for (const syntax::ModuleId candidate_module : this->core_.accessible_module_export_modules(exported_module)) {
            if (consider_candidate(candidate_module, lookup_name_id)) {
                return true;
            }
        }
        return false;
    };
    if (consider_exported_modules(module, name_id)) {
        return nullptr;
    }
    for (const SelectiveReexportTarget& target : this->core_.accessible_selective_reexports(module, name_id, name)) {
        if (consider_exported_modules(target.module, target.name_id)) {
            return nullptr;
        }
    }
    if (result == nullptr && report_unknown) {
        this->core_.report_lookup(
            range, sema_unknown_generic_type_in_module_message(this->core_.module_name(module), name));
    }
    return result;
}

const SemanticAnalyzerCore::GenericTemplateInfo*
SemanticAnalyzerCore::GenericAnalyzer::find_generic_type_alias_in_visible_modules(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    const ModuleLookupKey lookup_key =
        this->core_.find_module_lookup_key(this->core_.state_.flow.current_module, name_id);
    if (is_valid(lookup_key)) {
        if (const auto found = this->core_.state_.names.generic_type_alias_templates_by_name.find(lookup_key);
            found != this->core_.state_.names.generic_type_alias_templates_by_name.end()) {
            return found->second;
        }
    }

    if (report_unknown) {
        this->core_.report_lookup(range, sema_unknown_generic_type_message(name));
    }
    return nullptr;
}

const SemanticAnalyzerCore::GenericTemplateInfo*
SemanticAnalyzerCore::GenericAnalyzer::find_generic_type_alias_in_module(const syntax::ModuleId module,
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            this->core_.report_lookup(range, sema_unknown_generic_type_message(name));
        }
        return nullptr;
    }
    const GenericTemplateInfo* result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    const auto consider_candidate = [&](const syntax::ModuleId candidate_module, const IdentId lookup_name_id) -> bool {
        const GenericTemplateInfo* candidate = nullptr;
        const ModuleLookupKey lookup_key = this->core_.find_module_lookup_key(candidate_module, lookup_name_id);
        if (is_valid(lookup_key)) {
            if (const auto found = this->core_.state_.names.generic_type_alias_templates_by_name.find(lookup_key);
                found != this->core_.state_.names.generic_type_alias_templates_by_name.end()) {
                candidate = found->second;
            }
        }
        if (candidate == nullptr) {
            return false;
        }
        if (!this->core_.can_access_module(candidate_module, candidate->visibility)) {
            if (candidate_module.value == module.value && report_unknown) {
                this->core_.report_visibility(
                    range, sema_private_generic_type_message(this->core_.module_name(candidate_module), name));
                return true;
            }
            return false;
        }
        if (result == candidate) {
            return false;
        }
        if (result != nullptr) {
            this->core_.report_lookup(range,
                sema_ambiguous_generic_type_name_message(
                    name, this->core_.module_name(result_module), this->core_.module_name(candidate_module)));
            result = nullptr;
            return true;
        }
        result = candidate;
        result_module = candidate_module;
        return false;
    };
    const auto consider_exported_modules = [&](const syntax::ModuleId exported_module, const IdentId lookup_name_id) {
        for (const syntax::ModuleId candidate_module : this->core_.accessible_module_export_modules(exported_module)) {
            if (consider_candidate(candidate_module, lookup_name_id)) {
                return true;
            }
        }
        return false;
    };
    if (consider_exported_modules(module, name_id)) {
        return nullptr;
    }
    for (const SelectiveReexportTarget& target : this->core_.accessible_selective_reexports(module, name_id, name)) {
        if (consider_exported_modules(target.module, target.name_id)) {
            return nullptr;
        }
    }
    if (result == nullptr && report_unknown) {
        this->core_.report_lookup(
            range, sema_unknown_generic_type_in_module_message(this->core_.module_name(module), name));
    }
    return result;
}

bool SemanticAnalyzerCore::GenericAnalyzer::generic_type_template_exists_in_module(
    const syntax::ModuleId module, const IdentId name_id, const std::string_view name) const
{
    for (const syntax::ModuleId candidate_module : this->core_.accessible_module_export_modules(module)) {
        if (const GenericTemplateInfo* const found =
                this->core_.find_any_generic_type_template_in_module(candidate_module, name_id, name);
            found != nullptr && this->core_.can_access_module(candidate_module, found->visibility)) {
            return true;
        }
    }
    for (const SelectiveReexportTarget& target : this->core_.accessible_selective_reexports(module, name_id, name)) {
        for (const syntax::ModuleId candidate_module : this->core_.accessible_module_export_modules(target.module)) {
            if (const GenericTemplateInfo* const found =
                    this->core_.find_any_generic_type_template_in_module(candidate_module, target.name_id, target.name);
                found != nullptr && this->core_.can_access_module(candidate_module, found->visibility)) {
                return true;
            }
        }
    }
    return false;
}

const SemanticAnalyzerCore::GenericTemplateInfo*
SemanticAnalyzerCore::GenericAnalyzer::find_any_generic_type_template_in_module(
    const syntax::ModuleId module, const IdentId name_id, const std::string_view name) const
{
    if (!syntax::is_valid(module)) {
        return nullptr;
    }
    const ModuleLookupKey lookup_key = this->core_.find_module_lookup_key(module, name_id);
    if (is_valid(lookup_key)) {
        if (const auto found = this->core_.state_.names.generic_struct_templates_by_name.find(lookup_key);
            found != this->core_.state_.names.generic_struct_templates_by_name.end()) {
            return found->second;
        }
        if (const auto found = this->core_.state_.names.generic_enum_templates_by_name.find(lookup_key);
            found != this->core_.state_.names.generic_enum_templates_by_name.end()) {
            return found->second;
        }
        if (const auto found = this->core_.state_.names.generic_type_alias_templates_by_name.find(lookup_key);
            found != this->core_.state_.names.generic_type_alias_templates_by_name.end()) {
            return found->second;
        }
    }
    static_cast<void>(name);
    return nullptr;
}

bool SemanticAnalyzerCore::GenericAnalyzer::report_generic_type_requires_args_if_visible(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range)
{
    if (const GenericTemplateInfo* const found =
            this->core_.find_any_generic_type_template_in_module(this->core_.state_.flow.current_module, name_id, name);
        found != nullptr && this->core_.can_access_module(this->core_.state_.flow.current_module, found->visibility)) {
        this->core_.report_type(range, sema_generic_type_requires_args_message(name));
        return true;
    }
    return false;
}

void SemanticAnalyzerCore::GenericAnalyzer::report_generic_type_template_in_module(
    const syntax::ModuleId module, const IdentId name_id, const std::string_view name, const base::SourceRange& range)
{
    if (!syntax::is_valid(module)) {
        this->core_.report_lookup(range, sema_unknown_generic_type_message(name));
        return;
    }

    const auto report_from_exported_modules = [&](const syntax::ModuleId exported_module, const IdentId lookup_name_id,
                                                  const std::string_view diagnostic_name) -> bool {
        for (const syntax::ModuleId candidate_module : this->core_.accessible_module_export_modules(exported_module)) {
            const ModuleLookupKey lookup_key = this->core_.find_module_lookup_key(candidate_module, lookup_name_id);
            const bool has_struct_template =
                is_valid(lookup_key) && this->core_.state_.names.generic_struct_templates_by_name.contains(lookup_key);
            const bool has_enum_template =
                is_valid(lookup_key) && this->core_.state_.names.generic_enum_templates_by_name.contains(lookup_key);
            const bool has_alias_template = is_valid(lookup_key)
                && this->core_.state_.names.generic_type_alias_templates_by_name.contains(lookup_key);
            if (has_struct_template) {
                if (const GenericTemplateInfo* info =
                        this->core_.find_any_generic_type_template_in_module(candidate_module, lookup_name_id, name);
                    info != nullptr && this->core_.can_access_module(candidate_module, info->visibility)) {
                    this->core_.report_type(range, sema_generic_type_requires_args_message(diagnostic_name));
                    return true;
                }
                static_cast<void>(
                    this->core_.find_generic_struct_in_module(exported_module, lookup_name_id, name, range, true));
                return true;
            }
            if (has_enum_template) {
                if (const GenericTemplateInfo* info =
                        this->core_.find_any_generic_type_template_in_module(candidate_module, lookup_name_id, name);
                    info != nullptr && this->core_.can_access_module(candidate_module, info->visibility)) {
                    this->core_.report_type(range, sema_generic_type_requires_args_message(diagnostic_name));
                    return true;
                }
                static_cast<void>(
                    this->core_.find_generic_enum_in_module(exported_module, lookup_name_id, name, range, true));
                return true;
            }
            if (has_alias_template) {
                if (const GenericTemplateInfo* info =
                        this->core_.find_any_generic_type_template_in_module(candidate_module, lookup_name_id, name);
                    info != nullptr && this->core_.can_access_module(candidate_module, info->visibility)) {
                    this->core_.report_type(range, sema_generic_type_requires_args_message(diagnostic_name));
                    return true;
                }
                static_cast<void>(
                    this->core_.find_generic_type_alias_in_module(exported_module, lookup_name_id, name, range, true));
                return true;
            }
        }
        return false;
    };

    if (report_from_exported_modules(module, name_id, name)) {
        return;
    }
    for (const SelectiveReexportTarget& target : this->core_.accessible_selective_reexports(module, name_id, name)) {
        if (report_from_exported_modules(target.module, target.name_id, name)) {
            return;
        }
    }
    this->core_.report_lookup(
        range, sema_unknown_generic_type_in_module_message(this->core_.module_name(module), name));
}

const SemanticAnalyzerCore::GenericTemplateInfo*
SemanticAnalyzerCore::GenericAnalyzer::find_generic_function_in_visible_modules(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    const ModuleLookupKey lookup_key =
        this->core_.find_module_lookup_key(this->core_.state_.flow.current_module, name_id);
    if (is_valid(lookup_key)) {
        if (const auto found = this->core_.state_.names.generic_function_templates_by_name.find(lookup_key);
            found != this->core_.state_.names.generic_function_templates_by_name.end()) {
            return found->second;
        }
    }

    if (report_unknown) {
        this->core_.report_lookup(range, sema_unknown_generic_function_message(name));
    }
    return nullptr;
}

const SemanticAnalyzerCore::GenericTemplateInfo* SemanticAnalyzerCore::GenericAnalyzer::find_generic_function_in_module(
    const syntax::ModuleId module, const IdentId name_id, const std::string_view name, const base::SourceRange& range,
    const bool report_unknown)
{
    if (!syntax::is_valid(module)) {
        if (report_unknown) {
            this->core_.report_lookup(range, sema_unknown_generic_function_message(name));
        }
        return nullptr;
    }
    const GenericTemplateInfo* result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    const auto consider_candidate = [&](const syntax::ModuleId candidate_module, const IdentId lookup_name_id) -> bool {
        const GenericTemplateInfo* candidate = nullptr;
        const ModuleLookupKey lookup_key = this->core_.find_module_lookup_key(candidate_module, lookup_name_id);
        if (is_valid(lookup_key)) {
            if (const auto found = this->core_.state_.names.generic_function_templates_by_name.find(lookup_key);
                found != this->core_.state_.names.generic_function_templates_by_name.end()) {
                candidate = found->second;
            }
        }
        if (candidate == nullptr) {
            return false;
        }
        if (!this->core_.can_access_module(candidate_module, candidate->visibility)) {
            if (candidate_module.value == module.value && report_unknown) {
                this->core_.report_visibility(
                    range, sema_private_generic_function_message(this->core_.module_name(candidate_module), name));
                return true;
            }
            return false;
        }
        if (result == candidate) {
            return false;
        }
        if (result != nullptr) {
            this->core_.report_lookup(range,
                sema_ambiguous_generic_function_name_message(
                    name, this->core_.module_name(result_module), this->core_.module_name(candidate_module)));
            result = nullptr;
            return true;
        }
        result = candidate;
        result_module = candidate_module;
        return false;
    };
    const auto consider_exported_modules = [&](const syntax::ModuleId exported_module, const IdentId lookup_name_id) {
        for (const syntax::ModuleId candidate_module : this->core_.accessible_module_export_modules(exported_module)) {
            if (consider_candidate(candidate_module, lookup_name_id)) {
                return true;
            }
        }
        return false;
    };
    if (consider_exported_modules(module, name_id)) {
        return nullptr;
    }
    for (const SelectiveReexportTarget& target : this->core_.accessible_selective_reexports(module, name_id, name)) {
        if (consider_exported_modules(target.module, target.name_id)) {
            return nullptr;
        }
    }
    if (result == nullptr && report_unknown) {
        this->core_.report_lookup(
            range, sema_unknown_generic_function_in_module_message(this->core_.module_name(module), name));
    }
    return result;
}

TypeHandle SemanticAnalyzerCore::GenericAnalyzer::instantiate_generic_struct(const GenericTemplateInfo& info,
    const syntax::TypeNode& use_type, const syntax::TypeId, const std::vector<TypeHandle>& args)
{
    if (args.size() != info.params.size()) {
        this->core_.report_type(use_type.range,
            sema_generic_argument_count_message("generic type arguments", info.name, args.size(), info.params.size()));
        return INVALID_TYPE_HANDLE;
    }
    for (const TypeHandle arg : args) {
        if (!is_valid(arg)) {
            return INVALID_TYPE_HANDLE;
        }
    }
    if (!this->core_.validate_generic_arguments(info, args, use_type.range)) {
        return INVALID_TYPE_HANDLE;
    }
    const std::string instance_key = this->core_.generic_struct_instance_key(info, args);
    const IdentId instance_key_id = this->core_.intern_generated_key(instance_key);
    if (const auto found = this->core_.state_.generics.struct_instances.find(instance_key_id);
        found != this->core_.state_.generics.struct_instances.end()) {
        return found->second;
    }

    base::Result<GenericInstanceIdentity> instance_identity =
        this->core_.generic_instance_identity(info, args, query::DefNamespace::type);
    if (!instance_identity) {
        this->core_.report_internal_contract(use_type.range, instance_identity.error().message);
        return INVALID_TYPE_HANDLE;
    }
    const query::GenericInstanceKey instance_query_key = instance_identity.value().key;

    const syntax::ItemNode item = this->core_.ctx_.module.items[info.item.value];
    const std::string abi_suffix = this->core_.generic_instance_abi_suffix(instance_query_key);
    const std::string qualified = this->core_.qualified_name(info.module, item.name);
    const std::string c_name = this->core_.c_symbol_name(info.module, std::string(item.name) + abi_suffix);

    const TypeHandle handle = this->core_.state_.checked.types.named_struct(qualified, c_name, false);
    this->core_.state_.checked.types.set_generic_instance(
        handle, this->core_.generic_template_key_prefix(info.module, info.name_id, info.name), args);
    this->core_.state_.generics.struct_instances[instance_key_id] = handle;

    StructInfo struct_info = this->core_.state_.checked.make_struct_info();
    struct_info.name = this->core_.source_name_text(item.name_id, item.name);
    struct_info.name_id = item.name_id;
    struct_info.c_name = this->core_.state_.checked.intern_text(c_name);
    struct_info.module = info.module;
    struct_info.type = handle;
    struct_info.visibility = info.visibility;
    struct_info.part_index = info.part_index;
    struct_info.stable_id = sema::stable_definition_id(
        this->core_.stable_module_id(info.module), StableSymbolKind::type, instance_identity.value().fingerprint_text);
    struct_info.generic_instance_key = instance_query_key;
    struct_info.is_generic_placeholder = std::ranges::any_of(args, [&](const TypeHandle arg) {
        return is_valid(arg) && this->core_.state_.checked.types.get(arg).kind == TypeKind::generic_param;
    });

    GenericContext generic_context = this->core_.make_generic_context();
    this->core_.populate_generic_concrete_context(info, args, generic_context);

    bool contains_array = false;
    {
        GenericAnalysisScope scope(this->core_, info.module, &generic_context, nullptr, false, info.item);
        std::unordered_set<IdentId, IdentIdHash> seen_fields;
        for (const syntax::FieldDecl& field : item.fields) {
            if (!seen_fields.insert(field.name_id).second) {
                this->core_.report_duplicate(field.range, sema_duplicate_struct_field_message(field.name));
                continue;
            }
            const TypeHandle field_type = this->core_.resolve_type(field.type);
            if (!this->core_.is_valid_storage_type(field_type)) {
                this->core_.report_general(field.range, std::string(SEMA_FIELD_STORAGE));
            }
            if (this->core_.state_.checked.types.contains_array(field_type)) {
                contains_array = true;
            }
            struct_info.fields.push_back(StructFieldInfo{
                this->core_.source_name_text(field.name_id, field.name),
                field.name_id,
                {},
                info.module,
                field_type,
                field.range,
                field.visibility,
                this->core_.stable_member_key(
                    struct_info.stable_id, StableSymbolKind::struct_field, field.name_id, field.name),
            });
        }
    }

    base::Result<std::string> signature_fingerprint =
        this->core_.generic_struct_instance_signature_fingerprint(info, instance_identity.value(), struct_info);
    if (!signature_fingerprint) {
        this->core_.report_internal_contract(use_type.range, signature_fingerprint.error().message);
        return INVALID_TYPE_HANDLE;
    }
    struct_info.incremental_key =
        this->core_.stable_incremental_key(struct_info.stable_id, signature_fingerprint.value());

    this->core_.state_.checked.types.set_record_contains_array(handle, contains_array);
    auto inserted = this->core_.state_.checked.structs.emplace(
        ModuleLookupKey{info.module.value, instance_key_id}, std::move(struct_info));
    if (inserted.second) {
        this->core_.state_.types.struct_infos_by_type[handle.value] = &inserted.first->second;
    }
    return handle;
}

TypeHandle SemanticAnalyzerCore::GenericAnalyzer::instantiate_generic_enum(const GenericTemplateInfo& info,
    const syntax::TypeNode& use_type, const syntax::TypeId, const std::vector<TypeHandle>& args)
{
    if (args.size() != info.params.size()) {
        this->core_.report_type(use_type.range,
            sema_generic_argument_count_message("generic type arguments", info.name, args.size(), info.params.size()));
        return INVALID_TYPE_HANDLE;
    }
    for (const TypeHandle arg : args) {
        if (!is_valid(arg)) {
            return INVALID_TYPE_HANDLE;
        }
    }
    if (!this->core_.validate_generic_arguments(info, args, use_type.range)) {
        return INVALID_TYPE_HANDLE;
    }

    const std::string instance_key = this->core_.generic_enum_instance_key(info, args);
    const IdentId instance_key_id = this->core_.intern_generated_key(instance_key);
    if (const auto found = this->core_.state_.generics.enum_instances.find(instance_key_id);
        found != this->core_.state_.generics.enum_instances.end()) {
        return found->second;
    }

    base::Result<GenericInstanceIdentity> instance_identity =
        this->core_.generic_instance_identity(info, args, query::DefNamespace::type);
    if (!instance_identity) {
        this->core_.report_internal_contract(use_type.range, instance_identity.error().message);
        return INVALID_TYPE_HANDLE;
    }
    const query::GenericInstanceKey instance_query_key = instance_identity.value().key;

    const syntax::ItemNode item = this->core_.ctx_.module.items[info.item.value];
    const std::string abi_suffix = this->core_.generic_instance_abi_suffix(instance_query_key);
    const std::string qualified = this->core_.qualified_name(info.module, item.name);
    const std::string c_name = this->core_.c_symbol_name(info.module, std::string(item.name) + abi_suffix);

    const TypeHandle handle = this->core_.state_.checked.types.named_enum(qualified, c_name);
    this->core_.state_.checked.types.set_generic_instance(
        handle, this->core_.generic_template_key_prefix(info.module, info.name_id, info.name), args);
    this->core_.state_.generics.enum_instances[instance_key_id] = handle;

    GenericEnumInstanceInfo enum_instance;
    enum_instance.key = ModuleLookupKey{info.module.value, instance_key_id};
    enum_instance.item = info.item;
    enum_instance.generic_instance_key = instance_query_key;
    enum_instance.type = handle;
    enum_instance.stable_id = sema::stable_definition_id(
        this->core_.stable_module_id(info.module), StableSymbolKind::type, instance_identity.value().fingerprint_text);
    enum_instance.part_index = info.part_index;

    GenericContext generic_context = this->core_.make_generic_context();
    this->core_.populate_generic_concrete_context(info, args, generic_context);

    {
        GenericAnalysisScope scope(this->core_, info.module, &generic_context, nullptr, false, info.item);
        this->core_.register_enum_cases_for_item(item, info.module, handle, std::string(item.name),
            std::string(item.name) + abi_suffix + "_", std::string(item.name) + abi_suffix + "_", info.visibility,
            instance_query_key);
    }

    base::Result<std::string> signature_fingerprint =
        this->core_.generic_enum_instance_signature_fingerprint(info, instance_identity.value(), handle);
    if (!signature_fingerprint) {
        this->core_.report_internal_contract(use_type.range, signature_fingerprint.error().message);
        return INVALID_TYPE_HANDLE;
    }
    enum_instance.incremental_key =
        this->core_.stable_incremental_key(enum_instance.stable_id, signature_fingerprint.value());
    this->core_.state_.checked.generic_enum_instances.push_back(std::move(enum_instance));
    return handle;
}

TypeHandle SemanticAnalyzerCore::GenericAnalyzer::instantiate_generic_type_alias(const GenericTemplateInfo& info,
    const syntax::TypeNode& use_type, const syntax::TypeId, const std::vector<TypeHandle>& args,
    const bool opaque_allowed_as_pointee)
{
    if (args.size() != info.params.size()) {
        this->core_.report_type(use_type.range,
            sema_generic_argument_count_message("generic type arguments", info.name, args.size(), info.params.size()));
        return INVALID_TYPE_HANDLE;
    }
    for (const TypeHandle arg : args) {
        if (!is_valid(arg)) {
            return INVALID_TYPE_HANDLE;
        }
    }
    if (!this->core_.validate_generic_arguments(info, args, use_type.range)) {
        return INVALID_TYPE_HANDLE;
    }

    const std::string instance_key = this->core_.generic_type_alias_instance_key(info, args);
    const IdentId instance_key_id = this->core_.intern_generated_key(instance_key);
    if (const auto found = this->core_.state_.generics.resolved_type_aliases.find(instance_key_id);
        found != this->core_.state_.generics.resolved_type_aliases.end()) {
        return found->second;
    }
    if (std::ranges::find(
            this->core_.state_.types.resolving_type_aliases, ModuleLookupKey{info.module.value, instance_key_id})
        != this->core_.state_.types.resolving_type_aliases.end()) {
        this->core_.report_general(use_type.range, sema_cyclic_type_alias_message(info.name));
        this->core_.state_.generics.resolved_type_aliases[instance_key_id] = INVALID_TYPE_HANDLE;
        return INVALID_TYPE_HANDLE;
    }

    base::Result<GenericInstanceIdentity> instance_identity =
        this->core_.generic_instance_identity(info, args, query::DefNamespace::type);
    if (!instance_identity) {
        this->core_.report_internal_contract(use_type.range, instance_identity.error().message);
        this->core_.state_.generics.resolved_type_aliases[instance_key_id] = INVALID_TYPE_HANDLE;
        return INVALID_TYPE_HANDLE;
    }
    const query::GenericInstanceKey instance_query_key = instance_identity.value().key;

    const syntax::ItemNode item = this->core_.ctx_.module.items[info.item.value];
    this->core_.state_.types.resolving_type_aliases.push_back(ModuleLookupKey{info.module.value, instance_key_id});
    GenericContext generic_context = this->core_.make_generic_context();
    this->core_.populate_generic_concrete_context(info, args, generic_context);

    const TypeHandle resolved = [&] {
        GenericAnalysisScope scope(this->core_, info.module, &generic_context, nullptr, false, info.item);
        return this->core_.resolve_type(item.alias_type, opaque_allowed_as_pointee);
    }();
    this->core_.state_.types.resolving_type_aliases.pop_back();

    if (is_valid(resolved)) {
        base::Result<std::string> signature_fingerprint =
            this->core_.generic_type_alias_instance_signature_fingerprint(info, instance_identity.value(), resolved);
        if (!signature_fingerprint) {
            this->core_.report_internal_contract(use_type.range, signature_fingerprint.error().message);
            this->core_.state_.generics.resolved_type_aliases[instance_key_id] = INVALID_TYPE_HANDLE;
            return INVALID_TYPE_HANDLE;
        }
        GenericTypeAliasInstanceInfo alias_instance;
        alias_instance.key = ModuleLookupKey{info.module.value, instance_key_id};
        alias_instance.item = info.item;
        alias_instance.generic_instance_key = instance_query_key;
        alias_instance.resolved_type = resolved;
        alias_instance.stable_id = sema::stable_definition_id(this->core_.stable_module_id(info.module),
            StableSymbolKind::type, instance_identity.value().fingerprint_text);
        alias_instance.incremental_key =
            this->core_.stable_incremental_key(alias_instance.stable_id, signature_fingerprint.value());
        alias_instance.part_index = info.part_index;
        this->core_.state_.checked.generic_type_alias_instances.push_back(std::move(alias_instance));
    }
    this->core_.state_.generics.resolved_type_aliases[instance_key_id] = resolved;
    return resolved;
}

bool SemanticAnalyzerCore::GenericAnalyzer::unify_generic_type(const TypeHandle pattern, const TypeHandle actual,
    std::unordered_map<GenericParamIdentity, TypeHandle, GenericParamIdentityHash>& inferred) const
{
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
        const TypeInfo& pattern_info = this->core_.state_.checked.types.get(current_pattern);
        if (pattern_info.kind == TypeKind::generic_param) {
            const GenericParamIdentity identity = this->core_.generic_param_identity(pattern_info);
            const auto found = inferred.find(identity);
            if (found == inferred.end()) {
                inferred.emplace(identity, current_actual);
                continue;
            }
            if (!this->core_.state_.checked.types.same(found->second, current_actual)) {
                return false;
            }
            continue;
        }
        const TypeInfo& actual_info = this->core_.state_.checked.types.get(current_actual);
        if (pattern_info.kind != actual_info.kind) {
            return false;
        }
        switch (pattern_info.kind) {
            case TypeKind::builtin:
            case TypeKind::opaque_struct:
            case TypeKind::associated_projection:
                if (!this->core_.state_.checked.types.same(current_pattern, current_actual)) {
                    return false;
                }
                break;
            case TypeKind::pointer:
            case TypeKind::reference:
                if (pattern_info.pointer_mutability == PointerMutability::mut
                    && actual_info.pointer_mutability != PointerMutability::mut) {
                    return false;
                }
                pending.emplace_back(pattern_info.pointee, actual_info.pointee);
                break;
            case TypeKind::slice:
                if (pattern_info.slice_mutability == PointerMutability::mut
                    && actual_info.slice_mutability != PointerMutability::mut) {
                    return false;
                }
                pending.emplace_back(pattern_info.slice_element, actual_info.slice_element);
                break;
            case TypeKind::function:
                if (pattern_info.function_call_conv != actual_info.function_call_conv
                    || pattern_info.function_is_unsafe != actual_info.function_is_unsafe
                    || pattern_info.function_is_variadic != actual_info.function_is_variadic
                    || pattern_info.function_params.size() != actual_info.function_params.size()) {
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
                if (pattern_info.generic_origin_key.empty()
                    || pattern_info.generic_origin_key != actual_info.generic_origin_key
                    || pattern_info.generic_args.size() != actual_info.generic_args.size()) {
                    if (!this->core_.state_.checked.types.same(current_pattern, current_actual)) {
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

bool SemanticAnalyzerCore::GenericAnalyzer::infer_generic_arguments(
    const GenericTemplateInfo& info, const SemanticAnalyzerCore::ExprView& call, std::vector<TypeHandle>& args)
{
    const syntax::ItemNode function = this->core_.ctx_.module.items[info.item.value];
    if (call.args.size() != function.params.size()) {
        this->core_.report_type(call.range, sema_argument_count_message(info.name));
        return false;
    }

    GenericContext generic_context = this->core_.make_generic_context();
    this->core_.populate_generic_placeholder_context(info, generic_context);

    std::vector<TypeHandle> pattern_param_types;
    pattern_param_types.reserve(function.params.size());
    {
        GenericAnalysisScope scope(this->core_, info.module, &generic_context, nullptr, false, info.item);
        for (const syntax::ParamDecl& param : function.params) {
            pattern_param_types.push_back(this->core_.resolve_type(param.type));
        }
    }

    std::unordered_map<GenericParamIdentity, TypeHandle, GenericParamIdentityHash> inferred;
    for (base::usize i = 0; i < call.args.size(); ++i) {
        const TypeHandle actual = this->core_.analyze_expr(call.args[i], pattern_param_types[i]);
        if (!this->core_.unify_generic_type(pattern_param_types[i], actual, inferred)) {
            this->core_.report_type(this->core_.ctx_.module.exprs.range(call.args[i].value),
                sema_generic_call_argument_unify_message(info.name));
            return false;
        }
    }

    args.clear();
    args.reserve(info.params.size());
    for (base::usize i = 0; i < info.params.size(); ++i) {
        const auto found = inferred.find(this->core_.generic_param_identity(info, i));
        if (found == inferred.end() || !is_valid(found->second)) {
            this->core_.report_type(call.range,
                sema_generic_call_argument_infer_message(this->core_.generic_param_name(info, i), info.name));
            return false;
        }
        args.push_back(found->second);
    }
    if (!this->core_.validate_generic_arguments(info, args, call.range)) {
        return false;
    }
    return true;
}

std::unordered_set<GenericParamIdentity, GenericParamIdentityHash>
SemanticAnalyzerCore::GenericAnalyzer::generic_param_identities_in_type(const TypeHandle type) const
{
    std::unordered_set<GenericParamIdentity, GenericParamIdentityHash> identities;
    std::vector<TypeHandle> pending;
    pending.push_back(type);
    while (!pending.empty()) {
        const TypeHandle current = pending.back();
        pending.pop_back();
        if (!is_valid(current)) {
            continue;
        }
        const TypeInfo& type_info = this->core_.state_.checked.types.get(current);
        switch (type_info.kind) {
            case TypeKind::generic_param:
                identities.insert(this->core_.generic_param_identity(type_info));
                break;
            case TypeKind::associated_projection:
                pending.push_back(type_info.associated_base);
                break;
            case TypeKind::pointer:
            case TypeKind::reference:
                pending.push_back(type_info.pointee);
                break;
            case TypeKind::slice:
                pending.push_back(type_info.slice_element);
                break;
            case TypeKind::array:
                pending.push_back(type_info.array_element);
                break;
            case TypeKind::tuple:
                for (const TypeHandle element : type_info.tuple_elements) {
                    pending.push_back(element);
                }
                break;
            case TypeKind::function:
                pending.push_back(type_info.function_return);
                for (const TypeHandle param : type_info.function_params) {
                    pending.push_back(param);
                }
                break;
            case TypeKind::struct_:
            case TypeKind::enum_:
                for (const TypeHandle arg : type_info.generic_args) {
                    pending.push_back(arg);
                }
                break;
            case TypeKind::builtin:
            case TypeKind::opaque_struct:
                break;
        }
    }
    return identities;
}

std::vector<base::usize> SemanticAnalyzerCore::GenericAnalyzer::method_local_generic_param_indices(
    const GenericTemplateInfo& info) const
{
    const std::unordered_set<GenericParamIdentity, GenericParamIdentityHash> owner_params =
        this->generic_param_identities_in_type(info.impl_type_pattern);
    std::vector<base::usize> local_indices;
    local_indices.reserve(info.params.size());
    for (base::usize i = 0; i < info.params.size(); ++i) {
        if (!owner_params.contains(this->core_.generic_param_identity(info, i))) {
            local_indices.push_back(i);
        }
    }
    return local_indices;
}

bool SemanticAnalyzerCore::GenericAnalyzer::infer_generic_method_arguments(const GenericTemplateInfo& info,
    const TypeHandle owner_type, const SemanticAnalyzerCore::ExprView& call, const base::usize receiver_count,
    std::vector<TypeHandle>& args)
{
    const syntax::ItemNode function = this->core_.ctx_.module.items[info.item.value];
    if (function.params.size() < receiver_count) {
        this->core_.report_type(call.range, sema_argument_count_message(info.name));
        return false;
    }
    const base::usize expected_arg_count = function.params.size() - receiver_count;
    if (call.args.size() != expected_arg_count) {
        this->core_.report_type(call.range, sema_argument_count_message(info.name));
        return false;
    }

    GenericContext generic_context = this->core_.make_generic_context();
    this->core_.populate_generic_placeholder_context(info, generic_context);

    std::vector<TypeHandle> pattern_param_types;
    pattern_param_types.reserve(function.params.size());
    {
        GenericAnalysisScope scope(this->core_, info.module, &generic_context, nullptr, false, info.item);
        for (const syntax::ParamDecl& param : function.params) {
            pattern_param_types.push_back(this->core_.resolve_type(param.type));
        }
    }

    std::unordered_map<GenericParamIdentity, TypeHandle, GenericParamIdentityHash> inferred;
    if (!this->core_.unify_generic_type(info.impl_type_pattern, owner_type, inferred)) {
        return false;
    }
    for (base::usize i = 0; i < call.args.size(); ++i) {
        const TypeHandle pattern = pattern_param_types[i + receiver_count];
        const TypeHandle actual = this->core_.analyze_expr(call.args[i], pattern);
        if (!this->core_.unify_generic_type(pattern, actual, inferred)) {
            const base::SourceRange arg_range =
                syntax::is_valid(call.args[i]) && call.args[i].value < this->core_.ctx_.module.exprs.size()
                ? this->core_.ctx_.module.exprs.range(call.args[i].value)
                : call.range;
            this->core_.report_type(arg_range, sema_generic_call_argument_unify_message(info.name));
            return false;
        }
    }

    args.clear();
    args.reserve(info.params.size());
    for (base::usize i = 0; i < info.params.size(); ++i) {
        const auto found = inferred.find(this->core_.generic_param_identity(info, i));
        if (found == inferred.end() || !is_valid(found->second)) {
            this->core_.report_type(call.range,
                sema_generic_call_argument_infer_message(this->core_.generic_param_name(info, i), info.name));
            return false;
        }
        args.push_back(found->second);
    }
    return this->core_.validate_generic_arguments(info, args, call.range);
}

bool SemanticAnalyzerCore::GenericAnalyzer::apply_explicit_generic_method_arguments(const GenericTemplateInfo& info,
    const TypeHandle owner_type, const std::span<const syntax::TypeId> explicit_type_args,
    const base::SourceRange& use_range, std::vector<TypeHandle>& args)
{
    const std::vector<base::usize> local_indices = this->method_local_generic_param_indices(info);
    if (explicit_type_args.size() != local_indices.size()) {
        this->core_.report_type(use_range,
            sema_generic_argument_count_message(
                "generic method type arguments", info.name, explicit_type_args.size(), local_indices.size()));
        return false;
    }

    std::unordered_map<GenericParamIdentity, TypeHandle, GenericParamIdentityHash> inferred;
    if (!this->core_.unify_generic_type(info.impl_type_pattern, owner_type, inferred)) {
        return false;
    }

    args.assign(info.params.size(), INVALID_TYPE_HANDLE);
    for (base::usize i = 0; i < info.params.size(); ++i) {
        const auto found = inferred.find(this->core_.generic_param_identity(info, i));
        if (found != inferred.end() && is_valid(found->second)) {
            args[i] = found->second;
        }
    }
    for (base::usize i = 0; i < local_indices.size(); ++i) {
        args[local_indices[i]] = this->core_.resolve_type(explicit_type_args[i]);
    }
    for (base::usize i = 0; i < args.size(); ++i) {
        if (!is_valid(args[i])) {
            this->core_.report_type(use_range,
                sema_generic_call_argument_infer_message(this->core_.generic_param_name(info, i), info.name));
            return false;
        }
    }
    return this->core_.validate_generic_arguments(info, args, use_range);
}

FunctionSignature* SemanticAnalyzerCore::GenericAnalyzer::instantiate_generic_placeholder_function(
    const GenericTemplateInfo& info, const std::vector<TypeHandle>& args, const base::SourceRange& use_range)
{
    if (args.size() != info.params.size()) {
        this->core_.report_type(use_range,
            sema_generic_argument_count_message(
                "generic function type arguments", info.name, args.size(), info.params.size()));
        return nullptr;
    }
    const syntax::ItemNode function = this->core_.ctx_.module.items[info.item.value];

    for (base::usize i = 0; i < info.params.size(); ++i) {
        if (!is_valid(args[i])) {
            return nullptr;
        }
    }

    GenericContext generic_context = this->core_.make_generic_context();
    this->core_.populate_generic_concrete_context(info, args, generic_context);

    FunctionSignature signature = this->core_.state_.checked.make_function_signature();
    {
        GenericAnalysisScope scope(this->core_, info.module, &generic_context, nullptr, false, info.item);
        signature.name = this->core_.source_name_text(info.name_id, info.name);
        signature.name_id = info.name_id;
        signature.c_name = signature.name;
        signature.stable_id = info.stable_id;
        signature.generic_args = this->core_.state_.checked.copy_type_handle_list(args);
        signature.module = info.module;
        signature.part_index = info.part_index;
        signature.return_type = syntax::is_valid(function.return_type) ? this->core_.resolve_type(function.return_type)
                                                                       : INVALID_TYPE_HANDLE;
        signature.range = function.range;
        signature.is_unsafe = function.is_unsafe;
        signature.has_definition = true;
        signature.visibility = info.visibility;
        signature.definition_item = info.item;
        for (const syntax::ParamDecl& param : function.params) {
            signature.param_types.push_back(this->core_.resolve_type(param.type));
        }
        signature.incremental_key = this->core_.stable_incremental_key(signature.stable_id,
            this->core_.function_incremental_fingerprint(
                info.name, signature.return_type, signature.param_types, false, signature.is_variadic));
    }

    const IdentId key_id = this->core_.intern_generated_key(this->core_.generic_function_instance_key(info, args));
    const FunctionLookupKey key = this->core_.function_lookup_key(info.module, key_id);
    signature.semantic_key = key;
    const auto inserted = this->core_.state_.generics.placeholder_functions.emplace(key, std::move(signature));
    return &inserted.first->second;
}

bool SemanticAnalyzerCore::GenericAnalyzer::type_contains_generic_param(const TypeHandle type) const
{
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
        const TypeInfo& info = this->core_.state_.checked.types.get(current);
        switch (info.kind) {
            case TypeKind::generic_param:
                return true;
            case TypeKind::associated_projection:
                pending.push_back(info.associated_base);
                break;
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

FunctionSignature* SemanticAnalyzerCore::GenericAnalyzer::instantiate_generic_function(
    const GenericTemplateInfo& info, const std::vector<TypeHandle>& args, const base::SourceRange& use_range)
{
    if (args.size() != info.params.size()) {
        this->core_.report_type(use_range,
            sema_generic_argument_count_message(
                "generic function type arguments", info.name, args.size(), info.params.size()));
        return nullptr;
    }
    for (const TypeHandle arg : args) {
        if (!is_valid(arg)) {
            return nullptr;
        }
    }
    if (!this->core_.validate_generic_arguments(info, args, use_range)) {
        return nullptr;
    }
    if (std::ranges::any_of(args, [&](const TypeHandle arg) {
            return this->core_.type_contains_generic_param(arg);
        })) {
        return this->core_.instantiate_generic_placeholder_function(info, args, use_range);
    }
    const IdentId key_id = this->core_.intern_generated_key(this->core_.generic_function_instance_key(info, args));
    const FunctionLookupKey key = this->core_.function_lookup_key(info.module, key_id);
    if (const auto found = this->core_.state_.generics.function_instances.find(key);
        found != this->core_.state_.generics.function_instances.end()) {
        return &this->core_.state_.checked.generic_function_instances[found->second].signature;
    }
    if (!this->core_.ctx_.options.retain_generic_side_tables) {
        if (const auto found = this->core_.state_.checked.functions.find(key);
            found != this->core_.state_.checked.functions.end()) {
            return &found->second;
        }
    }
    base::Result<GenericInstanceIdentity> instance_identity =
        this->core_.generic_instance_identity(info, args, query::DefNamespace::value);
    if (!instance_identity) {
        this->core_.report_internal_contract(use_range, instance_identity.error().message);
        return nullptr;
    }
    const query::GenericInstanceKey instance_query_key = instance_identity.value().key;

    const syntax::ItemNode function = this->core_.ctx_.module.items[info.item.value];

    GenericContext generic_context = this->core_.make_generic_context();
    this->core_.populate_generic_concrete_context(info, args, generic_context);

    FunctionSignature signature = this->core_.state_.checked.make_function_signature();
    {
        GenericAnalysisScope scope(this->core_, info.module, &generic_context, nullptr, false, info.item);
        signature.name = this->core_.source_name_text(info.name_id, info.name);
        signature.name_id = info.name_id;
        signature.semantic_key = key;
        signature.stable_id = sema::stable_definition_id(this->core_.stable_module_id(info.module),
            StableSymbolKind::function, instance_identity.value().fingerprint_text);
        signature.c_name = this->core_.state_.checked.intern_text(this->core_.c_symbol_name(
            info.module, std::string(info.name.view()) + this->core_.generic_instance_abi_suffix(instance_query_key)));
        signature.generic_instance_key = instance_query_key;
        signature.generic_args = this->core_.state_.checked.copy_type_handle_list(args);
        signature.module = info.module;
        signature.part_index = info.part_index;
        signature.return_type = syntax::is_valid(function.return_type) ? this->core_.resolve_type(function.return_type)
                                                                       : INVALID_TYPE_HANDLE;
        signature.range = function.range;
        signature.is_unsafe = function.is_unsafe;
        signature.has_definition = true;
        signature.visibility = info.visibility;
        signature.definition_item = info.item;
        for (const syntax::ParamDecl& param : function.params) {
            signature.param_types.push_back(this->core_.resolve_type(param.type));
        }
    }

    base::Result<std::string> signature_fingerprint = this->core_.generic_instance_signature_fingerprint(
        info, instance_identity.value(), signature.return_type, signature.param_types, false, signature.is_variadic);
    if (!signature_fingerprint) {
        this->core_.report_internal_contract(use_range, signature_fingerprint.error().message);
        return nullptr;
    }
    signature.incremental_key = this->core_.stable_incremental_key(signature.stable_id, signature_fingerprint.value());

    if (syntax::is_valid(function.return_type) && is_valid(signature.return_type)) {
        this->core_.validate_function_return_type(function, signature.return_type);
    }

    if (!this->core_.ctx_.options.retain_generic_side_tables) {
        const auto function_inserted = this->core_.state_.checked.functions.emplace(key, signature);
        if (!function_inserted.second) {
            function_inserted.first->second = signature;
        } else {
            this->core_.state_.names.internal_function_lookup_exclusions += 1;
        }
        this->core_.state_.functions.body_states[key] = FunctionBodyState::not_started;
        GenericSideTables transient_side_tables = make_generic_side_tables(info);
        GenericContext body_context = this->core_.make_generic_context();
        this->core_.populate_generic_concrete_context(info, args, body_context);
        {
            GenericAnalysisScope scope(
                this->core_, info.module, &body_context, &transient_side_tables, true, info.item);
            this->core_.analyze_function_body_with_signature(
                function, key, function_inserted.first->second, this->core_.state_.functions.body_states[key]);
        }
        return &this->core_.state_.checked.functions.at(key);
    }

    GenericFunctionInstanceInfo instance;
    instance.key = key;
    instance.item = info.item;
    instance.body = function.body;
    instance.generic_instance_key = instance_query_key;
    instance.signature = std::move(signature);
    if (info.has_sparse_node_ids()) {
        instance.side_table_layout_index = this->core_.generic_side_table_layout_index(info);
    }
    instance.side_tables = this->core_.make_generic_instance_side_tables(info);
    const base::usize instance_index = this->core_.state_.checked.generic_function_instances.size();
    this->core_.state_.checked.generic_function_instances.push_back(std::move(instance));
    if (const GenericSideTableLayout* const layout = this->core_.state_.checked.generic_side_table_layout(
            this->core_.state_.checked.generic_function_instances[instance_index].side_table_layout_index);
        layout != nullptr) {
        this->core_.state_.checked.generic_function_instances[instance_index].side_tables.bind_local_dense_layout(
            *layout);
    }
    this->core_.state_.generics.function_instances[key] = instance_index;

    FunctionSignature checked_signature =
        this->core_.state_.checked.generic_function_instances[instance_index].signature;
    const auto function_inserted = this->core_.state_.checked.functions.emplace(key, checked_signature);
    if (!function_inserted.second) {
        function_inserted.first->second = checked_signature;
    } else {
        this->core_.state_.names.internal_function_lookup_exclusions += 1;
    }
    this->core_.state_.functions.body_states[key] = FunctionBodyState::not_started;
    GenericContext body_context = this->core_.make_generic_context();
    this->core_.populate_generic_concrete_context(info, args, body_context);
    {
        GenericAnalysisScope scope(this->core_, info.module, &body_context,
            &this->core_.state_.checked.generic_function_instances[instance_index].side_tables, true, info.item);
        this->core_.analyze_function_body_with_signature(function, key,
            this->core_.state_.checked.generic_function_instances[instance_index].signature,
            this->core_.state_.functions.body_states[key]);
    }
    this->core_.state_.checked.generic_function_instances[instance_index].signature =
        this->core_.state_.checked.functions.at(key);
    this->core_.state_.checked.generic_function_instances[instance_index].side_tables.release_analysis_only_storage();
    return &this->core_.state_.checked.generic_function_instances[instance_index].signature;
}

FunctionSignature* SemanticAnalyzerCore::GenericAnalyzer::instantiate_generic_method(const GenericTemplateInfo& info,
    const TypeHandle owner_type, const std::vector<TypeHandle>& args, const base::SourceRange& use_range)
{
    if (args.size() != info.params.size()) {
        this->core_.report_type(use_range,
            sema_generic_argument_count_message(
                "generic method type arguments", info.name, args.size(), info.params.size()));
        return nullptr;
    }
    for (const TypeHandle arg : args) {
        if (!is_valid(arg)) {
            return nullptr;
        }
    }
    if (!this->core_.validate_generic_arguments(info, args, use_range)) {
        return nullptr;
    }

    std::vector<TypeHandle> method_identity_args;
    method_identity_args.reserve(args.size() + 1U);
    method_identity_args.push_back(owner_type);
    method_identity_args.insert(method_identity_args.end(), args.begin(), args.end());
    const IdentId key_id =
        this->core_.intern_generated_key(this->core_.generic_function_instance_key(info, method_identity_args));
    const FunctionLookupKey key = this->core_.method_function_lookup_key(info.module, owner_type, key_id);
    if (const auto found = this->core_.state_.generics.function_instances.find(key);
        found != this->core_.state_.generics.function_instances.end()) {
        return &this->core_.state_.checked.generic_function_instances[found->second].signature;
    }
    if (!this->core_.ctx_.options.retain_generic_side_tables) {
        if (const auto found = this->core_.state_.checked.functions.find(key);
            found != this->core_.state_.checked.functions.end()) {
            return &found->second;
        }
    }
    base::Result<GenericInstanceIdentity> instance_identity =
        this->core_.generic_instance_identity(info, method_identity_args, query::DefNamespace::member);
    if (!instance_identity) {
        this->core_.report_internal_contract(use_range, instance_identity.error().message);
        return nullptr;
    }
    const query::GenericInstanceKey instance_query_key = instance_identity.value().key;

    const syntax::ItemNode function = this->core_.ctx_.module.items[info.item.value];
    GenericContext generic_context = this->core_.make_generic_context();
    this->core_.populate_generic_concrete_context(info, args, generic_context);

    FunctionSignature signature = this->core_.state_.checked.make_function_signature();
    {
        GenericAnalysisScope scope(this->core_, info.module, &generic_context, nullptr, false, info.item);
        signature.name = this->core_.source_name_text(info.name_id, info.name);
        signature.name_id = info.name_id;
        signature.semantic_key = key;
        signature.stable_id = sema::stable_definition_id(this->core_.stable_module_id(info.module),
            StableSymbolKind::method, instance_identity.value().fingerprint_text);
        signature.c_name = this->core_.state_.checked.intern_text(this->core_.method_c_symbol_name(
            owner_type, std::string(info.name.view()) + this->core_.generic_instance_abi_suffix(instance_query_key)));
        signature.generic_instance_key = instance_query_key;
        signature.generic_args = this->core_.state_.checked.copy_type_handle_list(args);
        signature.module = info.module;
        signature.part_index = info.part_index;
        signature.method_owner_type = owner_type;
        signature.return_type = syntax::is_valid(function.return_type) ? this->core_.resolve_type(function.return_type)
                                                                       : INVALID_TYPE_HANDLE;
        signature.range = function.range;
        signature.is_unsafe = function.is_unsafe;
        signature.has_definition = true;
        signature.is_method = true;
        signature.has_self_param = !function.params.empty() && function.params.front().name == "self";
        signature.visibility = info.visibility;
        signature.definition_item = info.item;
        for (const syntax::ParamDecl& param : function.params) {
            signature.param_types.push_back(this->core_.resolve_type(param.type));
        }
    }

    base::Result<std::string> signature_fingerprint = this->core_.generic_instance_signature_fingerprint(
        info, instance_identity.value(), signature.return_type, signature.param_types, true, signature.is_variadic);
    if (!signature_fingerprint) {
        this->core_.report_internal_contract(use_range, signature_fingerprint.error().message);
        return nullptr;
    }
    signature.incremental_key = this->core_.stable_incremental_key(signature.stable_id, signature_fingerprint.value());

    if (syntax::is_valid(function.return_type) && is_valid(signature.return_type)) {
        this->core_.validate_function_return_type(function, signature.return_type);
    }

    if (!this->core_.ctx_.options.retain_generic_side_tables) {
        const auto function_inserted = this->core_.state_.checked.functions.emplace(key, signature);
        if (!function_inserted.second) {
            function_inserted.first->second = signature;
        } else {
            this->core_.state_.names.internal_function_lookup_exclusions += 1;
        }
        this->core_.state_.functions.body_states[key] = FunctionBodyState::not_started;

        GenericSideTables transient_side_tables = make_generic_side_tables(info);
        GenericContext body_context = this->core_.make_generic_context();
        this->core_.populate_generic_concrete_context(info, args, body_context);
        {
            GenericAnalysisScope scope(
                this->core_, info.module, &body_context, &transient_side_tables, true, info.item);
            this->core_.analyze_function_body_with_signature(
                function, key, function_inserted.first->second, this->core_.state_.functions.body_states[key]);
        }
        return &this->core_.state_.checked.functions.at(key);
    }

    GenericFunctionInstanceInfo instance;
    instance.key = key;
    instance.item = info.item;
    instance.body = function.body;
    instance.generic_instance_key = instance_query_key;
    instance.signature = std::move(signature);
    if (info.has_sparse_node_ids()) {
        instance.side_table_layout_index = this->core_.generic_side_table_layout_index(info);
    }
    instance.side_tables = this->core_.make_generic_instance_side_tables(info);
    const base::usize instance_index = this->core_.state_.checked.generic_function_instances.size();
    this->core_.state_.checked.generic_function_instances.push_back(std::move(instance));
    if (const GenericSideTableLayout* const layout = this->core_.state_.checked.generic_side_table_layout(
            this->core_.state_.checked.generic_function_instances[instance_index].side_table_layout_index);
        layout != nullptr) {
        this->core_.state_.checked.generic_function_instances[instance_index].side_tables.bind_local_dense_layout(
            *layout);
    }
    this->core_.state_.generics.function_instances[key] = instance_index;

    FunctionSignature checked_signature =
        this->core_.state_.checked.generic_function_instances[instance_index].signature;
    const auto function_inserted = this->core_.state_.checked.functions.emplace(key, checked_signature);
    if (!function_inserted.second) {
        function_inserted.first->second = checked_signature;
    } else {
        this->core_.state_.names.internal_function_lookup_exclusions += 1;
    }
    this->core_.state_.functions.body_states[key] = FunctionBodyState::not_started;

    GenericContext body_context = this->core_.make_generic_context();
    this->core_.populate_generic_concrete_context(info, args, body_context);
    {
        GenericAnalysisScope scope(this->core_, info.module, &body_context,
            &this->core_.state_.checked.generic_function_instances[instance_index].side_tables, true, info.item);
        this->core_.analyze_function_body_with_signature(function, key,
            this->core_.state_.checked.generic_function_instances[instance_index].signature,
            this->core_.state_.functions.body_states[key]);
    }
    this->core_.state_.checked.generic_function_instances[instance_index].signature =
        this->core_.state_.checked.functions.at(key);
    this->core_.state_.checked.generic_function_instances[instance_index].side_tables.release_analysis_only_storage();
    return &this->core_.state_.checked.functions.at(key);
}

FunctionSignature* SemanticAnalyzerCore::GenericAnalyzer::find_generic_method_in_visible_modules(
    const TypeHandle owner_type, const IdentId name_id, const std::string_view name, const base::SourceRange& range,
    const bool require_self, const bool report_unknown, const SemanticAnalyzerCore::ExprView* const call,
    const base::usize receiver_count, const bool has_explicit_type_args,
    const std::span<const syntax::TypeId> explicit_type_args, bool* const saw_matching_template)
{
    FunctionSignature* result = nullptr;
    syntax::ModuleId result_module = syntax::INVALID_MODULE_ID;
    bool saw_candidate = false;
    const std::array<syntax::ModuleId, 2> modules{
        this->core_.state_.flow.current_module, this->core_.owner_module(owner_type)};
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
        const ModuleLookupKey lookup_key = this->core_.find_module_lookup_key(module, name_id);
        if (is_valid(lookup_key)) {
            if (const auto found = this->core_.state_.names.generic_method_templates_by_name.find(lookup_key);
                found != this->core_.state_.names.generic_method_templates_by_name.end()) {
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
            if (!this->core_.can_access_module(module, info.visibility)) {
                continue;
            }
            std::unordered_map<GenericParamIdentity, TypeHandle, GenericParamIdentityHash> inferred;
            if (!this->core_.unify_generic_type(info.impl_type_pattern, owner_type, inferred)) {
                continue;
            }
            saw_candidate = true;
            std::vector<TypeHandle> args;
            bool resolved_args = false;
            if (has_explicit_type_args) {
                resolved_args = this->core_.apply_explicit_generic_method_arguments(
                    info, owner_type, explicit_type_args, range, args);
            } else if (call != nullptr && !this->method_local_generic_param_indices(info).empty()) {
                resolved_args =
                    this->core_.infer_generic_method_arguments(info, owner_type, *call, receiver_count, args);
            } else {
                args.reserve(info.params.size());
                resolved_args = true;
                for (base::usize i = 0; i < info.params.size(); ++i) {
                    const auto found = inferred.find(this->core_.generic_param_identity(info, i));
                    if (found == inferred.end() || !is_valid(found->second)) {
                        resolved_args = false;
                        break;
                    }
                    args.push_back(found->second);
                }
            }
            if (!resolved_args) {
                continue;
            }
            FunctionSignature* candidate = this->core_.instantiate_generic_method(info, owner_type, args, range);
            if (candidate == nullptr || (require_self && !candidate->has_self_param)) {
                continue;
            }
            if (result != nullptr) {
                this->core_.report_lookup(range,
                    sema_ambiguous_method_message(this->core_.state_.checked.types.display_name(owner_type), name,
                        this->core_.module_name(result_module), this->core_.module_name(module)));
                return nullptr;
            }
            result = candidate;
            result_module = module;
        }
    }
    if (saw_matching_template != nullptr) {
        *saw_matching_template = saw_candidate;
    }
    if (result == nullptr && report_unknown && !saw_candidate) {
        this->core_.report_lookup(
            range, sema_unknown_method_message(this->core_.state_.checked.types.display_name(owner_type), name));
    }
    return result;
}

void SemanticAnalyzerCore::GenericAnalyzer::analyze_generic_function_definition(const GenericTemplateInfo& info)
{
    const syntax::ItemNode function = this->core_.ctx_.module.items[info.item.value];
    GenericContext generic_context = this->core_.make_generic_context();
    this->core_.populate_generic_placeholder_context(info, generic_context);

    FunctionSignature signature = this->core_.state_.checked.make_function_signature();
    {
        GenericAnalysisScope scope(this->core_, info.module, &generic_context, nullptr, false, info.item);
        signature.name = this->core_.source_name_text(info.name_id, info.name);
        signature.name_id = info.name_id;
        signature.semantic_key = info.function_key;
        signature.stable_id = info.stable_id;
        signature.c_name = signature.name;
        signature.module = info.module;
        signature.part_index = info.part_index;
        signature.return_type = syntax::is_valid(function.return_type) ? this->core_.resolve_type(function.return_type)
                                                                       : INVALID_TYPE_HANDLE;
        signature.range = function.range;
        signature.is_unsafe = function.is_unsafe;
        signature.has_definition = true;
        signature.visibility = info.visibility;
        for (const syntax::ParamDecl& param : function.params) {
            signature.param_types.push_back(this->core_.resolve_type(param.type));
        }
        signature.incremental_key = this->core_.stable_incremental_key(signature.stable_id,
            this->core_.function_incremental_fingerprint(
                info.name, signature.return_type, signature.param_types, false, signature.is_variadic));
        const auto placeholder_inserted =
            this->core_.state_.generics.placeholder_functions.emplace(info.function_key, signature);
        if (!placeholder_inserted.second) {
            placeholder_inserted.first->second = signature;
        }
    }

    FunctionBodyState state = FunctionBodyState::not_started;
    this->core_.analyze_generic_function_body(function, info, signature, state);
}

void SemanticAnalyzerCore::GenericAnalyzer::analyze_generic_function_body(const syntax::ItemNode& function,
    const GenericTemplateInfo& info, const FunctionSignature& signature, FunctionBodyState& state)
{
    GenericContext generic_context = this->core_.make_generic_context();
    this->core_.populate_generic_placeholder_context(info, generic_context);
    GenericSideTables side_tables = make_generic_side_tables(info);
    GenericAnalysisScope scope(this->core_, info.module, &generic_context, &side_tables, true, info.item);
    this->core_.analyze_function_body_with_signature(function, info.function_key, signature, state);
}

bool SemanticAnalyzerCore::has_generic_params(const syntax::ItemNode& item) const noexcept
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).has_generic_params(item);
}

bool SemanticAnalyzerCore::has_generic_constraints(const syntax::ItemNode& item) const noexcept
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).has_generic_constraints(item);
}

void SemanticAnalyzerCore::validate_generic_parameter_list(const syntax::ItemNode& item)
{
    GenericAnalyzer(*this).validate_generic_parameter_list(item);
}

void SemanticAnalyzerCore::validate_generic_constraints(const syntax::ItemNode& item, GenericTemplateInfo& info)
{
    GenericAnalyzer(*this).validate_generic_constraints(item, info);
}

bool SemanticAnalyzerCore::generic_param_has_capability(
    const std::string_view param, const CapabilityKind capability) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).generic_param_has_capability(param, capability);
}

bool SemanticAnalyzerCore::generic_param_has_capability(const TypeHandle param, const CapabilityKind capability) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).generic_param_has_capability(param, capability);
}

bool SemanticAnalyzerCore::type_satisfies_capability(const TypeHandle type, const CapabilityKind capability) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).type_satisfies_capability(type, capability);
}

bool SemanticAnalyzerCore::type_satisfies_equality_capability(const TypeHandle type) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).type_satisfies_equality_capability(type);
}

bool SemanticAnalyzerCore::type_satisfies_ordering_capability(const TypeHandle type) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).type_satisfies_ordering_capability(type);
}

bool SemanticAnalyzerCore::type_supports_equality_operator(const TypeHandle type) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).type_supports_equality_operator(type);
}

bool SemanticAnalyzerCore::type_supports_ordering_operator(const TypeHandle type) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).type_supports_ordering_operator(type);
}

bool SemanticAnalyzerCore::type_supports_hash_capability(const TypeHandle type) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).type_supports_hash_capability(type);
}

bool SemanticAnalyzerCore::validate_generic_arguments(
    const GenericTemplateInfo& info, const std::vector<TypeHandle>& args, const base::SourceRange& use_range)
{
    return GenericAnalyzer(*this).validate_generic_arguments(info, args, use_range);
}

void SemanticAnalyzerCore::populate_generic_template_node_spans(
    GenericTemplateInfo& info, const syntax::ItemNode& item) const
{
    GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).populate_generic_template_node_spans(info, item);
}

std::string SemanticAnalyzerCore::generic_template_incremental_fingerprint(
    const syntax::ItemNode& item, const GenericTemplateInfo& info) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this))
        .generic_template_incremental_fingerprint(item, info);
}

void SemanticAnalyzerCore::record_generic_template_signature(
    const GenericTemplateInfo& info, const query::DefNamespace name_space)
{
    GenericAnalyzer(*this).record_generic_template_signature(info, name_space);
}

GenericSideTables SemanticAnalyzerCore::make_generic_instance_side_tables(const GenericTemplateInfo& info)
{
    return GenericAnalyzer(*this).make_generic_instance_side_tables(info);
}

base::usize SemanticAnalyzerCore::generic_side_table_layout_index(const GenericTemplateInfo& info)
{
    return GenericAnalyzer(*this).generic_side_table_layout_index(info);
}

void SemanticAnalyzerCore::register_generic_template(const syntax::ItemNode& item, const syntax::ItemId item_id)
{
    GenericAnalyzer(*this).register_generic_template(item, item_id);
}

void SemanticAnalyzerCore::populate_generic_param_identities(GenericTemplateInfo& info)
{
    GenericAnalyzer(*this).populate_generic_param_identities(info);
}

GenericParamIdentity SemanticAnalyzerCore::make_generic_param_identity(
    const GenericTemplateInfo& info, const base::usize index) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).make_generic_param_identity(info, index);
}

std::string_view SemanticAnalyzerCore::generic_param_name(
    const GenericTemplateInfo& info, const base::usize index) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).generic_param_name(info, index);
}

GenericParamIdentity SemanticAnalyzerCore::generic_param_identity(
    const GenericTemplateInfo& info, const base::usize index) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).generic_param_identity(info, index);
}

GenericParamIdentity SemanticAnalyzerCore::generic_param_identity(const TypeInfo& info) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).generic_param_identity(info);
}

TypeHandle SemanticAnalyzerCore::generic_param_placeholder(const GenericTemplateInfo& info, const base::usize index)
{
    return GenericAnalyzer(*this).generic_param_placeholder(info, index);
}

void SemanticAnalyzerCore::populate_generic_placeholder_context(
    const GenericTemplateInfo& info, GenericContext& context)
{
    GenericAnalyzer(*this).populate_generic_placeholder_context(info, context);
}

void SemanticAnalyzerCore::populate_generic_concrete_context(
    const GenericTemplateInfo& info, const std::vector<TypeHandle>& args, GenericContext& context) const
{
    GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).populate_generic_concrete_context(info, args, context);
}

std::string SemanticAnalyzerCore::generic_instance_key_suffix(const std::vector<TypeHandle>& args) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).generic_instance_key_suffix(args);
}

std::string SemanticAnalyzerCore::generic_instance_abi_suffix(const query::GenericInstanceKey& key) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).generic_instance_abi_suffix(key);
}

std::string SemanticAnalyzerCore::generic_instance_key(
    const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).generic_instance_key(info, args);
}

std::string SemanticAnalyzerCore::generic_struct_instance_key(
    const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).generic_struct_instance_key(info, args);
}

std::string SemanticAnalyzerCore::generic_enum_instance_key(
    const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).generic_enum_instance_key(info, args);
}

std::string SemanticAnalyzerCore::generic_type_alias_instance_key(
    const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).generic_type_alias_instance_key(info, args);
}

std::string SemanticAnalyzerCore::generic_function_instance_key(
    const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).generic_function_instance_key(info, args);
}

const SemanticAnalyzerCore::GenericTemplateInfo* SemanticAnalyzerCore::find_generic_struct_in_visible_modules(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    return GenericAnalyzer(*this).find_generic_struct_in_visible_modules(name_id, name, range, report_unknown);
}

const SemanticAnalyzerCore::GenericTemplateInfo* SemanticAnalyzerCore::find_generic_struct_in_module(
    const syntax::ModuleId module, const IdentId name_id, const std::string_view name, const base::SourceRange& range,
    const bool report_unknown)
{
    return GenericAnalyzer(*this).find_generic_struct_in_module(module, name_id, name, range, report_unknown);
}

const SemanticAnalyzerCore::GenericTemplateInfo* SemanticAnalyzerCore::find_generic_enum_in_visible_modules(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    return GenericAnalyzer(*this).find_generic_enum_in_visible_modules(name_id, name, range, report_unknown);
}

const SemanticAnalyzerCore::GenericTemplateInfo* SemanticAnalyzerCore::find_generic_enum_in_module(
    const syntax::ModuleId module, const IdentId name_id, const std::string_view name, const base::SourceRange& range,
    const bool report_unknown)
{
    return GenericAnalyzer(*this).find_generic_enum_in_module(module, name_id, name, range, report_unknown);
}

const SemanticAnalyzerCore::GenericTemplateInfo* SemanticAnalyzerCore::find_generic_type_alias_in_visible_modules(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    return GenericAnalyzer(*this).find_generic_type_alias_in_visible_modules(name_id, name, range, report_unknown);
}

const SemanticAnalyzerCore::GenericTemplateInfo* SemanticAnalyzerCore::find_generic_type_alias_in_module(
    const syntax::ModuleId module, const IdentId name_id, const std::string_view name, const base::SourceRange& range,
    const bool report_unknown)
{
    return GenericAnalyzer(*this).find_generic_type_alias_in_module(module, name_id, name, range, report_unknown);
}

bool SemanticAnalyzerCore::generic_type_template_exists_in_module(
    const syntax::ModuleId module, const IdentId name_id, const std::string_view name) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this))
        .generic_type_template_exists_in_module(module, name_id, name);
}

const SemanticAnalyzerCore::GenericTemplateInfo* SemanticAnalyzerCore::find_any_generic_type_template_in_module(
    const syntax::ModuleId module, const IdentId name_id, const std::string_view name) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this))
        .find_any_generic_type_template_in_module(module, name_id, name);
}

bool SemanticAnalyzerCore::report_generic_type_requires_args_if_visible(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range)
{
    return GenericAnalyzer(*this).report_generic_type_requires_args_if_visible(name_id, name, range);
}

void SemanticAnalyzerCore::report_generic_type_template_in_module(
    const syntax::ModuleId module, const IdentId name_id, const std::string_view name, const base::SourceRange& range)
{
    GenericAnalyzer(*this).report_generic_type_template_in_module(module, name_id, name, range);
}

const SemanticAnalyzerCore::GenericTemplateInfo* SemanticAnalyzerCore::find_generic_function_in_visible_modules(
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool report_unknown)
{
    return GenericAnalyzer(*this).find_generic_function_in_visible_modules(name_id, name, range, report_unknown);
}

const SemanticAnalyzerCore::GenericTemplateInfo* SemanticAnalyzerCore::find_generic_function_in_module(
    const syntax::ModuleId module, const IdentId name_id, const std::string_view name, const base::SourceRange& range,
    const bool report_unknown)
{
    return GenericAnalyzer(*this).find_generic_function_in_module(module, name_id, name, range, report_unknown);
}

TypeHandle SemanticAnalyzerCore::instantiate_generic_struct(const GenericTemplateInfo& info,
    const syntax::TypeNode& use_type, const syntax::TypeId use_type_id, const std::vector<TypeHandle>& args)
{
    return GenericAnalyzer(*this).instantiate_generic_struct(info, use_type, use_type_id, args);
}

TypeHandle SemanticAnalyzerCore::instantiate_generic_enum(const GenericTemplateInfo& info,
    const syntax::TypeNode& use_type, const syntax::TypeId use_type_id, const std::vector<TypeHandle>& args)
{
    return GenericAnalyzer(*this).instantiate_generic_enum(info, use_type, use_type_id, args);
}

TypeHandle SemanticAnalyzerCore::instantiate_generic_type_alias(const GenericTemplateInfo& info,
    const syntax::TypeNode& use_type, const syntax::TypeId use_type_id, const std::vector<TypeHandle>& args,
    const bool opaque_allowed_as_pointee)
{
    return GenericAnalyzer(*this).instantiate_generic_type_alias(
        info, use_type, use_type_id, args, opaque_allowed_as_pointee);
}

bool SemanticAnalyzerCore::unify_generic_type(const TypeHandle pattern, const TypeHandle actual,
    std::unordered_map<GenericParamIdentity, TypeHandle, GenericParamIdentityHash>& inferred) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).unify_generic_type(pattern, actual, inferred);
}

bool SemanticAnalyzerCore::infer_generic_arguments(
    const GenericTemplateInfo& info, const SemanticAnalyzerCore::ExprView& call, std::vector<TypeHandle>& args)
{
    return GenericAnalyzer(*this).infer_generic_arguments(info, call, args);
}

bool SemanticAnalyzerCore::infer_generic_method_arguments(const GenericTemplateInfo& info, const TypeHandle owner_type,
    const SemanticAnalyzerCore::ExprView& call, const base::usize receiver_count, std::vector<TypeHandle>& args)
{
    return GenericAnalyzer(*this).infer_generic_method_arguments(info, owner_type, call, receiver_count, args);
}

bool SemanticAnalyzerCore::apply_explicit_generic_method_arguments(const GenericTemplateInfo& info,
    const TypeHandle owner_type, const std::span<const syntax::TypeId> explicit_type_args,
    const base::SourceRange& use_range, std::vector<TypeHandle>& args)
{
    return GenericAnalyzer(*this).apply_explicit_generic_method_arguments(
        info, owner_type, explicit_type_args, use_range, args);
}

FunctionSignature* SemanticAnalyzerCore::instantiate_generic_placeholder_function(
    const GenericTemplateInfo& info, const std::vector<TypeHandle>& args, const base::SourceRange& use_range)
{
    return GenericAnalyzer(*this).instantiate_generic_placeholder_function(info, args, use_range);
}

bool SemanticAnalyzerCore::type_contains_generic_param(const TypeHandle type) const
{
    return GenericAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).type_contains_generic_param(type);
}

FunctionSignature* SemanticAnalyzerCore::instantiate_generic_function(
    const GenericTemplateInfo& info, const std::vector<TypeHandle>& args, const base::SourceRange& use_range)
{
    return GenericAnalyzer(*this).instantiate_generic_function(info, args, use_range);
}

FunctionSignature* SemanticAnalyzerCore::instantiate_generic_method(const GenericTemplateInfo& info,
    const TypeHandle owner_type, const std::vector<TypeHandle>& args, const base::SourceRange& use_range)
{
    return GenericAnalyzer(*this).instantiate_generic_method(info, owner_type, args, use_range);
}

FunctionSignature* SemanticAnalyzerCore::find_generic_method_in_visible_modules(const TypeHandle owner_type,
    const IdentId name_id, const std::string_view name, const base::SourceRange& range, const bool require_self,
    const bool report_unknown, const SemanticAnalyzerCore::ExprView* const call, const base::usize receiver_count,
    const bool has_explicit_type_args, const std::span<const syntax::TypeId> explicit_type_args,
    bool* const saw_matching_template)
{
    return GenericAnalyzer(*this).find_generic_method_in_visible_modules(owner_type, name_id, name, range, require_self,
        report_unknown, call, receiver_count, has_explicit_type_args, explicit_type_args, saw_matching_template);
}

void SemanticAnalyzerCore::analyze_generic_function_definition(const GenericTemplateInfo& info)
{
    GenericAnalyzer(*this).analyze_generic_function_definition(info);
}

void SemanticAnalyzerCore::analyze_generic_function_body(const syntax::ItemNode& function,
    const GenericTemplateInfo& info, const FunctionSignature& signature, FunctionBodyState& state)
{
    GenericAnalyzer(*this).analyze_generic_function_body(function, info, signature, state);
}

} // namespace aurex::sema
