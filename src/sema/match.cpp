#include "aurex/sema/sema.hpp"

#include <algorithm>
#include <string>

namespace aurex::sema {

TypeHandle SemanticAnalyzer::analyze_match_expr(
    const syntax::ExprId expr_id,
    const syntax::ExprNode& expr,
    const TypeHandle expected_type
) {
    if (in_const_initializer_) {
        report(expr.range, "match expression cannot be used in const initializer");
    }
    const TypeHandle matched = analyze_expr(expr.match_value);
    const bool enum_match = is_valid(matched) && checked_.types.get(matched).kind == TypeKind::enum_;
    const bool literal_match = is_valid(matched) && (checked_.types.is_integer(matched) || checked_.types.is_bool(matched));
    if (!enum_match && !literal_match) {
        report(expr.range, "match expression requires an enum, integer, or bool value");
        return record_expr_type(expr_id, invalid_type_handle);
    }
    if (expr.match_arms.empty()) {
        report(expr.range, "match expression requires at least one arm");
        return record_expr_type(expr_id, invalid_type_handle);
    }

    std::vector<std::string> covered;
    TypeHandle result = invalid_type_handle;
    bool saw_wildcard = false;
    bool covered_true = false;
    bool covered_false = false;
    for (const syntax::MatchArm& arm : expr.match_arms) {
        const bool guarded = syntax::is_valid(arm.guard);
        const syntax::PatternNode* pattern = syntax::is_valid(arm.pattern) && arm.pattern.value < module_.patterns.size()
            ? &module_.patterns[arm.pattern.value]
            : nullptr;
        std::vector<std::string> arm_covered;
        std::vector<std::string>& coverage_target = guarded ? arm_covered : covered;
        bool arm_covered_true = false;
        bool arm_covered_false = false;
        bool arm_saw_wildcard = false;
        const EnumCaseInfo* case_info = enum_match
            ? analyze_enum_case_pattern(arm.pattern, matched, coverage_target, saw_wildcard)
            : analyze_value_pattern(
                arm.pattern,
                matched,
                guarded ? arm_covered_true : covered_true,
                guarded ? arm_covered_false : covered_false,
                guarded ? arm_saw_wildcard : saw_wildcard
            );
        TypeHandle arm_type = invalid_type_handle;
        if (pattern != nullptr && !pattern->binding_name.empty()) {
            if (case_info == nullptr) {
                const TypeHandle arm_expected = is_valid(result) ? result : expected_type;
                arm_type = analyze_expr(arm.value, arm_expected);
            } else if (!is_valid(case_info->payload_type)) {
                report(arm.range, "match arm payload binding requires a payload enum case");
            } else {
                symbols_.push_scope();
                static_cast<void>(symbols_.insert(Symbol {
                    SymbolKind::local,
                    std::string(pattern->binding_name),
                    {},
                    syntax::invalid_module_id,
                    case_info->payload_type,
                    arm.range,
                    false,
                    syntax::Visibility::private_,
                }, diagnostics_));
                if (guarded) {
                    const TypeHandle guard_type = analyze_expr(arm.guard);
                    if (!checked_.types.is_bool(guard_type)) {
                        report(module_.exprs[arm.guard.value].range, "match guard must be bool");
                    }
                }
                const TypeHandle arm_expected = is_valid(result) ? result : expected_type;
                arm_type = analyze_expr(arm.value, arm_expected);
                symbols_.pop_scope();
            }
        } else {
            if (guarded) {
                const TypeHandle guard_type = analyze_expr(arm.guard);
                if (!checked_.types.is_bool(guard_type)) {
                    report(module_.exprs[arm.guard.value].range, "match guard must be bool");
                }
            }
            const TypeHandle arm_expected = is_valid(result) ? result : expected_type;
            arm_type = analyze_expr(arm.value, arm_expected);
        }
        if (!is_valid(result)) {
            result = arm_type;
        } else if (!checked_.types.same(result, arm_type)) {
            report(arm.range, "match expression arms must have the same type");
        }
    }

    if (enum_match) {
        for (const auto& entry : checked_.enum_cases) {
            const EnumCaseInfo& case_info = entry.second;
            if (!checked_.types.same(case_info.type, matched)) {
                continue;
            }
            if (!saw_wildcard && std::find(covered.begin(), covered.end(), case_info.c_name) == covered.end()) {
                report(expr.range, "match expression is not exhaustive for enum case: " + case_info.name);
            }
        }
    } else if (!saw_wildcard && (!checked_.types.is_bool(matched) || !covered_true || !covered_false)) {
        report(expr.range, "match expression over integer or bool requires a wildcard arm");
    }
    if (is_valid(result) && checked_.types.is_void(result)) {
        report(expr.range, "match expression result cannot be void");
        return record_expr_type(expr_id, invalid_type_handle);
    }
    return record_expr_type(expr_id, result);
}

const EnumCaseInfo* SemanticAnalyzer::analyze_enum_case_pattern(
    const syntax::PatternId pattern_id,
    const TypeHandle matched,
    std::vector<std::string>& covered,
    bool& saw_wildcard
) {
    if (!syntax::is_valid(pattern_id) || pattern_id.value >= module_.patterns.size()) {
        return nullptr;
    }
    const syntax::PatternNode& pattern = module_.patterns[pattern_id.value];
    if (pattern.kind == syntax::PatternKind::or_pattern) {
        const EnumCaseInfo* first_case = nullptr;
        for (syntax::PatternId alternative : pattern.alternatives) {
            const syntax::PatternNode* alternative_pattern = syntax::is_valid(alternative) && alternative.value < module_.patterns.size()
                ? &module_.patterns[alternative.value]
                : nullptr;
            if (alternative_pattern != nullptr && !alternative_pattern->binding_name.empty()) {
                report(alternative_pattern->range, "or-pattern alternatives cannot bind payloads");
            }
            const EnumCaseInfo* case_info = analyze_enum_case_pattern(alternative, matched, covered, saw_wildcard);
            if (first_case == nullptr) {
                first_case = case_info;
            }
            merge_pattern_case_names(pattern_id, alternative);
        }
        return first_case;
    }
    return analyze_single_enum_case_pattern(pattern_id, matched, covered, saw_wildcard);
}

const EnumCaseInfo* SemanticAnalyzer::analyze_single_enum_case_pattern(
    const syntax::PatternId pattern_id,
    const TypeHandle matched,
    std::vector<std::string>& covered,
    bool& saw_wildcard
) {
    if (!syntax::is_valid(pattern_id) || pattern_id.value >= module_.patterns.size()) {
        return nullptr;
    }
    const syntax::PatternNode& pattern = module_.patterns[pattern_id.value];
    if (saw_wildcard) {
        report(pattern.range, "match arm is unreachable after wildcard pattern");
    }
    if (pattern.kind == syntax::PatternKind::wildcard) {
        saw_wildcard = true;
        return nullptr;
    }
    if (pattern.kind == syntax::PatternKind::literal) {
        report(pattern.range, "enum match pattern must be an enum case or wildcard");
        return nullptr;
    }

    const EnumCaseInfo* case_info = nullptr;
    if (pattern.scoped) {
        if (!pattern.enum_name.empty()) {
            case_info = find_enum_case_by_scoped_name(pattern.enum_name, pattern.case_name, pattern.range);
        } else {
            case_info = find_enum_case_by_type_and_case(matched, pattern.case_name);
            if (case_info == nullptr) {
                report(pattern.range, "unknown enum case for matched enum: " + std::string(pattern.case_name));
            }
        }
    } else {
        case_info = find_enum_case_in_visible_modules(pattern.case_name, pattern.range);
    }
    if (case_info == nullptr) {
        return nullptr;
    }
    if (!checked_.types.same(case_info->type, matched)) {
        report(pattern.range, "match arm case does not belong to matched enum");
    }
    if (std::find(covered.begin(), covered.end(), case_info->c_name) != covered.end()) {
        report(pattern.range, "duplicate match arm for enum case: " + case_info->name);
    } else {
        covered.push_back(case_info->c_name);
    }
    record_pattern_c_name(pattern_id, case_info->c_name);
    record_pattern_case_name(pattern_id, case_info->c_name);
    return case_info;
}

const EnumCaseInfo* SemanticAnalyzer::analyze_value_pattern(
    const syntax::PatternId pattern_id,
    const TypeHandle matched,
    bool& covered_true,
    bool& covered_false,
    bool& saw_wildcard
) {
    if (!syntax::is_valid(pattern_id) || pattern_id.value >= module_.patterns.size()) {
        return nullptr;
    }
    const syntax::PatternNode& pattern = module_.patterns[pattern_id.value];
    if (pattern.kind == syntax::PatternKind::or_pattern) {
        for (syntax::PatternId alternative : pattern.alternatives) {
            static_cast<void>(analyze_value_pattern(alternative, matched, covered_true, covered_false, saw_wildcard));
        }
        return nullptr;
    }
    return analyze_single_value_pattern(pattern_id, matched, covered_true, covered_false, saw_wildcard);
}

const EnumCaseInfo* SemanticAnalyzer::analyze_single_value_pattern(
    const syntax::PatternId pattern_id,
    const TypeHandle matched,
    bool& covered_true,
    bool& covered_false,
    bool& saw_wildcard
) {
    if (!syntax::is_valid(pattern_id) || pattern_id.value >= module_.patterns.size()) {
        return nullptr;
    }
    const syntax::PatternNode& pattern = module_.patterns[pattern_id.value];
    if (saw_wildcard) {
        report(pattern.range, "match arm is unreachable after wildcard pattern");
    }
    if (pattern.kind == syntax::PatternKind::wildcard) {
        saw_wildcard = true;
        return nullptr;
    }
    if (pattern.kind != syntax::PatternKind::literal) {
        report(pattern.range, "match pattern for integer or bool value must be a literal or wildcard");
        return nullptr;
    }
    if (checked_.types.is_bool(matched)) {
        if (pattern.case_name != "true" && pattern.case_name != "false") {
            report(pattern.range, "bool match pattern must be true or false");
        } else if (pattern.case_name == "true") {
            covered_true = true;
        } else {
            covered_false = true;
        }
        return nullptr;
    }
    if (checked_.types.is_integer(matched)) {
        if (pattern.case_name == "true" || pattern.case_name == "false") {
            report(pattern.range, "integer match pattern must be an integer literal");
        } else if (!integer_literal_fits_type(matched, pattern.case_name)) {
            report(pattern.range, "integer match pattern literal is out of range for matched type");
        }
        return nullptr;
    }
    report(pattern.range, "unsupported literal match pattern");
    return nullptr;
}

} // namespace aurex::sema
