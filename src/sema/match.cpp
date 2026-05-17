#include <aurex/sema/sema.hpp>

#include <aurex/sema/sema_messages.hpp>

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace aurex::sema {

namespace {

constexpr std::string_view SEMA_MATCH_BOOL_TRUE_NAME = "true";
constexpr std::string_view SEMA_MATCH_BOOL_FALSE_NAME = "false";
constexpr base::u64 SEMA_MATCH_U8_DOMAIN_SIZE = 256;
constexpr base::u64 SEMA_MATCH_MATRIX_MAX_ARRAY_COLUMNS = 4096;
constexpr base::usize SEMA_MATCH_SLICE_WITNESS_BOUND_EXTRA = 1;
constexpr base::usize SEMA_MATCH_SYMBOLIC_SLICE_MAX_LENGTH =
    static_cast<base::usize>(SEMA_MATCH_MATRIX_MAX_ARRAY_COLUMNS);

enum class MatchGuardTruth {
    none,
    always_true,
    always_false,
    dynamic,
};

[[nodiscard]] const syntax::PatternNode* pattern_node(
    const syntax::AstModule& module,
    const syntax::PatternId pattern
) noexcept {
    if (!syntax::is_valid(pattern) || pattern.value >= module.patterns.size()) {
        return nullptr;
    }
    return module.patterns.ptr(pattern.value);
}

[[nodiscard]] base::usize enum_case_payload_field_count(const EnumCaseInfo& info) noexcept {
    return info.payload_types.size();
}

[[nodiscard]] TypeHandle enum_case_payload_field_type(const EnumCaseInfo& info, const base::usize index) noexcept {
    return info.payload_types[index];
}

[[nodiscard]] const StructFieldInfo* find_struct_field(
    const StructInfo& info,
    const IdentId name_id
) noexcept {
    for (const StructFieldInfo& field : info.fields) {
        if (field.name_id == name_id) {
            return &field;
        }
    }
    return nullptr;
}

[[nodiscard]] MatchGuardTruth match_guard_truth(
    const syntax::AstModule& module,
    const syntax::ExprId guard
) noexcept {
    if (!syntax::is_valid(guard) || guard.value >= module.exprs.size()) {
        return MatchGuardTruth::none;
    }
    if (module.exprs.kind(guard.value) != syntax::ExprKind::bool_literal) {
        return MatchGuardTruth::dynamic;
    }
    const syntax::LiteralExprPayload* const payload = module.exprs.literal_payload(guard.value);
    if (payload == nullptr) {
        return MatchGuardTruth::dynamic;
    }
    if (payload->text == SEMA_MATCH_BOOL_TRUE_NAME) {
        return MatchGuardTruth::always_true;
    }
    if (payload->text == SEMA_MATCH_BOOL_FALSE_NAME) {
        return MatchGuardTruth::always_false;
    }
    return MatchGuardTruth::dynamic;
}

[[nodiscard]] bool match_type_is_u8(const TypeTable& types, const TypeHandle type) noexcept {
    if (!is_valid(type) || !types.is_integer(type)) {
        return false;
    }
    const TypeInfo& info = types.get(type);
    return info.kind == TypeKind::builtin && info.builtin == BuiltinType::u8;
}

} // namespace

bool SemanticAnalyzer::pattern_is_irrefutable(
    const syntax::PatternId pattern_id,
    const TypeHandle matched
) const {
    struct IrrefutableFrame {
        syntax::PatternId pattern = syntax::INVALID_PATTERN_ID;
        TypeHandle type = INVALID_TYPE_HANDLE;
        bool expanded = false;
    };

    std::vector<IrrefutableFrame> pending;
    std::vector<bool> results(this->module_.patterns.size(), false);
    pending.push_back(IrrefutableFrame {pattern_id, matched, false});
    while (!pending.empty()) {
        const IrrefutableFrame frame = pending.back();
        pending.pop_back();
        const syntax::PatternNode* pattern = pattern_node(this->module_, frame.pattern);
        if (pattern == nullptr) {
            continue;
        }
        if (frame.expanded) {
            bool result = false;
            switch (pattern->kind) {
            case syntax::PatternKind::wildcard:
            case syntax::PatternKind::binding:
                result = true;
                break;
            case syntax::PatternKind::literal:
            case syntax::PatternKind::const_:
            case syntax::PatternKind::enum_case:
                result = false;
                break;
            case syntax::PatternKind::tuple:
                result = this->checked_.types.is_tuple(frame.type);
                for (const syntax::PatternId element : pattern->elements) {
                    result = result &&
                        syntax::is_valid(element) &&
                        element.value < results.size() &&
                        results[element.value];
                }
                break;
            case syntax::PatternKind::slice: {
                const bool array_type = this->checked_.types.is_array(frame.type);
                const bool slice_type = this->checked_.types.is_slice(frame.type);
                result = false;
                if (array_type) {
                    const TypeInfo& array = this->checked_.types.get(frame.type);
                    result = pattern->has_slice_rest
                        ? pattern->elements.size() <= array.array_count
                        : pattern->elements.size() == array.array_count;
                } else if (slice_type) {
                    result = pattern->has_slice_rest && pattern->elements.empty();
                }
                for (const syntax::PatternId element : pattern->elements) {
                    result = result &&
                        syntax::is_valid(element) &&
                        element.value < results.size() &&
                        results[element.value];
                }
                break;
            }
            case syntax::PatternKind::struct_:
                result = this->find_struct(frame.type) != nullptr;
                for (const syntax::FieldPattern& field : pattern->field_patterns) {
                    result = result &&
                        syntax::is_valid(field.pattern) &&
                        field.pattern.value < results.size() &&
                        results[field.pattern.value];
                }
                break;
            case syntax::PatternKind::or_pattern:
                for (const syntax::PatternId alternative : pattern->alternatives) {
                    if (syntax::is_valid(alternative) &&
                        alternative.value < results.size() &&
                        results[alternative.value]) {
                        result = true;
                        break;
                    }
                }
                break;
            }
            results[frame.pattern.value] = result;
            continue;
        }

        pending.push_back(IrrefutableFrame {frame.pattern, frame.type, true});
        switch (pattern->kind) {
        case syntax::PatternKind::tuple: {
            if (!this->checked_.types.is_tuple(frame.type)) {
                break;
            }
            const TypeInfo& tuple = this->checked_.types.get(frame.type);
            const base::usize count = std::min(tuple.tuple_elements.size(), pattern->elements.size());
            for (base::usize i = count; i > 0; --i) {
                pending.push_back(IrrefutableFrame {
                    pattern->elements[i - 1],
                    tuple.tuple_elements[i - 1],
                    false,
                });
            }
            break;
        }
        case syntax::PatternKind::slice: {
            if (!this->checked_.types.is_array(frame.type) && !this->checked_.types.is_slice(frame.type)) {
                break;
            }
            const TypeInfo& info = this->checked_.types.get(frame.type);
            const TypeHandle element_type = info.kind == TypeKind::array
                ? info.array_element
                : info.slice_element;
            for (auto element = pattern->elements.rbegin(); element != pattern->elements.rend(); ++element) {
                pending.push_back(IrrefutableFrame {*element, element_type, false});
            }
            break;
        }
        case syntax::PatternKind::struct_: {
            const StructInfo* info = this->find_struct(frame.type);
            if (info == nullptr) {
                break;
            }
            for (auto field = pattern->field_patterns.rbegin(); field != pattern->field_patterns.rend(); ++field) {
                const StructFieldInfo* field_info = find_struct_field(*info, field->name_id);
                pending.push_back(IrrefutableFrame {
                    field->pattern,
                    field_info == nullptr ? INVALID_TYPE_HANDLE : field_info->type,
                    false,
                });
            }
            break;
        }
        case syntax::PatternKind::or_pattern:
            for (auto alternative = pattern->alternatives.rbegin(); alternative != pattern->alternatives.rend(); ++alternative) {
                pending.push_back(IrrefutableFrame {*alternative, frame.type, false});
            }
            break;
        case syntax::PatternKind::enum_case:
        case syntax::PatternKind::const_:
        case syntax::PatternKind::literal:
        case syntax::PatternKind::wildcard:
        case syntax::PatternKind::binding:
            break;
        }
    }
    return syntax::is_valid(pattern_id) && pattern_id.value < results.size() && results[pattern_id.value];
}

bool SemanticAnalyzer::analyze_pattern(
    const syntax::PatternId pattern_id,
    const TypeHandle matched,
    std::vector<PatternBinding>& bindings
) {
    struct PatternFrame {
        syntax::PatternId pattern = syntax::INVALID_PATTERN_ID;
        TypeHandle type = INVALID_TYPE_HANDLE;
    };

    const auto analyze_pattern_tree = [&](const syntax::PatternId root, const TypeHandle root_type, std::vector<PatternBinding>& out) {
        std::vector<PatternFrame> pending;
        pending.push_back(PatternFrame {root, root_type});
        while (!pending.empty()) {
            const PatternFrame frame = pending.back();
            pending.pop_back();
            const syntax::PatternNode* pattern = pattern_node(this->module_, frame.pattern);
            if (pattern == nullptr) {
                continue;
            }
            switch (pattern->kind) {
            case syntax::PatternKind::wildcard:
                break;
            case syntax::PatternKind::binding:
                out.push_back(PatternBinding {
                    this->source_name_text(pattern->binding_name_id, pattern->binding_name),
                    pattern->binding_name_id,
                    frame.type,
                    pattern->range,
                });
                break;
            case syntax::PatternKind::literal: {
                if (is_valid(frame.type) && this->checked_.types.get(frame.type).kind == TypeKind::enum_) {
                    this->report_pattern(pattern->range, std::string(SEMA_ENUM_MATCH_PATTERN));
                    break;
                }
                bool covered_true = false;
                bool covered_false = false;
                bool saw_wildcard = false;
                this->analyze_single_value_pattern(
                    frame.pattern,
                    frame.type,
                    covered_true,
                    covered_false,
                    saw_wildcard
                );
                break;
            }
            case syntax::PatternKind::const_: {
                if (is_valid(frame.type) && this->checked_.types.get(frame.type).kind == TypeKind::enum_) {
                    this->report_pattern(pattern->range, std::string(SEMA_ENUM_MATCH_PATTERN));
                    break;
                }
                const Symbol* const symbol = this->find_symbol(
                    pattern->binding_name_id,
                    pattern->binding_name,
                    pattern->range
                );
                if (symbol == nullptr) {
                    break;
                }
                if (symbol->kind != SymbolKind::const_) {
                    this->report_unsupported(pattern->range, std::string(SEMA_UNSUPPORTED_LITERAL_PATTERN));
                    break;
                }
                if (!this->checked_.types.same(symbol->type, frame.type)) {
                    this->report_pattern(pattern->range, std::string(SEMA_INTEGER_BOOL_PATTERN));
                    break;
                }
                if (!this->checked_.types.is_integer(frame.type) && !this->checked_.types.is_bool(frame.type)) {
                    this->report_unsupported(pattern->range, std::string(SEMA_UNSUPPORTED_LITERAL_PATTERN));
                    break;
                }
                this->record_pattern_c_name(frame.pattern, symbol->c_name);
                break;
            }
            case syntax::PatternKind::tuple: {
                if (!this->checked_.types.is_tuple(frame.type)) {
                    this->report_pattern(pattern->range, std::string(SEMA_TUPLE_DESTRUCTURE_TYPE));
                    break;
                }
                const TypeInfo& tuple = this->checked_.types.get(frame.type);
                if (tuple.tuple_elements.size() != pattern->elements.size()) {
                    this->report_pattern(pattern->range, std::string(SEMA_TUPLE_DESTRUCTURE_ARITY));
                }
                const base::usize count = std::min(tuple.tuple_elements.size(), pattern->elements.size());
                for (base::usize i = count; i > 0; --i) {
                    pending.push_back(PatternFrame {
                        pattern->elements[i - 1],
                        tuple.tuple_elements[i - 1],
                    });
                }
                for (base::usize i = pattern->elements.size(); i > count; --i) {
                    pending.push_back(PatternFrame {pattern->elements[i - 1], INVALID_TYPE_HANDLE});
                }
                break;
            }
            case syntax::PatternKind::slice: {
                if (!this->checked_.types.is_array(frame.type) && !this->checked_.types.is_slice(frame.type)) {
                    this->report_pattern(pattern->range, std::string(SEMA_SLICE_PATTERN_TYPE));
                    for (auto element = pattern->elements.rbegin(); element != pattern->elements.rend(); ++element) {
                        pending.push_back(PatternFrame {*element, INVALID_TYPE_HANDLE});
                    }
                    break;
                }
                const TypeInfo& info = this->checked_.types.get(frame.type);
                const bool array_type = info.kind == TypeKind::array;
                const TypeHandle element_type = array_type ? info.array_element : info.slice_element;
                if (array_type) {
                    const bool length_ok = pattern->has_slice_rest
                        ? pattern->elements.size() <= info.array_count
                        : pattern->elements.size() == info.array_count;
                    if (!length_ok) {
                        this->report_pattern(pattern->range, std::string(SEMA_SLICE_PATTERN_LENGTH));
                    }
                }
                for (auto element = pattern->elements.rbegin(); element != pattern->elements.rend(); ++element) {
                    pending.push_back(PatternFrame {*element, element_type});
                }
                break;
            }
            case syntax::PatternKind::struct_: {
                const StructInfo* info = this->find_struct(frame.type);
                if (info == nullptr) {
                    this->report_pattern(pattern->range, std::string(SEMA_STRUCT_PATTERN_TYPE));
                    break;
                }
                std::unordered_set<IdentId, IdentIdHash> seen_fields;
                for (auto field = pattern->field_patterns.rbegin(); field != pattern->field_patterns.rend(); ++field) {
                    if (!seen_fields.insert(field->name_id).second) {
                        this->report_pattern(field->range, std::string(SEMA_STRUCT_PATTERN_DUPLICATE_FIELD));
                        continue;
                    }
                    const StructFieldInfo* field_info = find_struct_field(*info, field->name_id);
                    if (field_info == nullptr) {
                        this->report_pattern(field->range, std::string(SEMA_STRUCT_PATTERN_FIELD));
                        this->report_lookup_suggestion(field->range, this->nearest_field_name(*info, field->name));
                        pending.push_back(PatternFrame {field->pattern, INVALID_TYPE_HANDLE});
                        continue;
                    }
                    pending.push_back(PatternFrame {field->pattern, field_info->type});
                }
                break;
            }
            case syntax::PatternKind::enum_case: {
                if (!is_valid(frame.type) || this->checked_.types.get(frame.type).kind != TypeKind::enum_) {
                    this->report_pattern(pattern->range, std::string(SEMA_ENUM_PATTERN_TYPE));
                    for (auto payload = pattern->payload_patterns.rbegin(); payload != pattern->payload_patterns.rend(); ++payload) {
                        pending.push_back(PatternFrame {*payload, INVALID_TYPE_HANDLE});
                    }
                    break;
                }
                const EnumCaseInfo* case_info = nullptr;
                if (pattern->scoped) {
                    if (syntax::is_valid(pattern->enum_type)) {
                        case_info = this->find_enum_case_by_pattern_type(
                            pattern->enum_type,
                            pattern->case_name_id,
                            pattern->case_name,
                            pattern->range
                        );
                    } else if (!pattern->enum_name.empty()) {
                        case_info = this->find_enum_case_by_scoped_name(
                            pattern->enum_name_id,
                            pattern->enum_name,
                            pattern->case_name_id,
                            pattern->case_name,
                            pattern->range
                        );
                    } else {
                        case_info = this->find_enum_case_by_type_and_case(
                            frame.type,
                            pattern->case_name_id,
                            pattern->case_name
                        );
                        if (case_info == nullptr) {
                            this->report_lookup(pattern->range, sema_unknown_matched_enum_case_message(pattern->case_name));
                        }
                    }
                } else {
                    case_info = this->find_enum_case_by_type_and_case(
                        frame.type,
                        pattern->case_name_id,
                        pattern->case_name
                    );
                    if (case_info == nullptr) {
                        this->report_lookup(pattern->range, sema_unknown_matched_enum_case_message(pattern->case_name));
                    }
                }
                if (case_info == nullptr) {
                    for (auto payload = pattern->payload_patterns.rbegin(); payload != pattern->payload_patterns.rend(); ++payload) {
                        pending.push_back(PatternFrame {*payload, INVALID_TYPE_HANDLE});
                    }
                    break;
                }
                if (!this->checked_.types.same(case_info->type, frame.type)) {
                    this->report_pattern(pattern->range, std::string(SEMA_MATCH_CASE_WRONG_ENUM));
                }
                this->record_pattern_c_name(frame.pattern, case_info->c_name);
                this->record_pattern_case_name(frame.pattern, case_info->c_name);
                if (pattern->payload_patterns.empty()) {
                    break;
                }
                if (enum_case_payload_field_count(*case_info) == 0) {
                    this->report_pattern(pattern->range, std::string(SEMA_MATCH_PAYLOAD_CASE));
                    break;
                }
                if (pattern->payload_patterns.size() != enum_case_payload_field_count(*case_info)) {
                    this->report_pattern(
                        pattern->range,
                        sema_enum_payload_pattern_binding_count_message(enum_case_payload_field_count(*case_info))
                    );
                    break;
                }
                for (base::usize i = pattern->payload_patterns.size(); i > 0; --i) {
                    pending.push_back(PatternFrame {
                        pattern->payload_patterns[i - 1],
                        enum_case_payload_field_type(*case_info, i - 1),
                    });
                }
                break;
            }
            case syntax::PatternKind::or_pattern:
                break;
            }
        }
    };

    const syntax::PatternNode* root_pattern = pattern_node(this->module_, pattern_id);
    if (root_pattern == nullptr || root_pattern->kind != syntax::PatternKind::or_pattern) {
        analyze_pattern_tree(pattern_id, matched, bindings);
        return this->pattern_is_irrefutable(pattern_id, matched);
    }

    std::vector<PatternBinding> unified_bindings;
    std::unordered_map<IdentId, PatternBinding, IdentIdHash> expected_bindings;
    bool have_expected = false;
    bool bindings_consistent = true;
    for (const syntax::PatternId alternative : root_pattern->alternatives) {
        std::vector<PatternBinding> alternative_bindings;
        analyze_pattern_tree(alternative, matched, alternative_bindings);

        std::unordered_map<IdentId, PatternBinding, IdentIdHash> actual_bindings;
        for (const PatternBinding& binding : alternative_bindings) {
            actual_bindings.emplace(binding.name_id, binding);
        }
        if (!have_expected) {
            expected_bindings = std::move(actual_bindings);
            unified_bindings = std::move(alternative_bindings);
            have_expected = true;
            continue;
        }
        if (actual_bindings.size() != expected_bindings.size()) {
            const syntax::PatternNode* alternative_pattern = pattern_node(this->module_, alternative);
            this->report_pattern(
                alternative_pattern == nullptr ? root_pattern->range : alternative_pattern->range,
                std::string(SEMA_OR_PATTERN_BINDING_NAMES)
            );
            bindings_consistent = false;
            continue;
        }
        for (const auto& [name, expected] : expected_bindings) {
            const auto actual = actual_bindings.find(name);
            if (actual == actual_bindings.end()) {
                const syntax::PatternNode* alternative_pattern = pattern_node(this->module_, alternative);
                this->report_pattern(
                    alternative_pattern == nullptr ? root_pattern->range : alternative_pattern->range,
                    std::string(SEMA_OR_PATTERN_BINDING_NAMES)
                );
                bindings_consistent = false;
                continue;
            }
            if (is_valid(expected.type) &&
                is_valid(actual->second.type) &&
                !this->checked_.types.same(expected.type, actual->second.type)) {
                this->report_pattern(actual->second.range, std::string(SEMA_OR_PATTERN_BINDING_TYPES));
                bindings_consistent = false;
            }
        }
    }
    if (bindings_consistent) {
        bindings.insert(bindings.end(), unified_bindings.begin(), unified_bindings.end());
    }
    return this->pattern_is_irrefutable(pattern_id, matched);
}

void SemanticAnalyzer::define_pattern_bindings(
    const std::vector<PatternBinding>& bindings,
    const bool is_mutable
) {
    for (const PatternBinding& binding : bindings) {
        if (binding.name.empty()) {
            continue;
        }
        static_cast<void>(this->can_define_local_name(binding.name_id, binding.name, binding.range));
        const auto inserted = this->symbols_.insert(Symbol {
            SymbolKind::local,
            binding.name,
            binding.name_id,
            {},
            syntax::INVALID_MODULE_ID,
            binding.type,
            binding.range,
            is_mutable,
            syntax::Visibility::private_,
            {},
        }, this->diagnostics_);
        static_cast<void>(inserted);
    }
}

struct SemanticAnalyzer::MatchUsefulnessChecker final {
    struct MatrixSlot {
        syntax::PatternId pattern = syntax::INVALID_PATTERN_ID;
        bool wildcard = true;
    };

    struct MatrixConstructor {
        enum class Kind {
            bool_true,
            bool_false,
            integer_literal,
            tuple,
            struct_,
            array,
            enum_case,
        };

        Kind kind = Kind::tuple;
        base::u64 integer_value = 0;
        std::vector<TypeHandle> fields;
        const EnumCaseInfo* enum_case = nullptr;
    };

    using MatrixRow = std::vector<MatrixSlot>;
    using PatternMatrix = std::vector<MatrixRow>;

    struct UsefulnessState {
        PatternMatrix matrix;
        MatrixRow row;
        std::vector<TypeHandle> types;
    };

    struct SliceLengthCandidates {
        std::vector<base::usize> lengths;
        bool overflow = false;
    };

    MatchUsefulnessChecker(
        SemanticAnalyzer& analyzer,
        const TypeHandle matched,
        const bool enum_match
    ) noexcept
        : analyzer_(analyzer),
          matched_(matched),
          enum_match_(enum_match) {}

    void add_unguarded_arm(const syntax::PatternId root) {
        const syntax::PatternNode* pattern = pattern_node(this->analyzer_.module_, root);
        if (pattern == nullptr) {
            return;
        }

        bool arm_is_useful = false;
        std::vector<MatrixRow> rows = this->expand_or_rows(MatrixRow {MatchUsefulnessChecker::pattern_slot(root)});
        for (MatrixRow& row : rows) {
            if (!this->row_is_useful(row)) {
                continue;
            }
            arm_is_useful = true;
            this->coverage_matrix_.push_back(std::move(row));
        }
        if (!arm_is_useful) {
            this->analyzer_.report_pattern_unreachable(pattern->range, std::string(SEMA_MATCH_ARM_UNREACHABLE));
        }
        this->record_enum_case_coverage(root);
    }

    [[nodiscard]] bool has_missing_witness() const {
        return this->matrix_row_is_useful(
            this->coverage_matrix_,
            MatchUsefulnessChecker::wildcard_row(1),
            std::vector<TypeHandle> {this->matched_}
        );
    }

    [[nodiscard]] const EnumCaseInfo* missing_enum_case() const {
        const std::vector<MatrixConstructor> constructors = this->constructors_for_type(this->matched_);
        for (const MatrixConstructor& constructor : constructors) {
            if (constructor.kind != MatrixConstructor::Kind::enum_case ||
                constructor.enum_case == nullptr ||
                !this->enum_case_has_missing_witness(constructor)) {
                continue;
            }
            return constructor.enum_case;
        }
        return nullptr;
    }

private:
    [[nodiscard]] static MatrixSlot wildcard_slot() noexcept {
        return MatrixSlot {};
    }

    [[nodiscard]] static MatrixSlot pattern_slot(const syntax::PatternId pattern) noexcept {
        return MatrixSlot {pattern, false};
    }

    [[nodiscard]] static MatrixRow wildcard_row(const base::usize count) {
        return MatrixRow(count, MatchUsefulnessChecker::wildcard_slot());
    }

    [[nodiscard]] static MatrixRow tail_row(const MatrixRow& row) {
        if (row.empty()) {
            return MatrixRow {};
        }
        return MatrixRow(row.begin() + 1, row.end());
    }

    [[nodiscard]] static std::vector<TypeHandle> tail_types(const std::vector<TypeHandle>& types) {
        if (types.empty()) {
            return std::vector<TypeHandle> {};
        }
        return std::vector<TypeHandle>(types.begin() + 1, types.end());
    }

    [[nodiscard]] static MatrixRow append_row(MatrixRow prefix, const MatrixRow& tail) {
        prefix.insert(prefix.end(), tail.begin(), tail.end());
        return prefix;
    }

    [[nodiscard]] static std::vector<TypeHandle> append_types(
        std::vector<TypeHandle> prefix,
        const std::vector<TypeHandle>& tail
    ) {
        prefix.insert(prefix.end(), tail.begin(), tail.end());
        return prefix;
    }

    [[nodiscard]] static bool same_constructor(
        const MatrixConstructor& lhs,
        const MatrixConstructor& rhs
    ) noexcept {
        if (lhs.kind != rhs.kind) {
            return false;
        }
        if (lhs.kind == MatrixConstructor::Kind::enum_case) {
            return lhs.enum_case == rhs.enum_case;
        }
        if (lhs.kind == MatrixConstructor::Kind::integer_literal) {
            return lhs.integer_value == rhs.integer_value;
        }
        return true;
    }

    [[nodiscard]] const syntax::PatternNode* slot_node(const MatrixSlot& slot) const {
        if (slot.wildcard) {
            return nullptr;
        }
        return pattern_node(this->analyzer_.module_, slot.pattern);
    }

    [[nodiscard]] bool slot_is_any(const MatrixSlot& slot) const {
        if (slot.wildcard) {
            return true;
        }
        const syntax::PatternNode* pattern = this->slot_node(slot);
        return pattern == nullptr ||
            pattern->kind == syntax::PatternKind::wildcard ||
            pattern->kind == syntax::PatternKind::binding;
    }

    [[nodiscard]] std::vector<MatrixConstructor> constructors_for_type(const TypeHandle type) const {
        std::vector<MatrixConstructor> constructors;
        if (!is_valid(type)) {
            return constructors;
        }
        if (this->analyzer_.checked_.types.is_bool(type)) {
            constructors.push_back(MatrixConstructor {MatrixConstructor::Kind::bool_true, 0, {}, nullptr});
            constructors.push_back(MatrixConstructor {MatrixConstructor::Kind::bool_false, 0, {}, nullptr});
            return constructors;
        }

        const TypeInfo& info = this->analyzer_.checked_.types.get(type);
        if (match_type_is_u8(this->analyzer_.checked_.types, type)) {
            constructors.reserve(static_cast<base::usize>(SEMA_MATCH_U8_DOMAIN_SIZE));
            for (base::u64 value = 0; value < SEMA_MATCH_U8_DOMAIN_SIZE; ++value) {
                constructors.push_back(MatrixConstructor {
                    MatrixConstructor::Kind::integer_literal,
                    value,
                    {},
                    nullptr,
                });
            }
            return constructors;
        }
        if (info.kind == TypeKind::tuple) {
            constructors.push_back(MatrixConstructor {
                MatrixConstructor::Kind::tuple,
                0,
                std::vector<TypeHandle>(info.tuple_elements.begin(), info.tuple_elements.end()),
                nullptr,
            });
            return constructors;
        }
        if (info.kind == TypeKind::struct_) {
            const StructInfo* struct_info = this->analyzer_.find_struct(type);
            if (struct_info == nullptr) {
                return constructors;
            }
            MatrixConstructor constructor;
            constructor.kind = MatrixConstructor::Kind::struct_;
            constructor.fields.reserve(struct_info->fields.size());
            for (const StructFieldInfo& field : struct_info->fields) {
                constructor.fields.push_back(field.type);
            }
            constructors.push_back(std::move(constructor));
            return constructors;
        }
        if (info.kind == TypeKind::array && info.array_count <= SEMA_MATCH_MATRIX_MAX_ARRAY_COLUMNS) {
            MatrixConstructor constructor;
            constructor.kind = MatrixConstructor::Kind::array;
            constructor.fields.assign(static_cast<base::usize>(info.array_count), info.array_element);
            constructors.push_back(std::move(constructor));
            return constructors;
        }
        if (info.kind == TypeKind::enum_) {
            std::vector<const EnumCaseInfo*> cases;
            if (const EnumCaseList* indexed_cases =
                    this->analyzer_.find_enum_cases_by_type(type);
                indexed_cases != nullptr) {
                cases.assign(indexed_cases->begin(), indexed_cases->end());
            } else {
                for (const auto& entry : this->analyzer_.checked_.enum_cases) {
                    const EnumCaseInfo& case_info = entry.second;
                    if (this->analyzer_.checked_.types.same(case_info.type, type)) {
                        cases.push_back(&case_info);
                    }
                }
            }
            std::sort(cases.begin(), cases.end(), [](const EnumCaseInfo* lhs, const EnumCaseInfo* rhs) {
                if (lhs == nullptr || rhs == nullptr) {
                    return rhs != nullptr;
                }
                return lhs->c_name.view() < rhs->c_name.view();
            });
            constructors.reserve(cases.size());
            for (const EnumCaseInfo* case_info : cases) {
                if (case_info == nullptr) {
                    continue;
                }
                MatrixConstructor constructor;
                constructor.kind = MatrixConstructor::Kind::enum_case;
                constructor.fields.assign(case_info->payload_types.begin(), case_info->payload_types.end());
                constructor.enum_case = case_info;
                constructors.push_back(std::move(constructor));
            }
        }
        return constructors;
    }

    [[nodiscard]] std::optional<MatrixConstructor> pattern_constructor(
        const MatrixSlot& slot,
        const TypeHandle type
    ) const {
        if (this->slot_is_any(slot)) {
            return std::nullopt;
        }
        const syntax::PatternNode* pattern = this->slot_node(slot);
        if (pattern == nullptr || !is_valid(type)) {
            return std::nullopt;
        }
        if (this->analyzer_.checked_.types.is_bool(type) && pattern->kind == syntax::PatternKind::literal) {
            if (pattern->case_name == SEMA_MATCH_BOOL_TRUE_NAME) {
                return MatrixConstructor {MatrixConstructor::Kind::bool_true, 0, {}, nullptr};
            }
            if (pattern->case_name == SEMA_MATCH_BOOL_FALSE_NAME) {
                return MatrixConstructor {MatrixConstructor::Kind::bool_false, 0, {}, nullptr};
            }
            return std::nullopt;
        }

        const TypeInfo& info = this->analyzer_.checked_.types.get(type);
        if (this->analyzer_.checked_.types.is_integer(type) &&
            pattern->kind == syntax::PatternKind::literal) {
            if (pattern->case_name == SEMA_MATCH_BOOL_TRUE_NAME ||
                pattern->case_name == SEMA_MATCH_BOOL_FALSE_NAME ||
                !this->analyzer_.integer_literal_fits_type(type, pattern->case_name)) {
                return std::nullopt;
            }
            base::u64 value = 0;
            if (!this->analyzer_.parse_integer_literal_text(pattern->case_name, value)) {
                return std::nullopt;
            }
            return MatrixConstructor {
                MatrixConstructor::Kind::integer_literal,
                value,
                {},
                nullptr,
            };
        }
        if (info.kind == TypeKind::tuple && pattern->kind == syntax::PatternKind::tuple) {
            return MatrixConstructor {
                MatrixConstructor::Kind::tuple,
                0,
                std::vector<TypeHandle>(info.tuple_elements.begin(), info.tuple_elements.end()),
                nullptr,
            };
        }
        if (info.kind == TypeKind::struct_ && pattern->kind == syntax::PatternKind::struct_) {
            const StructInfo* struct_info = this->analyzer_.find_struct(type);
            if (struct_info == nullptr) {
                return std::nullopt;
            }
            MatrixConstructor constructor;
            constructor.kind = MatrixConstructor::Kind::struct_;
            constructor.fields.reserve(struct_info->fields.size());
            for (const StructFieldInfo& field : struct_info->fields) {
                constructor.fields.push_back(field.type);
            }
            return constructor;
        }
        if (info.kind == TypeKind::array && pattern->kind == syntax::PatternKind::slice &&
            info.array_count <= SEMA_MATCH_MATRIX_MAX_ARRAY_COLUMNS) {
            MatrixConstructor constructor;
            constructor.kind = MatrixConstructor::Kind::array;
            constructor.fields.assign(static_cast<base::usize>(info.array_count), info.array_element);
            return constructor;
        }
        if (info.kind == TypeKind::enum_ && pattern->kind == syntax::PatternKind::enum_case) {
            const EnumCaseInfo* case_info = this->analyzer_.find_enum_case_by_type_and_case(
                type,
                pattern->case_name_id,
                pattern->case_name
            );
            if (case_info == nullptr) {
                return std::nullopt;
            }
            MatrixConstructor constructor;
            constructor.kind = MatrixConstructor::Kind::enum_case;
            constructor.fields.assign(case_info->payload_types.begin(), case_info->payload_types.end());
            constructor.enum_case = case_info;
            return constructor;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<MatrixRow> specialize_slot(
        const MatrixSlot& slot,
        const MatrixConstructor& constructor,
        const TypeHandle type
    ) const {
        if (this->slot_is_any(slot)) {
            return MatchUsefulnessChecker::wildcard_row(constructor.fields.size());
        }
        const syntax::PatternNode* pattern = this->slot_node(slot);
        if (pattern == nullptr) {
            return std::nullopt;
        }
        const std::optional<MatrixConstructor> slot_constructor = this->pattern_constructor(slot, type);
        if (!slot_constructor.has_value() ||
            !MatchUsefulnessChecker::same_constructor(*slot_constructor, constructor)) {
            return std::nullopt;
        }

        switch (constructor.kind) {
        case MatrixConstructor::Kind::bool_true:
        case MatrixConstructor::Kind::bool_false:
        case MatrixConstructor::Kind::integer_literal:
            return MatrixRow {};
        case MatrixConstructor::Kind::tuple:
            return this->specialize_tuple_pattern(*pattern, constructor);
        case MatrixConstructor::Kind::struct_:
            return this->specialize_struct_pattern(*pattern, type);
        case MatrixConstructor::Kind::array:
            return this->specialize_array_pattern(*pattern, constructor);
        case MatrixConstructor::Kind::enum_case:
            return this->specialize_enum_case_pattern(*pattern, constructor);
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<MatrixRow> specialize_tuple_pattern(
        const syntax::PatternNode& pattern,
        const MatrixConstructor& constructor
    ) const {
        if (pattern.kind != syntax::PatternKind::tuple ||
            pattern.elements.size() != constructor.fields.size()) {
            return std::nullopt;
        }
        MatrixRow row;
        row.reserve(pattern.elements.size());
        for (const syntax::PatternId element : pattern.elements) {
            row.push_back(MatchUsefulnessChecker::pattern_slot(element));
        }
        return row;
    }

    [[nodiscard]] std::optional<MatrixRow> specialize_struct_pattern(
        const syntax::PatternNode& pattern,
        const TypeHandle type
    ) const {
        const StructInfo* struct_info = this->analyzer_.find_struct(type);
        if (struct_info == nullptr) {
            return std::nullopt;
        }
        std::unordered_map<IdentId, syntax::PatternId, IdentIdHash> field_patterns;
        field_patterns.reserve(pattern.field_patterns.size());
        for (const syntax::FieldPattern& field : pattern.field_patterns) {
            field_patterns.emplace(field.name_id, field.pattern);
        }
        MatrixRow row;
        row.reserve(struct_info->fields.size());
        for (const StructFieldInfo& field : struct_info->fields) {
            if (const auto found = field_patterns.find(field.name_id); found != field_patterns.end()) {
                row.push_back(MatchUsefulnessChecker::pattern_slot(found->second));
            } else {
                row.push_back(MatchUsefulnessChecker::wildcard_slot());
            }
        }
        return row;
    }

    [[nodiscard]] std::optional<MatrixRow> specialize_array_pattern(
        const syntax::PatternNode& pattern,
        const MatrixConstructor& constructor
    ) const {
        if (pattern.kind != syntax::PatternKind::slice) {
            return std::nullopt;
        }
        const base::usize array_count = constructor.fields.size();
        if (!pattern.has_slice_rest && pattern.elements.size() != array_count) {
            return std::nullopt;
        }
        if (pattern.has_slice_rest && pattern.elements.size() > array_count) {
            return std::nullopt;
        }
        if (!pattern.has_slice_rest) {
            MatrixRow row;
            row.reserve(pattern.elements.size());
            for (const syntax::PatternId element : pattern.elements) {
                row.push_back(MatchUsefulnessChecker::pattern_slot(element));
            }
            return row;
        }

        const base::usize prefix_count = std::min(pattern.slice_rest_index, pattern.elements.size());
        const base::usize suffix_count = pattern.elements.size() - prefix_count;
        MatrixRow row;
        row.reserve(array_count);
        for (base::usize i = 0; i < array_count; ++i) {
            if (i < prefix_count) {
                row.push_back(MatchUsefulnessChecker::pattern_slot(pattern.elements[i]));
            } else if (i >= array_count - suffix_count) {
                const base::usize suffix_index = prefix_count + (i - (array_count - suffix_count));
                row.push_back(MatchUsefulnessChecker::pattern_slot(pattern.elements[suffix_index]));
            } else {
                row.push_back(MatchUsefulnessChecker::wildcard_slot());
            }
        }
        return row;
    }

    [[nodiscard]] bool slice_pattern_accepts_length(
        const syntax::PatternNode& pattern,
        const base::usize length
    ) const noexcept {
        if (pattern.kind != syntax::PatternKind::slice) {
            return false;
        }
        if (!pattern.has_slice_rest) {
            return pattern.elements.size() == length;
        }
        return pattern.elements.size() <= length;
    }

    [[nodiscard]] std::optional<MatrixRow> specialize_slice_slot_by_length(
        const MatrixSlot& slot,
        const base::usize length
    ) const {
        if (this->slot_is_any(slot)) {
            return MatchUsefulnessChecker::wildcard_row(length);
        }
        const syntax::PatternNode* pattern = this->slot_node(slot);
        if (pattern == nullptr || !this->slice_pattern_accepts_length(*pattern, length)) {
            return std::nullopt;
        }
        if (!pattern->has_slice_rest) {
            MatrixRow row;
            row.reserve(pattern->elements.size());
            for (const syntax::PatternId element : pattern->elements) {
                row.push_back(MatchUsefulnessChecker::pattern_slot(element));
            }
            return row;
        }

        const base::usize prefix_count = std::min(pattern->slice_rest_index, pattern->elements.size());
        const base::usize suffix_count = pattern->elements.size() - prefix_count;
        if (prefix_count + suffix_count > length) {
            return std::nullopt;
        }

        MatrixRow row;
        row.reserve(length);
        for (base::usize i = 0; i < length; ++i) {
            if (i < prefix_count) {
                row.push_back(MatchUsefulnessChecker::pattern_slot(pattern->elements[i]));
            } else if (i >= length - suffix_count) {
                const base::usize suffix_index = prefix_count + (i - (length - suffix_count));
                row.push_back(MatchUsefulnessChecker::pattern_slot(pattern->elements[suffix_index]));
            } else {
                row.push_back(MatchUsefulnessChecker::wildcard_slot());
            }
        }
        return row;
    }

    void collect_slice_length_bound(
        const MatrixSlot& slot,
        base::usize& max_explicit_elements,
        bool& overflow
    ) const noexcept {
        if (this->slot_is_any(slot)) {
            return;
        }
        const syntax::PatternNode* pattern = this->slot_node(slot);
        if (pattern == nullptr || pattern->kind != syntax::PatternKind::slice) {
            return;
        }
        if (pattern->elements.size() > SEMA_MATCH_SYMBOLIC_SLICE_MAX_LENGTH) {
            overflow = true;
            return;
        }
        max_explicit_elements = std::max(max_explicit_elements, pattern->elements.size());
    }

    [[nodiscard]] SliceLengthCandidates slice_length_candidates(
        const PatternMatrix& matrix,
        const MatrixSlot& row_slot
    ) const {
        base::usize max_explicit_elements = 0;
        bool overflow = false;
        this->collect_slice_length_bound(row_slot, max_explicit_elements, overflow);
        for (const MatrixRow& row : matrix) {
            if (row.empty()) {
                continue;
            }
            this->collect_slice_length_bound(row.front(), max_explicit_elements, overflow);
        }
        if (overflow) {
            return SliceLengthCandidates {{}, true};
        }

        const base::usize bound = std::min(
            max_explicit_elements + SEMA_MATCH_SLICE_WITNESS_BOUND_EXTRA,
            SEMA_MATCH_SYMBOLIC_SLICE_MAX_LENGTH
        );
        SliceLengthCandidates candidates;
        candidates.lengths.reserve(bound + 1);
        for (base::usize length = 0; length <= bound; ++length) {
            candidates.lengths.push_back(length);
        }
        return candidates;
    }

    [[nodiscard]] std::vector<TypeHandle> slice_field_types(
        const TypeHandle type,
        const base::usize length
    ) const {
        const TypeInfo& info = this->analyzer_.checked_.types.get(type);
        return std::vector<TypeHandle>(length, info.slice_element);
    }

    [[nodiscard]] PatternMatrix specialize_slice_matrix_by_length(
        const PatternMatrix& matrix,
        const base::usize length
    ) const {
        PatternMatrix specialized;
        for (const MatrixRow& row : matrix) {
            if (row.empty()) {
                continue;
            }
            std::optional<MatrixRow> prefix = this->specialize_slice_slot_by_length(row.front(), length);
            if (!prefix.has_value()) {
                continue;
            }
            std::vector<MatrixRow> expanded_rows = this->expand_or_rows(
                MatchUsefulnessChecker::append_row(std::move(*prefix), MatchUsefulnessChecker::tail_row(row))
            );
            specialized.insert(
                specialized.end(),
                std::make_move_iterator(expanded_rows.begin()),
                std::make_move_iterator(expanded_rows.end())
            );
        }
        return specialized;
    }

    [[nodiscard]] std::optional<MatrixRow> specialize_enum_case_pattern(
        const syntax::PatternNode& pattern,
        const MatrixConstructor& constructor
    ) const {
        if (pattern.kind != syntax::PatternKind::enum_case) {
            return std::nullopt;
        }
        if (pattern.payload_patterns.empty()) {
            return MatchUsefulnessChecker::wildcard_row(constructor.fields.size());
        }
        if (pattern.payload_patterns.size() != constructor.fields.size()) {
            return std::nullopt;
        }
        MatrixRow row;
        row.reserve(pattern.payload_patterns.size());
        for (const syntax::PatternId payload : pattern.payload_patterns) {
            row.push_back(MatchUsefulnessChecker::pattern_slot(payload));
        }
        return row;
    }

    [[nodiscard]] std::vector<MatrixRow> expand_or_rows(MatrixRow row) const {
        std::vector<MatrixRow> expanded;
        std::vector<MatrixRow> pending;
        pending.push_back(std::move(row));
        while (!pending.empty()) {
            MatrixRow current = std::move(pending.back());
            pending.pop_back();
            bool split = false;
            for (base::usize slot_index = 0; slot_index < current.size(); ++slot_index) {
                const syntax::PatternNode* pattern = this->slot_node(current[slot_index]);
                if (pattern == nullptr || pattern->kind != syntax::PatternKind::or_pattern) {
                    continue;
                }
                for (const syntax::PatternId alternative : pattern->alternatives) {
                    MatrixRow alternative_row = current;
                    alternative_row[slot_index] = MatchUsefulnessChecker::pattern_slot(alternative);
                    pending.push_back(std::move(alternative_row));
                }
                split = true;
                break;
            }
            if (split) {
                continue;
            }
            expanded.push_back(std::move(current));
        }
        return expanded;
    }

    [[nodiscard]] PatternMatrix expand_or_matrix(const PatternMatrix& matrix) const {
        PatternMatrix expanded;
        for (const MatrixRow& row : matrix) {
            std::vector<MatrixRow> rows = this->expand_or_rows(row);
            expanded.insert(
                expanded.end(),
                std::make_move_iterator(rows.begin()),
                std::make_move_iterator(rows.end())
            );
        }
        return expanded;
    }

    [[nodiscard]] bool row_is_irrefutable_for_types(
        const MatrixRow& row,
        const std::vector<TypeHandle>& types
    ) const {
        if (row.size() != types.size()) {
            return false;
        }
        for (base::usize i = 0; i < row.size(); ++i) {
            if (this->slot_is_any(row[i])) {
                continue;
            }
            if (!this->analyzer_.pattern_is_irrefutable(row[i].pattern, types[i])) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool matrix_has_irrefutable_row(
        const PatternMatrix& matrix,
        const std::vector<TypeHandle>& types
    ) const {
        for (const MatrixRow& row : matrix) {
            if (this->row_is_irrefutable_for_types(row, types)) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] PatternMatrix specialize_matrix(
        const PatternMatrix& matrix,
        const MatrixConstructor& constructor,
        const TypeHandle type
    ) const {
        PatternMatrix specialized;
        for (const MatrixRow& row : matrix) {
            if (row.empty()) {
                continue;
            }
            std::optional<MatrixRow> prefix = this->specialize_slot(row.front(), constructor, type);
            if (!prefix.has_value()) {
                continue;
            }
            std::vector<MatrixRow> expanded_rows = this->expand_or_rows(
                MatchUsefulnessChecker::append_row(std::move(*prefix), MatchUsefulnessChecker::tail_row(row))
            );
            specialized.insert(
                specialized.end(),
                std::make_move_iterator(expanded_rows.begin()),
                std::make_move_iterator(expanded_rows.end())
            );
        }
        return specialized;
    }

    [[nodiscard]] PatternMatrix default_matrix(const PatternMatrix& matrix) const {
        PatternMatrix result_matrix;
        for (const MatrixRow& row : matrix) {
            if (row.empty() || !this->slot_is_any(row.front())) {
                continue;
            }
            result_matrix.push_back(MatchUsefulnessChecker::tail_row(row));
        }
        return result_matrix;
    }

    [[nodiscard]] bool first_column_covers_constructor(
        const PatternMatrix& matrix,
        const MatrixConstructor& constructor,
        const TypeHandle type
    ) const {
        for (const MatrixRow& row : matrix) {
            if (row.empty()) {
                continue;
            }
            if (this->slot_is_any(row.front())) {
                return true;
            }
            const std::optional<MatrixConstructor> row_constructor = this->pattern_constructor(row.front(), type);
            if (row_constructor.has_value() &&
                MatchUsefulnessChecker::same_constructor(*row_constructor, constructor)) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool matrix_row_is_useful(
        const PatternMatrix& initial_matrix,
        const MatrixRow& initial_row,
        std::vector<TypeHandle> initial_types
    ) const {
        std::vector<UsefulnessState> pending;
        pending.push_back(UsefulnessState {
            this->expand_or_matrix(initial_matrix),
            initial_row,
            std::move(initial_types),
        });
        while (!pending.empty()) {
            UsefulnessState state = std::move(pending.back());
            pending.pop_back();

            std::vector<MatrixRow> expanded_rows = this->expand_or_rows(std::move(state.row));
            if (expanded_rows.size() > 1) {
                for (MatrixRow& row : expanded_rows) {
                    pending.push_back(UsefulnessState {state.matrix, std::move(row), state.types});
                }
                continue;
            }
            state.row = expanded_rows.empty() ? MatrixRow {} : std::move(expanded_rows.front());

            if (state.row.empty() || state.types.empty()) {
                if (state.matrix.empty()) {
                    return true;
                }
                continue;
            }
            if (this->matrix_has_irrefutable_row(state.matrix, state.types)) {
                continue;
            }

            const TypeHandle column_type = state.types.front();
            const MatrixSlot column_pattern = state.row.front();
            if (this->analyzer_.checked_.types.is_slice(column_type)) {
                const SliceLengthCandidates candidates = this->slice_length_candidates(state.matrix, column_pattern);
                if (candidates.overflow) {
                    return true;
                }
                bool considered_length = false;
                for (auto length = candidates.lengths.rbegin(); length != candidates.lengths.rend(); ++length) {
                    std::optional<MatrixRow> specialized_row =
                        this->specialize_slice_slot_by_length(column_pattern, *length);
                    if (!specialized_row.has_value()) {
                        continue;
                    }
                    considered_length = true;
                    pending.push_back(UsefulnessState {
                        this->specialize_slice_matrix_by_length(state.matrix, *length),
                        MatchUsefulnessChecker::append_row(
                            std::move(*specialized_row),
                            MatchUsefulnessChecker::tail_row(state.row)
                        ),
                        MatchUsefulnessChecker::append_types(
                            this->slice_field_types(column_type, *length),
                            MatchUsefulnessChecker::tail_types(state.types)
                        ),
                    });
                }
                if (considered_length) {
                    continue;
                }
                continue;
            }
            if (std::optional<MatrixConstructor> constructor = this->pattern_constructor(column_pattern, column_type);
                constructor.has_value()) {
                std::optional<MatrixRow> specialized_row =
                    this->specialize_slot(column_pattern, *constructor, column_type);
                if (!specialized_row.has_value()) {
                    continue;
                }
                pending.push_back(UsefulnessState {
                    this->specialize_matrix(state.matrix, *constructor, column_type),
                    MatchUsefulnessChecker::append_row(
                        std::move(*specialized_row),
                        MatchUsefulnessChecker::tail_row(state.row)
                    ),
                    MatchUsefulnessChecker::append_types(
                        constructor->fields,
                        MatchUsefulnessChecker::tail_types(state.types)
                    ),
                });
                continue;
            }

            const std::vector<MatrixConstructor> constructors = this->constructors_for_type(column_type);
            if (!constructors.empty()) {
                bool missing_constructor = false;
                for (const MatrixConstructor& constructor : constructors) {
                    if (!this->first_column_covers_constructor(state.matrix, constructor, column_type)) {
                        missing_constructor = true;
                        break;
                    }
                }
                if (missing_constructor) {
                    return true;
                }
                for (auto constructor = constructors.rbegin(); constructor != constructors.rend(); ++constructor) {
                    pending.push_back(UsefulnessState {
                        this->specialize_matrix(state.matrix, *constructor, column_type),
                        MatchUsefulnessChecker::append_row(
                            MatchUsefulnessChecker::wildcard_row(constructor->fields.size()),
                            MatchUsefulnessChecker::tail_row(state.row)
                        ),
                        MatchUsefulnessChecker::append_types(
                            constructor->fields,
                            MatchUsefulnessChecker::tail_types(state.types)
                        ),
                    });
                }
                continue;
            }

            pending.push_back(UsefulnessState {
                this->default_matrix(state.matrix),
                MatchUsefulnessChecker::tail_row(state.row),
                MatchUsefulnessChecker::tail_types(state.types),
            });
        }
        return false;
    }

    [[nodiscard]] bool row_is_useful(const MatrixRow& row) const {
        return this->matrix_row_is_useful(
            this->coverage_matrix_,
            row,
            std::vector<TypeHandle> {this->matched_}
        );
    }

    [[nodiscard]] bool enum_case_has_missing_witness(const MatrixConstructor& constructor) const {
        return this->matrix_row_is_useful(
            this->specialize_matrix(this->coverage_matrix_, constructor, this->matched_),
            MatchUsefulnessChecker::wildcard_row(constructor.fields.size()),
            constructor.fields
        );
    }

    [[nodiscard]] bool enum_case_payloads_cover_case(const syntax::PatternNode& enum_pattern) const {
        if (enum_pattern.payload_patterns.empty()) {
            return true;
        }
        const EnumCaseInfo* case_info = this->analyzer_.find_enum_case_by_type_and_case(
            this->matched_,
            enum_pattern.case_name_id,
            enum_pattern.case_name
        );
        if (case_info == nullptr || case_info->payload_types.size() != enum_pattern.payload_patterns.size()) {
            return false;
        }
        for (base::usize i = 0; i < enum_pattern.payload_patterns.size(); ++i) {
            if (!this->analyzer_.pattern_is_irrefutable(enum_pattern.payload_patterns[i], case_info->payload_types[i])) {
                return false;
            }
        }
        return true;
    }

    void record_enum_case_coverage(const syntax::PatternId root) {
        std::vector<syntax::PatternId> pending;
        pending.push_back(root);
        while (!pending.empty()) {
            const syntax::PatternId current = pending.back();
            pending.pop_back();
            const syntax::PatternNode* current_pattern = pattern_node(this->analyzer_.module_, current);
            if (current_pattern == nullptr) {
                continue;
            }
            if (current_pattern->kind == syntax::PatternKind::or_pattern) {
                for (const syntax::PatternId alternative : current_pattern->alternatives) {
                    pending.push_back(alternative);
                }
                continue;
            }
            if (!this->enum_match_ ||
                current_pattern->kind != syntax::PatternKind::enum_case ||
                !this->enum_case_payloads_cover_case(*current_pattern)) {
                continue;
            }
            const std::string_view pattern_name = this->analyzer_.cached_pattern_c_name(current);
            if (pattern_name.empty()) {
                continue;
            }
            if (std::find(this->covered_enum_cases_.begin(), this->covered_enum_cases_.end(), pattern_name) !=
                this->covered_enum_cases_.end()) {
                this->analyzer_.report_pattern_unreachable(
                    current_pattern->range,
                    sema_duplicate_match_enum_case_message(current_pattern->case_name)
                );
            } else {
                this->covered_enum_cases_.emplace_back(pattern_name);
            }
        }
    }

    SemanticAnalyzer& analyzer_;
    TypeHandle matched_ = INVALID_TYPE_HANDLE;
    bool enum_match_ = false;
    std::vector<std::string_view> covered_enum_cases_;
    PatternMatrix coverage_matrix_;
};

TypeHandle SemanticAnalyzer::analyze_match_expr(
    const syntax::ExprId expr_id,
    const SemanticAnalyzer::ExprView& expr,
    const TypeHandle expected_type
) {
    if (this->in_const_initializer_) {
        this->report_pattern(expr.range, std::string(SEMA_MATCH_CONST_INITIALIZER));
    }
    const TypeHandle matched = this->analyze_expr(expr.match_value);
    const bool enum_match = is_valid(matched) && this->checked_.types.get(matched).kind == TypeKind::enum_;
    const bool literal_match = is_valid(matched) &&
        (this->checked_.types.is_integer(matched) || this->checked_.types.is_bool(matched));
    const bool tuple_match = is_valid(matched) && this->checked_.types.is_tuple(matched);
    const bool struct_match = this->find_struct(matched) != nullptr;
    const bool array_match = is_valid(matched) && this->checked_.types.is_array(matched);
    const bool slice_match = is_valid(matched) && this->checked_.types.is_slice(matched);
    if (!enum_match && !literal_match && !tuple_match && !struct_match && !array_match && !slice_match) {
        this->report_pattern(expr.range, std::string(SEMA_MATCH_VALUE_TYPE));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (expr.match_arms.empty()) {
        this->report_pattern(expr.range, std::string(SEMA_MATCH_ARM_REQUIRED));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    TypeHandle result = INVALID_TYPE_HANDLE;
    TypeHandle intrinsic_result = INVALID_TYPE_HANDLE;
    bool intrinsic_result_conflicted = false;
    std::vector<base::SourceRange> pending_null_arm_ranges;
    const auto resolve_pending_null_arms = [&]() {
        if (!is_valid(result) || pending_null_arm_ranges.empty()) {
            return;
        }
        if (!this->checked_.types.is_pointer(result)) {
            for (const base::SourceRange range : pending_null_arm_ranges) {
                this->report_type(range, std::string(SEMA_MATCH_ARM_TYPE));
            }
        }
        pending_null_arm_ranges.clear();
    };
    MatchUsefulnessChecker usefulness(*this, matched, enum_match);
    for (const syntax::MatchArm& arm : expr.match_arms) {
        const MatchGuardTruth guard_truth = match_guard_truth(this->module_, arm.guard);
        const bool guarded = guard_truth != MatchGuardTruth::none;
        std::vector<PatternBinding> bindings;
        static_cast<void>(this->analyze_pattern(arm.pattern, matched, bindings));
        if (guard_truth == MatchGuardTruth::none || guard_truth == MatchGuardTruth::always_true) {
            usefulness.add_unguarded_arm(arm.pattern);
        }
        const auto analyze_guard_and_arm_value = [&]() {
            if (guarded) {
                const TypeHandle guard_type = this->analyze_expr(arm.guard);
                if (!this->checked_.types.is_bool(guard_type)) {
                    this->report_pattern(
                        this->module_.exprs.range(arm.guard.value),
                        std::string(SEMA_MATCH_GUARD_BOOL)
                    );
                }
            }
            const TypeHandle arm_expected = is_valid(result) ? result : expected_type;
            return this->analyze_expr(arm.value, arm_expected);
        };
        TypeHandle arm_type = INVALID_TYPE_HANDLE;
        if (!bindings.empty()) {
            this->symbols_.push_scope(bindings.size());
            this->define_pattern_bindings(bindings, false);
            arm_type = analyze_guard_and_arm_value();
            this->symbols_.pop_scope();
        } else {
            arm_type = analyze_guard_and_arm_value();
        }
        const bool null_result_arm = this->is_null_result_expr(arm.value);
        if (!is_valid(arm_type) && null_result_arm && this->checked_.types.is_pointer(result)) {
            arm_type = this->analyze_expr(arm.value, result);
        }
        if (!is_valid(arm_type) && null_result_arm && !is_valid(result)) {
            pending_null_arm_ranges.push_back(this->module_.exprs.range(arm.value.value));
            continue;
        }
        const TypeHandle arm_intrinsic = this->cached_expr_intrinsic_type(arm.value);
        if (is_valid(arm_intrinsic)) {
            if (!is_valid(intrinsic_result) && !intrinsic_result_conflicted) {
                intrinsic_result = arm_intrinsic;
            } else if (is_valid(intrinsic_result) &&
                       !this->checked_.types.same(intrinsic_result, arm_intrinsic)) {
                intrinsic_result = INVALID_TYPE_HANDLE;
                intrinsic_result_conflicted = true;
            }
        }
        if (!is_valid(result)) {
            result = arm_type;
            resolve_pending_null_arms();
        } else if (!this->checked_.types.same(result, arm_type)) {
            this->report_type_mismatch(arm.range, std::string(SEMA_MATCH_ARM_TYPE), result, arm_type);
        }
    }

    if (usefulness.has_missing_witness()) {
        if (enum_match) {
            if (const EnumCaseInfo* missing_case = usefulness.missing_enum_case(); missing_case != nullptr) {
                this->report_pattern_exhaustiveness(
                    expr.range,
                    sema_match_missing_enum_case_message(
                        enum_case_display_name(this->checked_.types, *missing_case)
                    )
                );
            } else {
                this->report_pattern_exhaustiveness(expr.range, std::string(SEMA_MATCH_NON_ENUM_IRREFUTABLE));
            }
        } else if (literal_match) {
            if (this->checked_.types.is_integer(matched) && !match_type_is_u8(this->checked_.types, matched)) {
                this->report_pattern_exhaustiveness(expr.range, std::string(SEMA_MATCH_OPEN_INTEGER_WILDCARD));
            } else {
                this->report_pattern_exhaustiveness(expr.range, std::string(SEMA_MATCH_INTEGER_BOOL_WILDCARD));
            }
        } else if (slice_match) {
            this->report_pattern_exhaustiveness(expr.range, std::string(SEMA_MATCH_DYNAMIC_SLICE_WITNESS));
        } else if (tuple_match || struct_match || array_match) {
            this->report_pattern_exhaustiveness(expr.range, std::string(SEMA_MATCH_NON_ENUM_IRREFUTABLE));
        }
    }
    if (is_valid(result) && this->checked_.types.is_void(result)) {
        this->report_pattern(expr.range, std::string(SEMA_MATCH_RESULT_VOID));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (!is_valid(intrinsic_result) && !intrinsic_result_conflicted && !is_valid(expected_type)) {
        intrinsic_result = result;
    }
    return this->record_expr_types(expr_id, intrinsic_result, result);
}

void SemanticAnalyzer::analyze_single_value_pattern(
    const syntax::PatternId pattern_id,
    const TypeHandle matched,
    bool& covered_true,
    bool& covered_false,
    bool& saw_wildcard
) const
{
    if (!syntax::is_valid(pattern_id) || pattern_id.value >= this->module_.patterns.size()) {
        return;
    }
    const syntax::PatternNode& pattern = this->module_.patterns[pattern_id.value];
    if (saw_wildcard) {
        this->report_pattern_unreachable(pattern.range, std::string(SEMA_MATCH_ARM_UNREACHABLE));
    }
    if (pattern.kind == syntax::PatternKind::wildcard) {
        saw_wildcard = true;
        return;
    }
    if (pattern.kind != syntax::PatternKind::literal) {
        this->report_pattern(pattern.range, std::string(SEMA_INTEGER_BOOL_PATTERN));
        return;
    }
    if (this->checked_.types.is_bool(matched)) {
        if (pattern.case_name != SEMA_MATCH_BOOL_TRUE_NAME && pattern.case_name != SEMA_MATCH_BOOL_FALSE_NAME) {
            this->report_pattern(pattern.range, std::string(SEMA_BOOL_PATTERN));
        } else if (pattern.case_name == SEMA_MATCH_BOOL_TRUE_NAME) {
            covered_true = true;
        } else {
            covered_false = true;
        }
        return;
    }
    if (this->checked_.types.is_integer(matched)) {
        if (pattern.case_name == SEMA_MATCH_BOOL_TRUE_NAME || pattern.case_name == SEMA_MATCH_BOOL_FALSE_NAME) {
            this->report_pattern(pattern.range, std::string(SEMA_INTEGER_PATTERN));
        } else if (!this->integer_literal_fits_type(matched, pattern.case_name)) {
            this->report_pattern(pattern.range, std::string(SEMA_INTEGER_PATTERN_RANGE));
        }
        return;
    }
    this->report_unsupported(pattern.range, std::string(SEMA_UNSUPPORTED_LITERAL_PATTERN));
}

} // namespace aurex::sema
