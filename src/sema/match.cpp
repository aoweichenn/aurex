#include <aurex/sema/sema.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::sema {

namespace {

constexpr std::string_view SEMA_MATCH_BOOL_TRUE_NAME = "true";
constexpr std::string_view SEMA_MATCH_BOOL_FALSE_NAME = "false";

enum class MatchPatternActionKind {
    analyze,
    merge_case_names,
};

struct MatchPatternAction {
    MatchPatternActionKind kind = MatchPatternActionKind::analyze;
    syntax::PatternId pattern = syntax::INVALID_PATTERN_ID;
    syntax::PatternId parent = syntax::INVALID_PATTERN_ID;
};

[[nodiscard]] bool pattern_has_payload_bindings(const syntax::PatternNode& pattern) noexcept {
    return !pattern.binding_names.empty();
}

[[nodiscard]] base::usize pattern_payload_binding_count(const syntax::PatternNode& pattern) noexcept {
    return pattern.binding_names.size();
}

[[nodiscard]] std::string_view pattern_payload_binding_name(
    const syntax::PatternNode& pattern,
    const base::usize index
) noexcept {
    return pattern.binding_names[index];
}

[[nodiscard]] base::usize enum_case_payload_field_count(const EnumCaseInfo& info) noexcept {
    return info.payload_types.size();
}

[[nodiscard]] TypeHandle enum_case_payload_field_type(const EnumCaseInfo& info, const base::usize index) noexcept {
    return info.payload_types[index];
}

} // namespace

