#include <aurex/sema/sema.hpp>

#include <aurex/sema/sema_messages.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace aurex::sema {

namespace {

constexpr std::string_view SEMA_MATCH_BOOL_TRUE_NAME = "true";
constexpr std::string_view SEMA_MATCH_BOOL_FALSE_NAME = "false";
constexpr base::u64 SEMA_MATCH_STRUCTURAL_ARRAY_MAX_ELEMENTS = 8;

[[nodiscard]] const syntax::PatternNode* pattern_node(
    const syntax::AstModule& module,
    const syntax::PatternId pattern
) noexcept {
    if (!syntax::is_valid(pattern) || pattern.value >= module.patterns.size()) {
        return nullptr;
    }
    return &module.patterns[pattern.value];
}

[[nodiscard]] base::usize enum_case_payload_field_count(const EnumCaseInfo& info) noexcept {
    return info.payload_types.size();
}

[[nodiscard]] TypeHandle enum_case_payload_field_type(const EnumCaseInfo& info, const base::usize index) noexcept {
    return info.payload_types[index];
}

[[nodiscard]] const StructFieldInfo* find_struct_field(
    const StructInfo& info,
    const std::string_view name
) noexcept {
    for (const StructFieldInfo& field : info.fields) {
        if (field.name == name) {
            return &field;
        }
    }
    return nullptr;
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
                const StructFieldInfo* field_info = find_struct_field(*info, field->name);
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
                    std::string(pattern->binding_name),
                    frame.type,
                    pattern->range,
                });
                break;
            case syntax::PatternKind::literal: {
                if (is_valid(frame.type) && this->checked_.types.get(frame.type).kind == TypeKind::enum_) {
                    this->report(pattern->range, std::string(SEMA_ENUM_MATCH_PATTERN));
                    break;
                }
                bool covered_true = false;
                bool covered_false = false;
                bool saw_wildcard = false;
                static_cast<void>(this->analyze_single_value_pattern(
                    frame.pattern,
                    frame.type,
                    covered_true,
                    covered_false,
                    saw_wildcard
                ));
                break;
            }
            case syntax::PatternKind::tuple: {
                if (!this->checked_.types.is_tuple(frame.type)) {
                    this->report(pattern->range, std::string(SEMA_TUPLE_DESTRUCTURE_TYPE));
                    break;
                }
                const TypeInfo& tuple = this->checked_.types.get(frame.type);
                if (tuple.tuple_elements.size() != pattern->elements.size()) {
                    this->report(pattern->range, std::string(SEMA_TUPLE_DESTRUCTURE_ARITY));
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
                    this->report(pattern->range, std::string(SEMA_SLICE_PATTERN_TYPE));
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
                        this->report(pattern->range, std::string(SEMA_SLICE_PATTERN_LENGTH));
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
                    this->report(pattern->range, std::string(SEMA_STRUCT_PATTERN_TYPE));
                    break;
                }
                std::unordered_set<std::string_view> seen_fields;
                for (auto field = pattern->field_patterns.rbegin(); field != pattern->field_patterns.rend(); ++field) {
                    if (!seen_fields.insert(field->name).second) {
                        this->report(field->range, std::string(SEMA_STRUCT_PATTERN_DUPLICATE_FIELD));
                        continue;
                    }
                    const StructFieldInfo* field_info = find_struct_field(*info, field->name);
                    if (field_info == nullptr) {
                        this->report(field->range, std::string(SEMA_STRUCT_PATTERN_FIELD));
                        pending.push_back(PatternFrame {field->pattern, INVALID_TYPE_HANDLE});
                        continue;
                    }
                    pending.push_back(PatternFrame {field->pattern, field_info->type});
                }
                break;
            }
            case syntax::PatternKind::enum_case: {
                if (!is_valid(frame.type) || this->checked_.types.get(frame.type).kind != TypeKind::enum_) {
                    this->report(pattern->range, std::string(SEMA_ENUM_PATTERN_TYPE));
                    for (auto payload = pattern->payload_patterns.rbegin(); payload != pattern->payload_patterns.rend(); ++payload) {
                        pending.push_back(PatternFrame {*payload, INVALID_TYPE_HANDLE});
                    }
                    break;
                }
                const EnumCaseInfo* case_info = nullptr;
                if (pattern->scoped) {
                    if (!pattern->enum_name.empty()) {
                        case_info = this->find_enum_case_by_scoped_name(pattern->enum_name, pattern->case_name, pattern->range);
                    } else {
                        case_info = this->find_enum_case_by_type_and_case(frame.type, pattern->case_name);
                        if (case_info == nullptr) {
                            this->report(pattern->range, sema_unknown_matched_enum_case_message(pattern->case_name));
                        }
                    }
                } else {
                    case_info = this->find_enum_case_by_type_and_case(frame.type, pattern->case_name);
                    if (case_info == nullptr) {
                        this->report(pattern->range, sema_unknown_matched_enum_case_message(pattern->case_name));
                    }
                }
                if (case_info == nullptr) {
                    for (auto payload = pattern->payload_patterns.rbegin(); payload != pattern->payload_patterns.rend(); ++payload) {
                        pending.push_back(PatternFrame {*payload, INVALID_TYPE_HANDLE});
                    }
                    break;
                }
                if (!this->checked_.types.same(case_info->type, frame.type)) {
                    this->report(pattern->range, std::string(SEMA_MATCH_CASE_WRONG_ENUM));
                }
                this->record_pattern_c_name(frame.pattern, case_info->c_name);
                this->record_pattern_case_name(frame.pattern, case_info->c_name);
                if (pattern->payload_patterns.empty()) {
                    break;
                }
                if (enum_case_payload_field_count(*case_info) == 0) {
                    this->report(pattern->range, std::string(SEMA_MATCH_PAYLOAD_CASE));
                    break;
                }
                if (pattern->payload_patterns.size() != enum_case_payload_field_count(*case_info)) {
                    this->report(
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
    std::unordered_map<std::string, PatternBinding> expected_bindings;
    bool have_expected = false;
    bool bindings_consistent = true;
    for (const syntax::PatternId alternative : root_pattern->alternatives) {
        std::vector<PatternBinding> alternative_bindings;
        analyze_pattern_tree(alternative, matched, alternative_bindings);

        std::unordered_map<std::string, PatternBinding> actual_bindings;
        for (const PatternBinding& binding : alternative_bindings) {
            actual_bindings.emplace(binding.name, binding);
        }
        if (!have_expected) {
            expected_bindings = std::move(actual_bindings);
            unified_bindings = std::move(alternative_bindings);
            have_expected = true;
            continue;
        }
        if (actual_bindings.size() != expected_bindings.size()) {
            const syntax::PatternNode* alternative_pattern = pattern_node(this->module_, alternative);
            this->report(
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
                this->report(
                    alternative_pattern == nullptr ? root_pattern->range : alternative_pattern->range,
                    std::string(SEMA_OR_PATTERN_BINDING_NAMES)
                );
                bindings_consistent = false;
                continue;
            }
            if (is_valid(expected.type) &&
                is_valid(actual->second.type) &&
                !this->checked_.types.same(expected.type, actual->second.type)) {
                this->report(actual->second.range, std::string(SEMA_OR_PATTERN_BINDING_TYPES));
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
        const auto inserted = this->symbols_.insert(Symbol {
            SymbolKind::local,
            binding.name,
            {},
            syntax::INVALID_MODULE_ID,
            binding.type,
            binding.range,
            is_mutable,
            syntax::Visibility::private_,
        }, this->diagnostics_);
        static_cast<void>(inserted);
    }
}

TypeHandle SemanticAnalyzer::analyze_match_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
    const TypeHandle expected_type
) {
    if (this->in_const_initializer_) {
        this->report(expr.range, std::string(SEMA_MATCH_CONST_INITIALIZER));
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
        this->report(expr.range, std::string(SEMA_MATCH_VALUE_TYPE));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (expr.match_arms.empty()) {
        this->report(expr.range, std::string(SEMA_MATCH_ARM_REQUIRED));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }

    std::vector<std::string> covered;
    TypeHandle result = INVALID_TYPE_HANDLE;
    std::vector<base::SourceRange> pending_null_arm_ranges;
    const auto is_null_result_expr = [&](const syntax::ExprId candidate) {
        if (!syntax::is_valid(candidate) || candidate.value >= this->module_.exprs.size()) {
            return false;
        }
        const syntax::ExprNode& candidate_expr = this->module_.exprs[candidate.value];
        return candidate_expr.kind == syntax::ExprKind::null_literal ||
            (candidate_expr.kind == syntax::ExprKind::block_expr && this->is_null_literal(candidate_expr.block_result));
    };
    const auto resolve_pending_null_arms = [&]() {
        if (!is_valid(result) || pending_null_arm_ranges.empty()) {
            return;
        }
        if (!this->checked_.types.is_pointer(result)) {
            for (const base::SourceRange range : pending_null_arm_ranges) {
                this->report(range, std::string(SEMA_MATCH_ARM_TYPE));
            }
        }
        pending_null_arm_ranges.clear();
    };
    bool saw_wildcard = false;
    bool covered_true = false;
    bool covered_false = false;
    bool saw_irrefutable = false;
    const auto enum_case_payloads_cover_case = [&](const syntax::PatternNode& enum_pattern) {
        if (enum_pattern.payload_patterns.empty()) {
            return true;
        }
        const EnumCaseInfo* case_info = this->find_enum_case_by_type_and_case(matched, enum_pattern.case_name);
        if (case_info == nullptr || case_info->payload_types.size() != enum_pattern.payload_patterns.size()) {
            return false;
        }
        bool payloads_cover_case = true;
        for (base::usize i = 0; i < enum_pattern.payload_patterns.size(); ++i) {
            payloads_cover_case = payloads_cover_case &&
                this->pattern_is_irrefutable(enum_pattern.payload_patterns[i], case_info->payload_types[i]);
        }
        return payloads_cover_case;
    };

    const auto leaf_domain = [&](const TypeHandle type) {
        std::vector<std::string> domain;
        if (!is_valid(type)) {
            return domain;
        }
        if (this->checked_.types.is_bool(type)) {
            domain.emplace_back(SEMA_MATCH_BOOL_TRUE_NAME);
            domain.emplace_back(SEMA_MATCH_BOOL_FALSE_NAME);
            return domain;
        }
        if (this->checked_.types.get(type).kind == TypeKind::enum_) {
            const std::vector<const EnumCaseInfo*>* cases = this->find_enum_cases_by_type(type);
            if (cases == nullptr || cases->empty()) {
                return std::vector<std::string> {};
            }
            domain.reserve(cases->size());
            for (const EnumCaseInfo* case_info : *cases) {
                if (case_info == nullptr || !case_info->payload_types.empty()) {
                    return std::vector<std::string> {};
                }
                domain.push_back(case_info->c_name);
            }
        }
        return domain;
    };

    const auto combine_option_sets = [](const std::vector<std::vector<std::string>>& option_sets) {
        std::vector<std::string> combinations;
        combinations.emplace_back();
        for (const std::vector<std::string>& options : option_sets) {
            if (options.empty()) {
                return std::vector<std::string> {};
            }
            std::vector<std::string> next;
            next.reserve(combinations.size() * options.size());
            for (const std::string& prefix : combinations) {
                for (const std::string& option : options) {
                    std::string value = prefix;
                    if (!value.empty()) {
                        value.push_back('|');
                    }
                    value += option;
                    next.push_back(std::move(value));
                }
            }
            combinations = std::move(next);
        }
        return combinations;
    };

    struct StructuralSlot {
        std::string name;
        TypeHandle type = INVALID_TYPE_HANDLE;
    };

    const auto structural_slots = [&](const TypeHandle type) {
        std::vector<StructuralSlot> slots;
        if (!is_valid(type)) {
            return slots;
        }
        if (!leaf_domain(type).empty()) {
            slots.push_back(StructuralSlot {{}, type});
            return slots;
        }
        const TypeInfo& info = this->checked_.types.get(type);
        if (info.kind == TypeKind::tuple) {
            slots.reserve(info.tuple_elements.size());
            for (base::usize i = 0; i < info.tuple_elements.size(); ++i) {
                if (leaf_domain(info.tuple_elements[i]).empty()) {
                    return std::vector<StructuralSlot> {};
                }
                slots.push_back(StructuralSlot {std::to_string(i), info.tuple_elements[i]});
            }
            return slots;
        }
        if (info.kind == TypeKind::struct_) {
            const StructInfo* struct_info = this->find_struct(type);
            if (struct_info == nullptr) {
                return slots;
            }
            slots.reserve(struct_info->fields.size());
            for (const StructFieldInfo& field : struct_info->fields) {
                if (leaf_domain(field.type).empty()) {
                    return std::vector<StructuralSlot> {};
                }
                slots.push_back(StructuralSlot {field.name, field.type});
            }
            return slots;
        }
        if (info.kind == TypeKind::array &&
            info.array_count <= SEMA_MATCH_STRUCTURAL_ARRAY_MAX_ELEMENTS &&
            !leaf_domain(info.array_element).empty()) {
            slots.reserve(static_cast<base::usize>(info.array_count));
            for (base::u64 i = 0; i < info.array_count; ++i) {
                slots.push_back(StructuralSlot {std::to_string(i), info.array_element});
            }
        }
        return slots;
    };

    const auto total_structural_cases = [&]() {
        const std::vector<StructuralSlot> slots = structural_slots(matched);
        std::vector<std::vector<std::string>> option_sets;
        option_sets.reserve(slots.size());
        for (const StructuralSlot& slot : slots) {
            option_sets.push_back(leaf_domain(slot.type));
        }
        return combine_option_sets(option_sets);
    };

    const auto append_unique_option = [](
        std::vector<std::string>& options,
        std::unordered_set<std::string>& seen,
        std::string option
    ) {
        if (seen.insert(option).second) {
            options.push_back(std::move(option));
        }
    };

    const auto append_unique_options = [&append_unique_option](
        std::vector<std::string>& options,
        std::unordered_set<std::string>& seen,
        std::vector<std::string> values
    ) {
        for (std::string& value : values) {
            append_unique_option(options, seen, std::move(value));
        }
    };

    const auto non_or_leaf_pattern_options = [&](const syntax::PatternNode& leaf_pattern, const TypeHandle leaf_type) {
        if (leaf_pattern.kind == syntax::PatternKind::wildcard ||
            leaf_pattern.kind == syntax::PatternKind::binding) {
            return leaf_domain(leaf_type);
        }
        if (this->checked_.types.is_bool(leaf_type) && leaf_pattern.kind == syntax::PatternKind::literal) {
            if (leaf_pattern.case_name == SEMA_MATCH_BOOL_TRUE_NAME ||
                leaf_pattern.case_name == SEMA_MATCH_BOOL_FALSE_NAME) {
                return std::vector<std::string> {std::string(leaf_pattern.case_name)};
            }
            return std::vector<std::string> {};
        }
        if (is_valid(leaf_type) &&
            this->checked_.types.get(leaf_type).kind == TypeKind::enum_ &&
            leaf_pattern.kind == syntax::PatternKind::enum_case &&
            enum_case_payloads_cover_case(leaf_pattern)) {
            const EnumCaseInfo* case_info = this->find_enum_case_by_type_and_case(leaf_type, leaf_pattern.case_name);
            if (case_info != nullptr && case_info->payload_types.empty()) {
                return std::vector<std::string> {case_info->c_name};
            }
        }
        return std::vector<std::string> {};
    };

    const auto non_or_structural_pattern_cases = [&](const syntax::PatternNode& root_pattern) {
        const std::vector<StructuralSlot> slots = structural_slots(matched);
        if (slots.empty()) {
            return std::vector<std::string> {};
        }
        if (root_pattern.kind == syntax::PatternKind::wildcard ||
            root_pattern.kind == syntax::PatternKind::binding) {
            return total_structural_cases();
        }
        if (slots.size() == 1 &&
            !array_match &&
            !this->checked_.types.is_tuple(matched) &&
            this->find_struct(matched) == nullptr) {
            return non_or_leaf_pattern_options(root_pattern, matched);
        }
        std::vector<std::vector<std::string>> option_sets;
        option_sets.reserve(slots.size());
        if (root_pattern.kind == syntax::PatternKind::tuple && this->checked_.types.is_tuple(matched)) {
            if (root_pattern.elements.size() != slots.size()) {
                return std::vector<std::string> {};
            }
            const TypeInfo& tuple = this->checked_.types.get(matched);
            for (base::usize i = 0; i < root_pattern.elements.size(); ++i) {
                const syntax::PatternNode* element_pattern = pattern_node(this->module_, root_pattern.elements[i]);
                if (element_pattern == nullptr) {
                    return std::vector<std::string> {};
                }
                option_sets.push_back(non_or_leaf_pattern_options(*element_pattern, tuple.tuple_elements[i]));
            }
            return combine_option_sets(option_sets);
        }
        if (root_pattern.kind == syntax::PatternKind::struct_ && this->find_struct(matched) != nullptr) {
            std::unordered_map<std::string_view, syntax::PatternId> field_patterns;
            for (const syntax::FieldPattern& field : root_pattern.field_patterns) {
                field_patterns.emplace(field.name, field.pattern);
            }
            for (const StructuralSlot& slot : slots) {
                const auto found = field_patterns.find(slot.name);
                if (found == field_patterns.end()) {
                    option_sets.push_back(leaf_domain(slot.type));
                    continue;
                }
                const syntax::PatternNode* field_pattern = pattern_node(this->module_, found->second);
                if (field_pattern == nullptr) {
                    return std::vector<std::string> {};
                }
                option_sets.push_back(non_or_leaf_pattern_options(*field_pattern, slot.type));
            }
            return combine_option_sets(option_sets);
        }
        if (root_pattern.kind == syntax::PatternKind::slice && array_match && !root_pattern.has_slice_rest) {
            if (root_pattern.elements.size() != slots.size()) {
                return std::vector<std::string> {};
            }
            const TypeInfo& array = this->checked_.types.get(matched);
            for (const syntax::PatternId element : root_pattern.elements) {
                const syntax::PatternNode* element_pattern = pattern_node(this->module_, element);
                if (element_pattern == nullptr) {
                    return std::vector<std::string> {};
                }
                option_sets.push_back(non_or_leaf_pattern_options(*element_pattern, array.array_element));
            }
            return combine_option_sets(option_sets);
        }
        return std::vector<std::string> {};
    };

    const auto structural_pattern_cases = [&](const syntax::PatternNode& root_pattern) {
        if (root_pattern.kind != syntax::PatternKind::or_pattern) {
            return non_or_structural_pattern_cases(root_pattern);
        }

        std::vector<std::string> cases;
        std::unordered_set<std::string> seen;
        std::vector<const syntax::PatternNode*> pending;
        pending.push_back(&root_pattern);
        while (!pending.empty()) {
            const syntax::PatternNode* pattern = pending.back();
            pending.pop_back();
            if (pattern == nullptr) {
                continue;
            }
            if (pattern->kind == syntax::PatternKind::or_pattern) {
                for (const syntax::PatternId alternative : pattern->alternatives) {
                    pending.push_back(pattern_node(this->module_, alternative));
                }
                continue;
            }
            append_unique_options(cases, seen, non_or_structural_pattern_cases(*pattern));
        }
        return cases;
    };

    const std::vector<std::string> structural_total = total_structural_cases();
    std::unordered_set<std::string> structural_covered;
    for (const syntax::MatchArm& arm : expr.match_arms) {
        const bool guarded = syntax::is_valid(arm.guard);
        const syntax::PatternNode* pattern = syntax::is_valid(arm.pattern) && arm.pattern.value < this->module_.patterns.size()
            ? &this->module_.patterns[arm.pattern.value]
            : nullptr;
        if (!guarded && (saw_wildcard || saw_irrefutable) && pattern != nullptr) {
            this->report(pattern->range, std::string(SEMA_MATCH_WILDCARD_UNREACHABLE));
        }
        std::vector<PatternBinding> bindings;
        const bool irrefutable = this->analyze_pattern(arm.pattern, matched, bindings);
        if (!guarded && irrefutable) {
            saw_irrefutable = true;
        }
        if (!guarded && pattern != nullptr && !structural_total.empty()) {
            const std::vector<std::string> arm_cases = structural_pattern_cases(*pattern);
            if (!arm_cases.empty()) {
                bool already_covered = true;
                for (const std::string& arm_case : arm_cases) {
                    if (!structural_covered.contains(arm_case)) {
                        already_covered = false;
                        break;
                    }
                }
                if (already_covered) {
                    this->report(pattern->range, std::string(SEMA_MATCH_WILDCARD_UNREACHABLE));
                }
                for (const std::string& arm_case : arm_cases) {
                    structural_covered.insert(arm_case);
                }
            }
        }
        if (!guarded && pattern != nullptr) {
            if (pattern->kind == syntax::PatternKind::wildcard ||
                pattern->kind == syntax::PatternKind::binding) {
                saw_wildcard = true;
            } else if (enum_match && pattern->kind == syntax::PatternKind::enum_case) {
                if (enum_case_payloads_cover_case(*pattern)) {
                    const std::vector<std::string>& pattern_names = this->active_pattern_c_names();
                    if (arm.pattern.value < pattern_names.size() && !pattern_names[arm.pattern.value].empty()) {
                        if (std::find(covered.begin(), covered.end(), pattern_names[arm.pattern.value]) != covered.end()) {
                            this->report(pattern->range, sema_duplicate_match_enum_case_message(pattern->case_name));
                        } else {
                            covered.push_back(pattern_names[arm.pattern.value]);
                        }
                    }
                }
            } else if (literal_match && pattern->kind == syntax::PatternKind::literal) {
                if (this->checked_.types.is_bool(matched)) {
                    if (pattern->case_name == SEMA_MATCH_BOOL_TRUE_NAME) {
                        covered_true = true;
                    } else if (pattern->case_name == SEMA_MATCH_BOOL_FALSE_NAME) {
                        covered_false = true;
                    }
                }
            } else if (pattern->kind == syntax::PatternKind::or_pattern) {
                std::vector<syntax::PatternId> alternatives = pattern->alternatives;
                const std::vector<std::string>& pattern_names = this->active_pattern_c_names();
                while (!alternatives.empty()) {
                    const syntax::PatternId alternative = alternatives.back();
                    alternatives.pop_back();
                    const syntax::PatternNode* alternative_pattern = pattern_node(this->module_, alternative);
                    if (alternative_pattern == nullptr) {
                        continue;
                    }
                    if (alternative_pattern->kind == syntax::PatternKind::or_pattern) {
                        for (const syntax::PatternId nested : alternative_pattern->alternatives) {
                            alternatives.push_back(nested);
                        }
                        continue;
                    }
                    if (alternative_pattern->kind == syntax::PatternKind::wildcard ||
                        alternative_pattern->kind == syntax::PatternKind::binding) {
                        saw_wildcard = true;
                        continue;
                    }
                    if (enum_match &&
                        alternative_pattern->kind == syntax::PatternKind::enum_case &&
                        enum_case_payloads_cover_case(*alternative_pattern) &&
                        alternative.value < pattern_names.size() &&
                        !pattern_names[alternative.value].empty()) {
                        if (std::find(covered.begin(), covered.end(), pattern_names[alternative.value]) != covered.end()) {
                            this->report(
                                alternative_pattern->range,
                                sema_duplicate_match_enum_case_message(alternative_pattern->case_name)
                            );
                        } else {
                            covered.push_back(pattern_names[alternative.value]);
                        }
                    }
                }
            }
        }
        const auto analyze_guard_and_arm_value = [&]() {
            if (guarded) {
                const TypeHandle guard_type = this->analyze_expr(arm.guard);
                if (!this->checked_.types.is_bool(guard_type)) {
                    this->report(this->module_.exprs[arm.guard.value].range, std::string(SEMA_MATCH_GUARD_BOOL));
                }
            }
            const TypeHandle arm_expected = is_valid(result) ? result : expected_type;
            return this->analyze_expr(arm.value, arm_expected);
        };
        TypeHandle arm_type = INVALID_TYPE_HANDLE;
        if (!bindings.empty()) {
            this->symbols_.push_scope();
            this->define_pattern_bindings(bindings, false);
            arm_type = analyze_guard_and_arm_value();
            this->symbols_.pop_scope();
        } else {
            arm_type = analyze_guard_and_arm_value();
        }
        const bool null_result_arm = is_null_result_expr(arm.value);
        if (!is_valid(arm_type) && null_result_arm && this->checked_.types.is_pointer(result)) {
            arm_type = this->analyze_expr(arm.value, result);
        }
        if (!is_valid(arm_type) && null_result_arm && !is_valid(result)) {
            pending_null_arm_ranges.push_back(this->module_.exprs[arm.value.value].range);
            continue;
        }
        if (!is_valid(result)) {
            result = arm_type;
            resolve_pending_null_arms();
        } else if (!this->checked_.types.same(result, arm_type)) {
            this->report(arm.range, std::string(SEMA_MATCH_ARM_TYPE));
        }
    }

    if (enum_match) {
        const auto check_case = [&](const EnumCaseInfo& case_info) {
            if (!saw_wildcard && std::find(covered.begin(), covered.end(), case_info.c_name) == covered.end()) {
                this->report(expr.range, sema_match_missing_enum_case_message(case_info.name));
            }
        };
        if (const std::vector<const EnumCaseInfo*>* cases = this->find_enum_cases_by_type(matched); cases != nullptr) {
            for (const EnumCaseInfo* case_info : *cases) {
                check_case(*case_info);
            }
        } else {
            for (const auto& entry : this->checked_.enum_cases) {
                const EnumCaseInfo& case_info = entry.second;
                if (this->checked_.types.same(case_info.type, matched)) {
                    check_case(case_info);
                }
            }
        }
    } else if (literal_match && !saw_wildcard && !saw_irrefutable && (!this->checked_.types.is_bool(matched) || !covered_true || !covered_false)) {
        this->report(expr.range, std::string(SEMA_MATCH_INTEGER_BOOL_WILDCARD));
    } else if ((tuple_match || struct_match || array_match || slice_match) &&
               !saw_irrefutable &&
               !saw_wildcard &&
               (structural_total.empty() || structural_covered.size() != structural_total.size())) {
        this->report(expr.range, std::string(SEMA_MATCH_NON_ENUM_IRREFUTABLE));
    }
    if (is_valid(result) && this->checked_.types.is_void(result)) {
        this->report(expr.range, std::string(SEMA_MATCH_RESULT_VOID));
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    return this->record_expr_type(expr_id, result);
}

const EnumCaseInfo* SemanticAnalyzer::analyze_single_value_pattern(
    const syntax::PatternId pattern_id,
    const TypeHandle matched,
    bool& covered_true,
    bool& covered_false,
    bool& saw_wildcard
) {
    if (!syntax::is_valid(pattern_id) || pattern_id.value >= this->module_.patterns.size()) {
        return nullptr;
    }
    const syntax::PatternNode& pattern = this->module_.patterns[pattern_id.value];
    if (saw_wildcard) {
        this->report(pattern.range, std::string(SEMA_MATCH_WILDCARD_UNREACHABLE));
    }
    if (pattern.kind == syntax::PatternKind::wildcard) {
        saw_wildcard = true;
        return nullptr;
    }
    if (pattern.kind != syntax::PatternKind::literal) {
        this->report(pattern.range, std::string(SEMA_INTEGER_BOOL_PATTERN));
        return nullptr;
    }
    if (this->checked_.types.is_bool(matched)) {
        if (pattern.case_name != SEMA_MATCH_BOOL_TRUE_NAME && pattern.case_name != SEMA_MATCH_BOOL_FALSE_NAME) {
            this->report(pattern.range, std::string(SEMA_BOOL_PATTERN));
        } else if (pattern.case_name == SEMA_MATCH_BOOL_TRUE_NAME) {
            covered_true = true;
        } else {
            covered_false = true;
        }
        return nullptr;
    }
    if (this->checked_.types.is_integer(matched)) {
        if (pattern.case_name == SEMA_MATCH_BOOL_TRUE_NAME || pattern.case_name == SEMA_MATCH_BOOL_FALSE_NAME) {
            this->report(pattern.range, std::string(SEMA_INTEGER_PATTERN));
        } else if (!this->integer_literal_fits_type(matched, pattern.case_name)) {
            this->report(pattern.range, std::string(SEMA_INTEGER_PATTERN_RANGE));
        }
        return nullptr;
    }
    this->report(pattern.range, std::string(SEMA_UNSUPPORTED_LITERAL_PATTERN));
    return nullptr;
}

} // namespace aurex::sema
