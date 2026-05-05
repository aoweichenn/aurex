#include "aurex/sema/sema.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

namespace aurex::sema {

namespace {

[[nodiscard]] TypeHandle payload_storage_type(TypeTable& types, const base::u64 size, const base::u64 alignment) {
    TypeHandle unit = types.builtin(BuiltinType::u8);
    base::u64 unit_size = 1;
    if (alignment >= alignof(std::uint64_t)) {
        unit = types.builtin(BuiltinType::u64);
        unit_size = sizeof(std::uint64_t);
    } else if (alignment >= alignof(std::uint32_t)) {
        unit = types.builtin(BuiltinType::u32);
        unit_size = sizeof(std::uint32_t);
    } else if (alignment >= alignof(std::uint16_t)) {
        unit = types.builtin(BuiltinType::u16);
        unit_size = sizeof(std::uint16_t);
    }
    const base::u64 count = std::max<base::u64>(1, (size + unit_size - 1) / unit_size);
    return count == 1 ? unit : types.array(count, unit);
}

[[nodiscard]] const syntax::ItemNode* item_from_id(const syntax::AstModule& module, const syntax::ItemId id) noexcept {
    if (!syntax::is_valid(id) || id.value >= module.items.size()) {
        return nullptr;
    }
    return &module.items[id.value];
}

} // namespace

const GenericEnumTemplateInfo* SemanticAnalyzer::find_generic_enum_template_in_visible_modules(
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (const auto found = generic_enum_templates_.find(module_key(current_module_, name)); found != generic_enum_templates_.end()) {
        return &found->second;
    }

    const GenericEnumTemplateInfo* imported_result = nullptr;
    syntax::ModuleId result_module = syntax::invalid_module_id;
    if (syntax::is_valid(current_module_) && current_module_.value < module_.modules.size()) {
        for (syntax::ModuleId module : module_.modules[current_module_.value].imports) {
            const auto found = generic_enum_templates_.find(module_key(module, name));
            if (found == generic_enum_templates_.end()) {
                continue;
            }
            if (imported_result != nullptr) {
                report(range, "ambiguous generic enum '" + std::string(name) + "' from modules " + module_name(result_module) + " and " + module_name(module));
                return nullptr;
            }
            imported_result = &found->second;
            result_module = module;
        }
    }
    if (imported_result == nullptr && report_unknown) {
        report(range, "unknown generic enum: " + std::string(name));
    }
    return imported_result;
}

std::string SemanticAnalyzer::generic_instance_key(
    const GenericEnumTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    std::string key = module_key(info.module, info.name) + "<";
    for (base::usize i = 0; i < args.size(); ++i) {
        if (i != 0) {
            key += ",";
        }
        key += checked_.types.display_name(args[i]);
    }
    key += ">";
    return key;
}

std::string SemanticAnalyzer::generic_display_name(
    const GenericEnumTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    std::string name = qualified_name(info.module, info.name) + "<";
    for (base::usize i = 0; i < args.size(); ++i) {
        if (i != 0) {
            name += ", ";
        }
        name += checked_.types.display_name(args[i]);
    }
    name += ">";
    return name;
}

std::string SemanticAnalyzer::generic_c_name(
    const GenericEnumTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    std::string suffix = info.name;
    for (TypeHandle arg : args) {
        suffix += "__";
        for (const char ch : checked_.types.c_name(arg)) {
            const bool alnum = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
            suffix += alnum ? ch : '_';
        }
    }
    return c_symbol_name(info.module, suffix);
}

std::string SemanticAnalyzer::generic_case_name(
    const GenericEnumTemplateInfo& info,
    const std::vector<TypeHandle>& args,
    const std::string_view case_name
) const {
    std::string name = info.name + "<";
    for (base::usize i = 0; i < args.size(); ++i) {
        if (i != 0) {
            name += ", ";
        }
        name += checked_.types.display_name(args[i]);
    }
    name += ">_";
    name += std::string(case_name);
    return name;
}

std::string SemanticAnalyzer::generic_case_c_name(
    const GenericEnumTemplateInfo& info,
    const std::vector<TypeHandle>& args,
    const std::string_view case_name
) const {
    return generic_c_name(info, args) + "_" + std::string(case_name);
}

TypeHandle SemanticAnalyzer::instantiate_generic_enum_from_syntax(
    const GenericEnumTemplateInfo& info,
    const std::vector<syntax::TypeId>& args,
    const base::SourceRange range,
    const bool opaque_allowed_as_pointee
) {
    if (args.size() != info.params.size()) {
        report(range, "generic enum type argument count mismatch for " + info.name);
        return invalid_type_handle;
    }
    std::vector<TypeHandle> resolved_args;
    resolved_args.reserve(args.size());
    for (syntax::TypeId arg : args) {
        resolved_args.push_back(resolve_type(arg, opaque_allowed_as_pointee));
    }
    return instantiate_generic_enum(info, resolved_args, range);
}

TypeHandle SemanticAnalyzer::instantiate_generic_enum(
    const GenericEnumTemplateInfo& info,
    const std::vector<TypeHandle>& args,
    const base::SourceRange range
) {
    if (args.size() != info.params.size()) {
        report(range, "generic enum type argument count mismatch for " + info.name);
        return invalid_type_handle;
    }
    for (TypeHandle arg : args) {
        if (!is_valid(arg)) {
            return invalid_type_handle;
        }
    }
    const std::string instance_key = generic_instance_key(info, args);
    if (const auto found = generic_enum_instances_.find(instance_key); found != generic_enum_instances_.end()) {
        return found->second;
    }

    const syntax::ItemNode* item = item_from_id(module_, info.item);
    if (item == nullptr) {
        report(range, "invalid generic enum template: " + info.name);
        return invalid_type_handle;
    }

    TypeHandle enum_type = checked_.types.named_enum(generic_display_name(info, args), generic_c_name(info, args));
    generic_enum_instances_[instance_key] = enum_type;
    generic_enum_instance_infos_[enum_type.value] = GenericEnumInstanceInfo {
        info.name,
        info.module,
        args,
    };

    GenericTypeSubstitution substitution;
    for (base::usize i = 0; i < info.params.size(); ++i) {
        substitution.types.emplace(info.params[i], args[i]);
    }

    const syntax::ModuleId previous_module = current_module_;
    current_module_ = info.module;
    const TypeHandle underlying = resolve_type_with_substitution(item->enum_base_type, &substitution, false);
    if (!checked_.types.is_integer(underlying)) {
        report(item->range, "enum base type must be an integer type");
    }
    checked_.types.set_enum_underlying(enum_type, underlying);

    TypeHandle payload_storage = invalid_type_handle;
    base::u64 payload_size = 0;
    base::u64 payload_align = 1;
    for (const syntax::EnumCaseDecl& enum_case : item->enum_cases) {
        const bool has_payload = syntax::is_valid(enum_case.payload_type);
        const TypeHandle payload_type = has_payload
            ? resolve_type_with_substitution(enum_case.payload_type, &substitution, false)
            : invalid_type_handle;
        if (has_payload) {
            if (!is_valid_storage_type(payload_type)) {
                report(enum_case.range, "enum payload type is not valid storage");
            }
            const base::u64 case_size = abi_size(payload_type);
            const base::u64 case_align = abi_align(payload_type);
            if (!is_valid(payload_storage) ||
                case_size > payload_size ||
                (case_size == payload_size && case_align > payload_align)) {
                payload_storage = payload_type;
            }
            payload_size = std::max(payload_size, case_size);
            payload_align = std::max(payload_align, case_align);
            if (checked_.types.contains_array(payload_type)) {
                report(enum_case.range, "enum payload cannot contain array storage in M1");
            }
        }
        const std::string case_c_name = generic_case_c_name(info, args, enum_case.name);
        checked_.enum_cases.emplace(module_key(info.module, case_c_name), EnumCaseInfo {
            generic_case_name(info, args, enum_case.name),
            case_c_name,
            info.module,
            enum_type,
            payload_type,
            std::string(enum_case.value_text),
            enum_case.range,
            info.name,
            std::string(enum_case.name),
        });
    }
    if (is_valid(payload_storage)) {
        checked_.types.set_enum_payload_layout(
            enum_type,
            payload_storage_type(checked_.types, payload_size, payload_align),
            payload_size,
            payload_align
        );
    }
    current_module_ = previous_module;
    return enum_type;
}

bool SemanticAnalyzer::infer_generic_enum_args(
    const syntax::TypeId pattern_type_id,
    const TypeHandle actual,
    const GenericEnumTemplateInfo& info,
    std::vector<TypeHandle>& inferred,
    const base::SourceRange range
) {
    if (!syntax::is_valid(pattern_type_id) || pattern_type_id.value >= module_.types.size()) {
        return false;
    }
    const syntax::TypeNode& pattern = module_.types[pattern_type_id.value];
    if (pattern.kind == syntax::TypeKind::named && pattern.type_args.empty()) {
        const auto found = std::find(info.params.begin(), info.params.end(), std::string(pattern.name));
        if (found != info.params.end()) {
            const base::usize index = static_cast<base::usize>(std::distance(info.params.begin(), found));
            if (index >= inferred.size()) {
                return false;
            }
            if (!is_valid(inferred[index])) {
                inferred[index] = actual;
                return true;
            }
            if (!checked_.types.same(inferred[index], actual)) {
                report(range, "generic enum constructor type inference conflict for " + info.params[index]);
                return false;
            }
            return true;
        }
    }
    const TypeHandle expected = resolve_type(pattern_type_id);
    return can_assign(expected, actual, syntax::invalid_expr_id);
}

const GenericEnumInstanceInfo* SemanticAnalyzer::generic_enum_instance(const TypeHandle type) const noexcept {
    if (!is_valid(type)) {
        return nullptr;
    }
    const auto found = generic_enum_instance_infos_.find(type.value);
    return found == generic_enum_instance_infos_.end() ? nullptr : &found->second;
}

const EnumCaseInfo* SemanticAnalyzer::instantiate_generic_enum_constructor(
    const syntax::ExprId callee_id,
    const std::vector<TypeHandle>& arg_types,
    const TypeHandle expected_type,
    const bool report_unknown
) {
    if (!syntax::is_valid(callee_id) || callee_id.value >= module_.exprs.size()) {
        return nullptr;
    }
    const syntax::ExprNode& callee = module_.exprs[callee_id.value];
    if (callee.kind != syntax::ExprKind::field ||
        !syntax::is_valid(callee.object) ||
        callee.object.value >= module_.exprs.size() ||
        module_.exprs[callee.object.value].kind != syntax::ExprKind::name) {
        return nullptr;
    }
    const syntax::ExprNode& enum_name = module_.exprs[callee.object.value];
    const GenericEnumTemplateInfo* info = find_generic_enum_template_in_visible_modules(enum_name.text, callee.range, false);
    if (info == nullptr) {
        return nullptr;
    }
    const syntax::ItemNode* item = item_from_id(module_, info->item);
    if (item == nullptr) {
        return nullptr;
    }
    const syntax::EnumCaseDecl* case_decl = nullptr;
    for (const syntax::EnumCaseDecl& candidate : item->enum_cases) {
        if (candidate.name == callee.field_name) {
            case_decl = &candidate;
            break;
        }
    }
    if (case_decl == nullptr) {
        if (report_unknown) {
            report(callee.range, "unknown enum case: " + std::string(enum_name.text) + "." + std::string(callee.field_name));
        }
        return nullptr;
    }
    std::vector<TypeHandle> type_args;
    if (!enum_name.type_args.empty()) {
        if (enum_name.type_args.size() != info->params.size()) {
            report(enum_name.range, "generic enum type argument count mismatch for " + info->name);
            return nullptr;
        }
        type_args.reserve(enum_name.type_args.size());
        for (syntax::TypeId arg : enum_name.type_args) {
            type_args.push_back(resolve_type(arg));
        }
    } else {
        type_args.assign(info->params.size(), invalid_type_handle);
        if (const GenericEnumInstanceInfo* expected = generic_enum_instance(expected_type);
            expected != nullptr &&
            expected->name == info->name &&
            expected->module.value == info->module.value &&
            expected->args.size() == info->params.size()) {
            type_args = expected->args;
        }
        if (syntax::is_valid(case_decl->payload_type) && arg_types.size() == 1) {
            static_cast<void>(infer_generic_enum_args(case_decl->payload_type, arg_types.front(), *info, type_args, callee.range));
        }
        for (base::usize i = 0; i < type_args.size(); ++i) {
            if (!is_valid(type_args[i])) {
                if (report_unknown) {
                    report(callee.range, "generic enum constructor requires explicit type arguments for " + info->name);
                }
                return nullptr;
            }
        }
    }
    const TypeHandle enum_type = instantiate_generic_enum(*info, type_args, callee.range);
    if (!is_valid(enum_type)) {
        return nullptr;
    }
    return find_enum_case_by_type_and_case(enum_type, callee.field_name);
}

} // namespace aurex::sema