TypeHandle SemanticAnalyzer::analyze_match_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
    const TypeHandle expected_type
) {
    if (this->in_const_initializer_) {
        this->report(expr.range, "match expression cannot be used in const initializer");
    }
    const TypeHandle matched = this->analyze_expr(expr.match_value);
    const bool enum_match = is_valid(matched) && this->checked_.types.get(matched).kind == TypeKind::enum_;
    const bool literal_match = is_valid(matched) &&
        (this->checked_.types.is_integer(matched) || this->checked_.types.is_bool(matched));
    if (!enum_match && !literal_match) {
        this->report(expr.range, "match expression requires an enum, integer, or bool value");
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    if (expr.match_arms.empty()) {
        this->report(expr.range, "match expression requires at least one arm");
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
                this->report(range, "match expression arms must have the same type");
            }
        }
        pending_null_arm_ranges.clear();
    };
    bool saw_wildcard = false;
    bool covered_true = false;
    bool covered_false = false;
    for (const syntax::MatchArm& arm : expr.match_arms) {
        const bool guarded = syntax::is_valid(arm.guard);
        const syntax::PatternNode* pattern = syntax::is_valid(arm.pattern) && arm.pattern.value < this->module_.patterns.size()
            ? &this->module_.patterns[arm.pattern.value]
            : nullptr;
        std::vector<std::string> arm_covered;
        std::vector<std::string>& coverage_target = guarded ? arm_covered : covered;
        bool arm_covered_true = false;
        bool arm_covered_false = false;
        bool arm_saw_wildcard = false;
        const EnumCaseInfo* case_info = enum_match
            ? this->analyze_enum_case_pattern(arm.pattern, matched, coverage_target, guarded ? arm_saw_wildcard : saw_wildcard)
            : this->analyze_value_pattern(
                arm.pattern,
                matched,
                guarded ? arm_covered_true : covered_true,
                guarded ? arm_covered_false : covered_false,
                guarded ? arm_saw_wildcard : saw_wildcard
            );
        const auto analyze_guard_and_arm_value = [&]() {
            if (guarded) {
                const TypeHandle guard_type = this->analyze_expr(arm.guard);
                if (!this->checked_.types.is_bool(guard_type)) {
                    this->report(this->module_.exprs[arm.guard.value].range, "match guard must be bool");
                }
            }
            const TypeHandle arm_expected = is_valid(result) ? result : expected_type;
            return this->analyze_expr(arm.value, arm_expected);
        };
        TypeHandle arm_type = INVALID_TYPE_HANDLE;
        if (pattern != nullptr && pattern_has_payload_bindings(*pattern)) {
            if (case_info == nullptr) {
                arm_type = analyze_guard_and_arm_value();
            } else if (enum_case_payload_field_count(*case_info) == 0) {
                this->report(arm.range, "match arm payload binding requires a payload enum case");
                arm_type = analyze_guard_and_arm_value();
            } else if (const base::usize binding_count = pattern_payload_binding_count(*pattern);
                       binding_count != enum_case_payload_field_count(*case_info)) {
                this->report(
                    pattern->range,
                    "enum payload pattern requires " +
                        std::to_string(enum_case_payload_field_count(*case_info)) +
                        " binding" +
                        (enum_case_payload_field_count(*case_info) == 1 ? "" : "s")
                );
                arm_type = analyze_guard_and_arm_value();
            } else {
                this->symbols_.push_scope();
                for (base::usize i = 0; i < binding_count; ++i) {
                    const auto inserted = this->symbols_.insert(Symbol {
                        SymbolKind::local,
                        std::string(pattern_payload_binding_name(*pattern, i)),
                        {},
                        syntax::INVALID_MODULE_ID,
                        enum_case_payload_field_type(*case_info, i),
                        arm.range,
                        false,
                        syntax::Visibility::private_,
                    }, this->diagnostics_);
                    static_cast<void>(inserted);
                }
                arm_type = analyze_guard_and_arm_value();
                this->symbols_.pop_scope();
            }
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
            this->report(arm.range, "match expression arms must have the same type");
        }
    }

    if (enum_match) {
        const auto check_case = [&](const EnumCaseInfo& case_info) {
            if (!saw_wildcard && std::find(covered.begin(), covered.end(), case_info.c_name) == covered.end()) {
                this->report(expr.range, "match expression is not exhaustive for enum case: " + case_info.name);
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
    } else if (!saw_wildcard && (!this->checked_.types.is_bool(matched) || !covered_true || !covered_false)) {
        this->report(expr.range, "match expression over integer or bool requires a wildcard arm");
    }
    if (is_valid(result) && this->checked_.types.is_void(result)) {
        this->report(expr.range, "match expression result cannot be void");
        return this->record_expr_type(expr_id, INVALID_TYPE_HANDLE);
    }
    return this->record_expr_type(expr_id, result);
}

const EnumCaseInfo* SemanticAnalyzer::analyze_enum_case_pattern(
    const syntax::PatternId pattern_id,
    const TypeHandle matched,
    std::vector<std::string>& covered,
    bool& saw_wildcard
) {
    if (!syntax::is_valid(pattern_id) || pattern_id.value >= this->module_.patterns.size()) {
        return nullptr;
    }
    const EnumCaseInfo* first_case = nullptr;
    std::vector<MatchPatternAction> actions;
    actions.push_back(MatchPatternAction {
        MatchPatternActionKind::analyze,
        pattern_id,
        syntax::INVALID_PATTERN_ID,
    });
    while (!actions.empty()) {
        const MatchPatternAction action = actions.back();
        actions.pop_back();

        if (action.kind == MatchPatternActionKind::merge_case_names) {
            this->merge_pattern_case_names(action.parent, action.pattern);
            continue;
        }

        if (!syntax::is_valid(action.pattern) || action.pattern.value >= this->module_.patterns.size()) {
            continue;
        }
        const syntax::PatternNode& pattern = this->module_.patterns[action.pattern.value];
        if (pattern.kind == syntax::PatternKind::or_pattern) {
            for (const syntax::PatternId alternative : pattern.alternatives) {
                const syntax::PatternNode* alternative_pattern =
                    syntax::is_valid(alternative) && alternative.value < this->module_.patterns.size()
                        ? &this->module_.patterns[alternative.value]
                        : nullptr;
                if (alternative_pattern != nullptr && pattern_has_payload_bindings(*alternative_pattern)) {
                    this->report(alternative_pattern->range, "or-pattern alternatives cannot bind payloads");
                }
            }

            for (auto alternative = pattern.alternatives.rbegin(); alternative != pattern.alternatives.rend(); ++alternative) {
                actions.push_back(MatchPatternAction {
                    MatchPatternActionKind::merge_case_names,
                    *alternative,
                    action.pattern,
                });
                actions.push_back(MatchPatternAction {
                    MatchPatternActionKind::analyze,
                    *alternative,
                    syntax::INVALID_PATTERN_ID,
                });
            }
            continue;
        }

        const EnumCaseInfo* case_info =
            this->analyze_single_enum_case_pattern(action.pattern, matched, covered, saw_wildcard);
        if (first_case == nullptr) {
            first_case = case_info;
        }
    }
    return first_case;
}

const EnumCaseInfo* SemanticAnalyzer::analyze_single_enum_case_pattern(
    const syntax::PatternId pattern_id,
    const TypeHandle matched,
    std::vector<std::string>& covered,
    bool& saw_wildcard
) {
    if (!syntax::is_valid(pattern_id) || pattern_id.value >= this->module_.patterns.size()) {
        return nullptr;
    }
    const syntax::PatternNode& pattern = this->module_.patterns[pattern_id.value];
    if (saw_wildcard) {
        this->report(pattern.range, "match arm is unreachable after wildcard pattern");
    }
    if (pattern.kind == syntax::PatternKind::wildcard) {
        saw_wildcard = true;
        return nullptr;
    }
    if (pattern.kind == syntax::PatternKind::literal) {
        this->report(pattern.range, "enum match pattern must be an enum case or wildcard");
        return nullptr;
    }

    const EnumCaseInfo* case_info = nullptr;
    if (pattern.scoped) {
        if (!pattern.enum_name.empty()) {
            case_info = this->find_enum_case_by_scoped_name(pattern.enum_name, pattern.case_name, pattern.range);
        } else {
            case_info = this->find_enum_case_by_type_and_case(matched, pattern.case_name);
            if (case_info == nullptr) {
                this->report(pattern.range, "unknown enum case for matched enum: " + std::string(pattern.case_name));
            }
        }
    } else {
        case_info = this->find_enum_case_in_visible_modules(pattern.case_name, pattern.range);
    }
    if (case_info == nullptr) {
        return nullptr;
    }
    if (!this->checked_.types.same(case_info->type, matched)) {
        this->report(pattern.range, "match arm case does not belong to matched enum");
    }
    if (std::find(covered.begin(), covered.end(), case_info->c_name) != covered.end()) {
        this->report(pattern.range, "duplicate match arm for enum case: " + case_info->name);
    } else {
        covered.push_back(case_info->c_name);
    }
    this->record_pattern_c_name(pattern_id, case_info->c_name);
    this->record_pattern_case_name(pattern_id, case_info->c_name);
    return case_info;
}

const EnumCaseInfo* SemanticAnalyzer::analyze_value_pattern(
    const syntax::PatternId pattern_id,
    const TypeHandle matched,
    bool& covered_true,
    bool& covered_false,
    bool& saw_wildcard
) {
    if (!syntax::is_valid(pattern_id) || pattern_id.value >= this->module_.patterns.size()) {
        return nullptr;
    }
    std::vector<syntax::PatternId> pending_patterns;
    pending_patterns.push_back(pattern_id);
    while (!pending_patterns.empty()) {
        const syntax::PatternId current_pattern_id = pending_patterns.back();
        pending_patterns.pop_back();

        if (!syntax::is_valid(current_pattern_id) || current_pattern_id.value >= this->module_.patterns.size()) {
            continue;
        }
        const syntax::PatternNode& pattern = this->module_.patterns[current_pattern_id.value];
        if (pattern.kind == syntax::PatternKind::or_pattern) {
            for (auto alternative = pattern.alternatives.rbegin(); alternative != pattern.alternatives.rend(); ++alternative) {
                pending_patterns.push_back(*alternative);
            }
            continue;
        }
        static_cast<void>(
            this->analyze_single_value_pattern(
                current_pattern_id,
                matched,
                covered_true,
                covered_false,
                saw_wildcard
            )
        );
    }
    return nullptr;
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
        this->report(pattern.range, "match arm is unreachable after wildcard pattern");
    }
    if (pattern.kind == syntax::PatternKind::wildcard) {
        saw_wildcard = true;
        return nullptr;
    }
    if (pattern.kind != syntax::PatternKind::literal) {
        this->report(pattern.range, "match pattern for integer or bool value must be a literal or wildcard");
        return nullptr;
    }
    if (this->checked_.types.is_bool(matched)) {
        if (pattern.case_name != SEMA_MATCH_BOOL_TRUE_NAME && pattern.case_name != SEMA_MATCH_BOOL_FALSE_NAME) {
            this->report(pattern.range, "bool match pattern must be true or false");
        } else if (pattern.case_name == SEMA_MATCH_BOOL_TRUE_NAME) {
            covered_true = true;
        } else {
            covered_false = true;
        }
        return nullptr;
    }
    if (this->checked_.types.is_integer(matched)) {
        if (pattern.case_name == SEMA_MATCH_BOOL_TRUE_NAME || pattern.case_name == SEMA_MATCH_BOOL_FALSE_NAME) {
            this->report(pattern.range, "integer match pattern must be an integer literal");
        } else if (!this->integer_literal_fits_type(matched, pattern.case_name)) {
            this->report(pattern.range, "integer match pattern literal is out of range for matched type");
        }
        return nullptr;
    }
    this->report(pattern.range, "unsupported literal match pattern");
    return nullptr;
}

} // namespace aurex::sema
