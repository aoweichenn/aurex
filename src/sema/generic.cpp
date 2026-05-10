#include <aurex/sema/sema.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace aurex::sema {

namespace {

constexpr base::usize SEMA_GENERIC_INFERENCE_INITIAL_STACK_CAPACITY = 16;
constexpr base::u64 SEMA_GENERIC_EMPTY_PAYLOAD_SIZE = 0;
constexpr base::u64 SEMA_GENERIC_INITIAL_DISCRIMINANT_VALUE = 0;
constexpr base::u64 SEMA_GENERIC_MIN_PAYLOAD_ALIGNMENT = 1;
constexpr base::u64 SEMA_GENERIC_PAYLOAD_MIN_UNIT_SIZE = 1;
constexpr base::u64 SEMA_GENERIC_PAYLOAD_MIN_COUNT = 1;
constexpr base::usize SEMA_GENERIC_ENUM_CONSTRUCTOR_PAYLOAD_ARITY = 1;
constexpr char SEMA_GENERIC_ASCII_LOWER_FIRST_CHAR = 'a';
constexpr char SEMA_GENERIC_ASCII_LOWER_LAST_CHAR = 'z';
constexpr char SEMA_GENERIC_ASCII_UPPER_FIRST_CHAR = 'A';
constexpr char SEMA_GENERIC_ASCII_UPPER_LAST_CHAR = 'Z';
constexpr char SEMA_GENERIC_ASCII_DIGIT_FIRST_CHAR = '0';
constexpr char SEMA_GENERIC_ASCII_DIGIT_LAST_CHAR = '9';
constexpr char SEMA_GENERIC_IDENTIFIER_REPLACEMENT_CHAR = '_';
constexpr std::string_view SEMA_GENERIC_TEMPLATE_OPEN = "<";
constexpr std::string_view SEMA_GENERIC_TEMPLATE_CLOSE = ">";
constexpr std::string_view SEMA_GENERIC_KEY_SEPARATOR = ",";
constexpr std::string_view SEMA_GENERIC_ARGUMENT_SEPARATOR = ", ";
constexpr std::string_view SEMA_GENERIC_C_NAME_ARGUMENT_SEPARATOR = "__";
constexpr std::string_view SEMA_GENERIC_C_NAME_CASE_SEPARATOR = "_";
constexpr std::string_view SEMA_GENERIC_ITEM_KEY_SEPARATOR = ":";
constexpr std::string_view SEMA_GENERIC_METHOD_DISPLAY_SEPARATOR = ".";

struct GenericTypeInferenceFrame {
    syntax::TypeId pattern_type = syntax::invalid_type_id;
    TypeHandle actual = invalid_type_handle;
};

[[nodiscard]] std::optional<base::usize> generic_parameter_index(
    const std::vector<std::string>& params,
    const std::string_view name
) {
    for (base::usize index = 0; index < params.size(); ++index) {
        if (params[index] == name) {
            return index;
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool generic_ascii_alphanumeric(const char ch) noexcept {
    return (ch >= SEMA_GENERIC_ASCII_LOWER_FIRST_CHAR && ch <= SEMA_GENERIC_ASCII_LOWER_LAST_CHAR) ||
           (ch >= SEMA_GENERIC_ASCII_UPPER_FIRST_CHAR && ch <= SEMA_GENERIC_ASCII_UPPER_LAST_CHAR) ||
           (ch >= SEMA_GENERIC_ASCII_DIGIT_FIRST_CHAR && ch <= SEMA_GENERIC_ASCII_DIGIT_LAST_CHAR);
}

void append_sanitized_c_name_fragment(std::string& target, const std::string_view fragment) {
    for (const char ch : fragment) {
        target.push_back(generic_ascii_alphanumeric(ch) ? ch : SEMA_GENERIC_IDENTIFIER_REPLACEMENT_CHAR);
    }
}

template <typename AppendArgument>
void append_joined_type_arguments(
    std::string& target,
    const std::vector<TypeHandle>& args,
    const std::string_view separator,
    AppendArgument append_argument
) {
    for (base::usize i = 0; i < args.size(); ++i) {
        if (i != 0) {
            target.append(separator);
        }
        append_argument(target, args[i]);
    }
}

[[nodiscard]] TypeHandle payload_storage_type(TypeTable& types, const base::u64 size, const base::u64 alignment) {
    TypeHandle unit = types.builtin(BuiltinType::u8);
    base::u64 unit_size = SEMA_GENERIC_PAYLOAD_MIN_UNIT_SIZE;
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
    const base::u64 count = std::max<base::u64>(
        SEMA_GENERIC_PAYLOAD_MIN_COUNT,
        (size + unit_size - SEMA_GENERIC_PAYLOAD_MIN_COUNT) / unit_size
    );
    return count == SEMA_GENERIC_PAYLOAD_MIN_COUNT ? unit : types.array(count, unit);
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
    for (syntax::ModuleId module : visible_modules(current_module_)) {
        if (module.value == current_module_.value) {
            continue;
        }
        const auto found = generic_enum_templates_.find(module_key(module, name));
        if (found == generic_enum_templates_.end()) {
            continue;
        }
        if (!can_access(module, found->second.visibility)) {
            continue;
        }
        if (imported_result != nullptr) {
            report(range, "ambiguous generic enum '" + std::string(name) + "' from modules " + module_name(result_module) + " and " + module_name(module));
            return nullptr;
        }
        imported_result = &found->second;
        result_module = module;
    }
    if (imported_result == nullptr && report_unknown) {
        report(range, "unknown generic enum: " + std::string(name));
    }
    return imported_result;
}

const GenericStructTemplateInfo* SemanticAnalyzer::find_generic_struct_template_in_visible_modules(
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (const auto found = generic_struct_templates_.find(module_key(current_module_, name)); found != generic_struct_templates_.end()) {
        return &found->second;
    }

    const GenericStructTemplateInfo* imported_result = nullptr;
    syntax::ModuleId result_module = syntax::invalid_module_id;
    for (syntax::ModuleId module : visible_modules(current_module_)) {
        if (module.value == current_module_.value) {
            continue;
        }
        const auto found = generic_struct_templates_.find(module_key(module, name));
        if (found == generic_struct_templates_.end()) {
            continue;
        }
        if (!can_access(module, found->second.visibility)) {
            continue;
        }
        if (imported_result != nullptr) {
            report(range, "ambiguous generic struct '" + std::string(name) + "' from modules " + module_name(result_module) + " and " + module_name(module));
            return nullptr;
        }
        imported_result = &found->second;
        result_module = module;
    }
    if (imported_result == nullptr && report_unknown) {
        report(range, "unknown generic struct: " + std::string(name));
    }
    return imported_result;
}

const GenericFunctionTemplateInfo* SemanticAnalyzer::find_generic_function_template_in_visible_modules(
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (const auto found = generic_function_templates_.find(module_key(current_module_, name)); found != generic_function_templates_.end()) {
        return &found->second;
    }

    const GenericFunctionTemplateInfo* imported_result = nullptr;
    syntax::ModuleId result_module = syntax::invalid_module_id;
    for (syntax::ModuleId module : visible_modules(current_module_)) {
        if (module.value == current_module_.value) {
            continue;
        }
        const auto found = generic_function_templates_.find(module_key(module, name));
        if (found == generic_function_templates_.end()) {
            continue;
        }
        if (!can_access(module, found->second.visibility)) {
            continue;
        }
        if (imported_result != nullptr) {
            report(range, "ambiguous generic function '" + std::string(name) + "' from modules " + module_name(result_module) + " and " + module_name(module));
            return nullptr;
        }
        imported_result = &found->second;
        result_module = module;
    }
    if (imported_result == nullptr && report_unknown) {
        report(range, "unknown generic function: " + std::string(name));
    }
    return imported_result;
}

const GenericEnumTemplateInfo* SemanticAnalyzer::find_generic_enum_template_in_module(
    const syntax::ModuleId module,
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    const auto found = generic_enum_templates_.find(module_key(module, name));
    if (found == generic_enum_templates_.end()) {
        if (report_unknown) {
            report(range, "unknown generic enum in module " + module_name(module) + ": " + std::string(name));
        }
        return nullptr;
    }
    if (!can_access(module, found->second.visibility)) {
        if (report_unknown) {
            report(range, "generic enum is private: " + module_name(module) + "." + std::string(name));
        }
        return nullptr;
    }
    return &found->second;
}

const GenericStructTemplateInfo* SemanticAnalyzer::find_generic_struct_template_in_module(
    const syntax::ModuleId module,
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    const auto found = generic_struct_templates_.find(module_key(module, name));
    if (found == generic_struct_templates_.end()) {
        if (report_unknown) {
            report(range, "unknown generic struct in module " + module_name(module) + ": " + std::string(name));
        }
        return nullptr;
    }
    if (!can_access(module, found->second.visibility)) {
        if (report_unknown) {
            report(range, "generic struct is private: " + module_name(module) + "." + std::string(name));
        }
        return nullptr;
    }
    return &found->second;
}

const GenericFunctionTemplateInfo* SemanticAnalyzer::find_generic_function_template_in_module(
    const syntax::ModuleId module,
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    const auto found = generic_function_templates_.find(module_key(module, name));
    if (found == generic_function_templates_.end()) {
        if (report_unknown) {
            report(range, "unknown generic function in module " + module_name(module) + ": " + std::string(name));
        }
        return nullptr;
    }
    if (!can_access(module, found->second.visibility)) {
        if (report_unknown) {
            report(range, "generic function is private: " + module_name(module) + "." + std::string(name));
        }
        return nullptr;
    }
    return &found->second;
}

const GenericFunctionInstanceInfo* SemanticAnalyzer::find_generic_method_in_visible_modules(
    const TypeHandle owner_type,
    const std::string_view name,
    const base::SourceRange range,
    const bool require_self,
    const std::vector<TypeHandle>* const arg_types,
    const TypeHandle expected_type,
    const std::vector<syntax::TypeId>* const explicit_type_args,
    const bool report_unknown
) {
    const GenericFunctionInstanceInfo* imported_result = nullptr;
    syntax::ModuleId result_module = syntax::invalid_module_id;
    bool inaccessible_result = false;
    bool uninferred_method_params = false;
    bool type_arg_count_mismatch = false;

    const auto candidates = generic_method_template_indices_.equal_range(std::string(name));
    for (syntax::ModuleId module : visible_modules(current_module_)) {
        for (auto it = candidates.first; it != candidates.second; ++it) {
            const GenericFunctionTemplateInfo& info = generic_method_templates_[it->second];
            if (info.module.value != module.value ||
                info.name != name ||
                !info.is_method) {
                continue;
            }
            const syntax::ItemNode* item = item_from_id(module_, info.item);
            if (item == nullptr) {
                continue;
            }
            const bool has_self_param = !item->params.empty() && item->params.front().name == "self";
            if (require_self && !has_self_param) {
                continue;
            }

            std::vector<TypeHandle> inferred(info.params.size(), invalid_type_handle);
            if (!infer_generic_args_from_type_pattern(
                    info.impl_type,
                    owner_type,
                    info.params,
                    inferred,
                    range,
                    "generic method",
                    info.module
                )) {
                continue;
            }

            const bool has_explicit_type_args = explicit_type_args != nullptr && !explicit_type_args->empty();
            const base::usize method_param_count = info.params.size() >= info.impl_generic_param_count
                ? info.params.size() - info.impl_generic_param_count
                : 0;
            if (has_explicit_type_args) {
                if (explicit_type_args->size() != method_param_count) {
                    type_arg_count_mismatch = true;
                    continue;
                }
                for (base::usize i = 0; i < explicit_type_args->size(); ++i) {
                    const TypeHandle arg = resolve_type((*explicit_type_args)[i]);
                    inferred[info.impl_generic_param_count + i] = arg;
                }
            } else if (arg_types != nullptr || is_valid(expected_type)) {
                const std::vector<TypeHandle> no_args;
                const std::vector<TypeHandle>& provided_args = arg_types != nullptr ? *arg_types : no_args;
                static_cast<void>(infer_generic_function_args(info, provided_args, expected_type, inferred, range));
            }

            bool complete = true;
            bool missing_method_param = false;
            for (base::usize i = 0; i < inferred.size(); ++i) {
                if (!is_valid(inferred[i])) {
                    if (i >= info.impl_generic_param_count) {
                        missing_method_param = true;
                    }
                    complete = false;
                }
            }
            if (!complete) {
                if (missing_method_param) {
                    uninferred_method_params = true;
                }
                continue;
            }
            if (!can_access(module, info.visibility)) {
                inaccessible_result = true;
                continue;
            }
            if (imported_result != nullptr) {
                report(range, "ambiguous method '" + checked_.types.display_name(owner_type) + "." + std::string(name) +
                    "' from modules " + module_name(result_module) + " and " + module_name(module));
                return nullptr;
            }
            imported_result = this->instantiate_generic_function(info, inferred, range);
            result_module = module;
        }
    }

    if (imported_result == nullptr && report_unknown) {
        if (type_arg_count_mismatch) {
            report(range, "generic method type argument count mismatch for " + std::string(name));
            return nullptr;
        }
        if (uninferred_method_params) {
            report(range, "generic method requires explicit type arguments: " + std::string(name));
            return nullptr;
        }
        if (inaccessible_result) {
            report(range, "method is private: " + checked_.types.display_name(owner_type) + "." + std::string(name));
            return nullptr;
        }
    }
    return imported_result;
}

std::string SemanticAnalyzer::generic_instance_key(
    const GenericEnumTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    std::string key = module_key(info.module, info.name);
    key.append(SEMA_GENERIC_TEMPLATE_OPEN);
    append_joined_type_arguments(key, args, SEMA_GENERIC_KEY_SEPARATOR, [](std::string& target, const TypeHandle arg) {
        target += std::to_string(arg.value);
    });
    key.append(SEMA_GENERIC_TEMPLATE_CLOSE);
    return key;
}

std::string SemanticAnalyzer::generic_display_name(
    const GenericEnumTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    std::string name = qualified_name(info.module, info.name);
    name.append(SEMA_GENERIC_TEMPLATE_OPEN);
    append_joined_type_arguments(name, args, SEMA_GENERIC_ARGUMENT_SEPARATOR, [&](std::string& target, const TypeHandle arg) {
        target += this->checked_.types.display_name(arg);
    });
    name.append(SEMA_GENERIC_TEMPLATE_CLOSE);
    return name;
}

std::string SemanticAnalyzer::generic_c_name(
    const GenericEnumTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    std::string suffix = info.name;
    for (TypeHandle arg : args) {
        suffix.append(SEMA_GENERIC_C_NAME_ARGUMENT_SEPARATOR);
        append_sanitized_c_name_fragment(suffix, this->checked_.types.c_name(arg));
    }
    return c_symbol_name(info.module, suffix);
}

std::string SemanticAnalyzer::generic_instance_key(
    const GenericStructTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    std::string key = module_key(info.module, info.name);
    key.append(SEMA_GENERIC_TEMPLATE_OPEN);
    append_joined_type_arguments(key, args, SEMA_GENERIC_KEY_SEPARATOR, [](std::string& target, const TypeHandle arg) {
        target += std::to_string(arg.value);
    });
    key.append(SEMA_GENERIC_TEMPLATE_CLOSE);
    return key;
}

std::string SemanticAnalyzer::generic_display_name(
    const GenericStructTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    std::string name = qualified_name(info.module, info.name);
    name.append(SEMA_GENERIC_TEMPLATE_OPEN);
    append_joined_type_arguments(name, args, SEMA_GENERIC_ARGUMENT_SEPARATOR, [&](std::string& target, const TypeHandle arg) {
        target += this->checked_.types.display_name(arg);
    });
    name.append(SEMA_GENERIC_TEMPLATE_CLOSE);
    return name;
}

std::string SemanticAnalyzer::generic_c_name(
    const GenericStructTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    std::string suffix = info.name;
    for (TypeHandle arg : args) {
        suffix.append(SEMA_GENERIC_C_NAME_ARGUMENT_SEPARATOR);
        append_sanitized_c_name_fragment(suffix, this->checked_.types.c_name(arg));
    }
    return c_symbol_name(info.module, suffix);
}

std::string SemanticAnalyzer::generic_instance_key(
    const GenericFunctionTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    std::string item_key = std::to_string(info.item.value);
    item_key.append(SEMA_GENERIC_ITEM_KEY_SEPARATOR);
    item_key += info.name;

    std::string key = module_key(info.module, item_key);
    key.append(SEMA_GENERIC_TEMPLATE_OPEN);
    append_joined_type_arguments(key, args, SEMA_GENERIC_KEY_SEPARATOR, [](std::string& target, const TypeHandle arg) {
        target += std::to_string(arg.value);
    });
    key.append(SEMA_GENERIC_TEMPLATE_CLOSE);
    return key;
}

std::string SemanticAnalyzer::generic_display_name(
    const GenericFunctionTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    std::string name = qualified_name(info.module, info.name);
    name.append(SEMA_GENERIC_TEMPLATE_OPEN);
    append_joined_type_arguments(name, args, SEMA_GENERIC_ARGUMENT_SEPARATOR, [&](std::string& target, const TypeHandle arg) {
        target += this->checked_.types.display_name(arg);
    });
    name.append(SEMA_GENERIC_TEMPLATE_CLOSE);
    return name;
}

std::string SemanticAnalyzer::generic_c_name(
    const GenericFunctionTemplateInfo& info,
    const std::vector<TypeHandle>& args
) const {
    std::string suffix = info.name;
    for (TypeHandle arg : args) {
        suffix.append(SEMA_GENERIC_C_NAME_ARGUMENT_SEPARATOR);
        append_sanitized_c_name_fragment(suffix, this->checked_.types.c_name(arg));
    }
    return c_symbol_name(info.module, suffix);
}

std::string SemanticAnalyzer::generic_case_name(
    const GenericEnumTemplateInfo& info,
    const std::vector<TypeHandle>& args,
    const std::string_view case_name
) const {
    std::string name = info.name;
    name.append(SEMA_GENERIC_TEMPLATE_OPEN);
    append_joined_type_arguments(name, args, SEMA_GENERIC_ARGUMENT_SEPARATOR, [&](std::string& target, const TypeHandle arg) {
        target += this->checked_.types.display_name(arg);
    });
    name.append(SEMA_GENERIC_TEMPLATE_CLOSE);
    name.append(SEMA_GENERIC_C_NAME_CASE_SEPARATOR);
    name.append(case_name);
    return name;
}

std::string SemanticAnalyzer::generic_case_c_name(
    const GenericEnumTemplateInfo& info,
    const std::vector<TypeHandle>& args,
    const std::string_view case_name
) const {
    std::string name = generic_c_name(info, args);
    name.append(SEMA_GENERIC_C_NAME_CASE_SEPARATOR);
    name.append(case_name);
    return name;
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
    base::u64 payload_size = SEMA_GENERIC_EMPTY_PAYLOAD_SIZE;
    base::u64 payload_align = SEMA_GENERIC_MIN_PAYLOAD_ALIGNMENT;
    bool contains_array_payload = false;
    bool copyable = true;
    std::unordered_set<std::string> seen_cases;
    std::unordered_set<base::u64> seen_values;
    for (const syntax::EnumCaseDecl& enum_case : item->enum_cases) {
        if (!seen_cases.insert(std::string(enum_case.name)).second) {
            report(enum_case.range, "duplicate enum case: " + info.name + "." + std::string(enum_case.name));
            continue;
        }
        base::u64 discriminant = SEMA_GENERIC_INITIAL_DISCRIMINANT_VALUE;
        const bool parsed_discriminant = parse_integer_literal_text(enum_case.value_text, discriminant);
        if (!parsed_discriminant) {
            report(enum_case.range, "enum discriminant literal is out of range");
        } else if (!integer_literal_fits_type(underlying, enum_case.value_text)) {
            report(enum_case.range, "enum discriminant does not fit enum base type");
        } else if (!seen_values.insert(discriminant).second) {
            report(enum_case.range, "duplicate enum discriminant value in " + info.name);
        }
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
                contains_array_payload = true;
                report(enum_case.range, "enum payload cannot contain array storage");
            }
            if (!checked_.types.is_copyable(payload_type)) {
                copyable = false;
            }
        }
        const std::string case_c_name = generic_case_c_name(info, args, enum_case.name);
        const auto inserted = checked_.enum_cases.emplace(module_key(info.module, case_c_name), EnumCaseInfo {
            generic_case_name(info, args, enum_case.name),
            case_c_name,
            info.module,
            enum_type,
            payload_type,
            std::string(enum_case.value_text),
            enum_case.range,
            info.name,
            std::string(enum_case.name),
            info.visibility,
        });
        if (!inserted.second) {
            report(enum_case.range, "duplicate enum case: " + info.name + "." + std::string(enum_case.name));
        } else {
            index_enum_case(inserted.first->second);
        }
    }
    if (is_valid(payload_storage)) {
        checked_.types.set_enum_payload_layout(
            enum_type,
            payload_storage_type(checked_.types, payload_size, payload_align),
            payload_size,
            payload_align
        );
    }
    checked_.types.set_record_properties(enum_type, contains_array_payload, copyable && !contains_array_payload);
    current_module_ = previous_module;
    return enum_type;
}

TypeHandle SemanticAnalyzer::instantiate_generic_struct_from_syntax(
    const GenericStructTemplateInfo& info,
    const std::vector<syntax::TypeId>& args,
    const base::SourceRange range,
    const bool opaque_allowed_as_pointee
) {
    if (args.size() != info.params.size()) {
        report(range, "generic struct type argument count mismatch for " + info.name);
        return invalid_type_handle;
    }
    std::vector<TypeHandle> resolved_args;
    resolved_args.reserve(args.size());
    for (syntax::TypeId arg : args) {
        resolved_args.push_back(resolve_type(arg, opaque_allowed_as_pointee));
    }
    return instantiate_generic_struct(info, resolved_args, range);
}

TypeHandle SemanticAnalyzer::instantiate_generic_struct(
    const GenericStructTemplateInfo& info,
    const std::vector<TypeHandle>& args,
    const base::SourceRange range
) {
    if (args.size() != info.params.size()) {
        report(range, "generic struct type argument count mismatch for " + info.name);
        return invalid_type_handle;
    }
    for (TypeHandle arg : args) {
        if (!is_valid(arg)) {
            return invalid_type_handle;
        }
    }
    const std::string instance_key = generic_instance_key(info, args);
    if (const auto found = generic_struct_instances_.find(instance_key); found != generic_struct_instances_.end()) {
        return found->second;
    }

    const syntax::ItemNode* item = item_from_id(module_, info.item);
    if (item == nullptr) {
        report(range, "invalid generic struct template: " + info.name);
        return invalid_type_handle;
    }

    TypeHandle struct_type = checked_.types.named_struct(generic_display_name(info, args), generic_c_name(info, args), false);
    generic_struct_instances_[instance_key] = struct_type;
    generic_struct_instance_infos_[struct_type.value] = GenericStructInstanceInfo {
        info.name,
        info.module,
        args,
    };

    StructInfo instance_info;
    instance_info.name = generic_display_name(info, args);
    instance_info.c_name = generic_c_name(info, args);
    instance_info.module = info.module;
    instance_info.type = struct_type;
    instance_info.is_opaque = false;

    GenericTypeSubstitution substitution;
    for (base::usize i = 0; i < info.params.size(); ++i) {
        substitution.types.emplace(info.params[i], args[i]);
    }

    const syntax::ModuleId previous_module = current_module_;
    current_module_ = info.module;
    bool contains_array = false;
    bool copyable = true;
    std::unordered_set<std::string> seen_fields;
    for (const syntax::FieldDecl& field : item->fields) {
        if (!seen_fields.insert(std::string(field.name)).second) {
            report(field.range, "duplicate struct field: " + std::string(field.name));
            continue;
        }
        const TypeHandle field_type = resolve_type_with_substitution(field.type, &substitution, false);
        if (!is_valid_storage_type(field_type)) {
            report(field.range, "field type is not valid storage");
        }
        instance_info.fields.push_back(StructFieldInfo {
            std::string(field.name),
            {},
            syntax::invalid_module_id,
            field_type,
            field.range,
            field.visibility,
        });
        if (checked_.types.contains_array(field_type)) {
            contains_array = true;
        }
        if (!checked_.types.is_copyable(field_type)) {
            copyable = false;
        }
    }
    checked_.types.set_record_properties(struct_type, contains_array, copyable && !contains_array);
    current_module_ = previous_module;

    const auto inserted = checked_.structs.emplace(module_key(info.module, instance_info.c_name), std::move(instance_info));
    if (inserted.second) {
        struct_infos_by_type_[struct_type.value] = &inserted.first->second;
    }
    return struct_type;
}

bool SemanticAnalyzer::infer_generic_enum_args(
    const syntax::TypeId pattern_type_id,
    const TypeHandle actual,
    const GenericEnumTemplateInfo& info,
    std::vector<TypeHandle>& inferred,
    const base::SourceRange range
) {
    return infer_generic_args_from_type_pattern(
        pattern_type_id,
        actual,
        info.params,
        inferred,
        range,
        "generic enum constructor",
        info.module
    );
}

bool SemanticAnalyzer::infer_generic_args_from_type_pattern(
    const syntax::TypeId pattern_type_id,
    const TypeHandle actual,
    const std::vector<std::string>& params,
    std::vector<TypeHandle>& inferred,
    const base::SourceRange range,
    const std::string_view context,
    const syntax::ModuleId pattern_module
) {
    if (!syntax::is_valid(pattern_type_id) || pattern_type_id.value >= this->module_.types.size() || !is_valid(actual)) {
        return false;
    }

    const syntax::ModuleId previous_module = this->current_module_;
    this->current_module_ = pattern_module;

    bool result = true;
    std::vector<GenericTypeInferenceFrame> stack;
    stack.reserve(SEMA_GENERIC_INFERENCE_INITIAL_STACK_CAPACITY);
    stack.push_back(GenericTypeInferenceFrame {pattern_type_id, actual});
    while (result && !stack.empty()) {
        const GenericTypeInferenceFrame frame = stack.back();
        stack.pop_back();
        if (!syntax::is_valid(frame.pattern_type) ||
            frame.pattern_type.value >= this->module_.types.size() ||
            !is_valid(frame.actual)) {
            result = false;
            break;
        }

        const syntax::TypeNode& pattern = this->module_.types[frame.pattern_type.value];
        if (pattern.kind == syntax::TypeKind::named && pattern.type_args.empty()) {
            const std::optional<base::usize> param_index = generic_parameter_index(params, pattern.name);
            if (param_index.has_value()) {
                const base::usize index = param_index.value();
                if (index >= inferred.size()) {
                    result = false;
                } else if (!is_valid(inferred[index])) {
                    inferred[index] = frame.actual;
                } else if (!this->checked_.types.same(inferred[index], frame.actual)) {
                    this->report(range, std::string(context) + " type inference conflict for " + params[index]);
                    result = false;
                }
                continue;
            }
        }

        switch (pattern.kind) {
        case syntax::TypeKind::primitive: {
            const TypeHandle expected = this->resolve_type(frame.pattern_type);
            result = this->can_assign(expected, frame.actual, syntax::invalid_expr_id);
            break;
        }
        case syntax::TypeKind::pointer: {
            if (!this->checked_.types.is_pointer(frame.actual)) {
                result = false;
                break;
            }
            const TypeInfo& actual_info = this->checked_.types.get(frame.actual);
            const PointerMutability expected_mutability = pattern.pointer_mutability == syntax::PointerMutability::mut
                ? PointerMutability::mut
                : PointerMutability::const_;
            if (actual_info.pointer_mutability != expected_mutability) {
                result = false;
                break;
            }
            stack.push_back(GenericTypeInferenceFrame {pattern.pointee, actual_info.pointee});
            break;
        }
        case syntax::TypeKind::array: {
            if (!this->checked_.types.is_array(frame.actual)) {
                result = false;
                break;
            }
            const TypeInfo& actual_info = this->checked_.types.get(frame.actual);
            if (actual_info.array_count != pattern.array_count) {
                result = false;
                break;
            }
            stack.push_back(GenericTypeInferenceFrame {pattern.array_element, actual_info.array_element});
            break;
        }
        case syntax::TypeKind::named: {
            if (!pattern.type_args.empty()) {
                const bool qualified = !pattern.scope_name.empty();
                syntax::ModuleId scope_module = syntax::invalid_module_id;
                if (qualified) {
                    scope_module = this->resolve_import_alias(pattern.scope_name, pattern.scope_range, false);
                    if (!syntax::is_valid(scope_module)) {
                        result = false;
                        break;
                    }
                }

                const GenericStructTemplateInfo* struct_template = qualified
                    ? this->find_generic_struct_template_in_module(scope_module, pattern.name, pattern.range, false)
                    : this->find_generic_struct_template_in_visible_modules(pattern.name, pattern.range, false);
                if (struct_template != nullptr) {
                    const GenericStructInstanceInfo* instance = this->generic_struct_instance(frame.actual);
                    if (instance == nullptr ||
                        instance->name != struct_template->name ||
                        instance->module.value != struct_template->module.value ||
                        instance->args.size() != pattern.type_args.size()) {
                        result = false;
                        break;
                    }
                    for (base::usize i = pattern.type_args.size(); i > 0; --i) {
                        stack.push_back(GenericTypeInferenceFrame {pattern.type_args[i - 1], instance->args[i - 1]});
                    }
                    break;
                }

                const GenericEnumTemplateInfo* enum_template = qualified
                    ? this->find_generic_enum_template_in_module(scope_module, pattern.name, pattern.range, false)
                    : this->find_generic_enum_template_in_visible_modules(pattern.name, pattern.range, false);
                if (enum_template != nullptr) {
                    const GenericEnumInstanceInfo* instance = this->generic_enum_instance(frame.actual);
                    if (instance == nullptr ||
                        instance->name != enum_template->name ||
                        instance->module.value != enum_template->module.value ||
                        instance->args.size() != pattern.type_args.size()) {
                        result = false;
                        break;
                    }
                    for (base::usize i = pattern.type_args.size(); i > 0; --i) {
                        stack.push_back(GenericTypeInferenceFrame {pattern.type_args[i - 1], instance->args[i - 1]});
                    }
                    break;
                }
            }

            const TypeHandle expected = this->resolve_type(frame.pattern_type);
            result = this->can_assign(expected, frame.actual, syntax::invalid_expr_id);
            break;
        }
        }
    }

    this->current_module_ = previous_module;
    return result;
}

TypeHandle SemanticAnalyzer::infer_generic_struct_literal_type(
    const GenericStructTemplateInfo& info,
    const syntax::ExprNode& expr,
    const TypeHandle expected_type
) {
    if (const GenericStructInstanceInfo* expected = generic_struct_instance(expected_type);
        expected != nullptr &&
        expected->name == info.name &&
        expected->module.value == info.module.value &&
        expected->args.size() == info.params.size()) {
        return expected_type;
    }

    const syntax::ItemNode* item = item_from_id(module_, info.item);
    if (item == nullptr) {
        report(expr.range, "invalid generic struct template: " + info.name);
        return invalid_type_handle;
    }

    std::vector<TypeHandle> type_args(info.params.size(), invalid_type_handle);
    for (const syntax::FieldInit& init : expr.field_inits) {
        const syntax::FieldDecl* field_decl = nullptr;
        for (const syntax::FieldDecl& candidate : item->fields) {
            if (candidate.name == init.name) {
                field_decl = &candidate;
                break;
            }
        }
        if (field_decl == nullptr) {
            continue;
        }

        const TypeHandle actual = analyze_expr(init.value);
        static_cast<void>(infer_generic_args_from_type_pattern(
            field_decl->type,
            actual,
            info.params,
            type_args,
            init.range,
            "generic struct literal",
            info.module
        ));
    }

    for (TypeHandle arg : type_args) {
        if (!is_valid(arg)) {
            return invalid_type_handle;
        }
    }
    return instantiate_generic_struct(info, type_args, expr.range);
}

const GenericFunctionInstanceInfo* SemanticAnalyzer::instantiate_generic_function_from_syntax(
    const GenericFunctionTemplateInfo& info,
    const std::vector<syntax::TypeId>& args,
    const base::SourceRange range
) {
    if (args.size() != info.params.size()) {
        report(range, "generic function type argument count mismatch for " + info.name);
        return nullptr;
    }
    std::vector<TypeHandle> resolved_args;
    resolved_args.reserve(args.size());
    for (syntax::TypeId arg : args) {
        resolved_args.push_back(resolve_type(arg));
    }
    return instantiate_generic_function(info, resolved_args, range);
}

const GenericFunctionInstanceInfo* SemanticAnalyzer::instantiate_generic_function(
    const GenericFunctionTemplateInfo& info,
    const std::vector<TypeHandle>& args,
    const base::SourceRange range
) {
    if (args.size() != info.params.size()) {
        report(range, "generic function type argument count mismatch for " + info.name);
        return nullptr;
    }
    for (TypeHandle arg : args) {
        if (!is_valid(arg)) {
            return nullptr;
        }
    }

    const std::string instance_key = generic_instance_key(info, args);
    if (const auto found = generic_function_instances_.find(instance_key); found != generic_function_instances_.end()) {
        return &checked_.generic_function_instances[found->second];
    }

    const syntax::ItemNode* item = item_from_id(module_, info.item);
    if (item == nullptr) {
        report(range, "invalid generic function template: " + info.name);
        return nullptr;
    }
    if (!syntax::is_valid(item->return_type)) {
        report(item->range, "generic function return type must be explicit: " + info.name);
        return nullptr;
    }

    GenericTypeSubstitution substitution;
    for (base::usize i = 0; i < info.params.size(); ++i) {
        substitution.types.emplace(info.params[i], args[i]);
    }

    const syntax::ModuleId previous_module = current_module_;
    const GenericTypeSubstitution* const previous_substitution = current_type_substitution_;
    current_module_ = info.module;
    current_type_substitution_ = &substitution;

    const TypeHandle return_type = resolve_type(item->return_type);
    std::vector<TypeHandle> param_types;
    param_types.reserve(item->params.size());
    for (const syntax::ParamDecl& param : item->params) {
        TypeHandle param_type = resolve_type(param.type);
        if (!is_valid_storage_type(param_type)) {
            report(param.range, "function parameter type is not valid storage");
        }
        if (checked_.types.is_array(param_type)) {
            report(param.range, "array type cannot be used as a function parameter");
        }
        if (checked_.types.contains_array(param_type)) {
            report(param.range, "struct containing array cannot be passed by value");
        }
        param_types.push_back(param_type);
    }
    TypeHandle method_owner_type = invalid_type_handle;
    bool has_self_param = false;
    if (info.is_method) {
        method_owner_type = resolve_type(item->impl_type);
        if (is_valid(method_owner_type)) {
            const TypeKind owner_kind = checked_.types.get(method_owner_type).kind;
            if (owner_kind != TypeKind::struct_ &&
                owner_kind != TypeKind::enum_ &&
                owner_kind != TypeKind::opaque_struct) {
                report(item->range, "impl target must be a named type");
            }
        }
        for (base::usize i = 0; i < item->params.size(); ++i) {
            if (item->params[i].name != "self") {
                continue;
            }
            if (i != 0) {
                report(item->params[i].range, "method self parameter must be first");
            }
            has_self_param = true;
        }
        if (has_self_param && !param_types.empty() && is_valid(method_owner_type)) {
            TypeHandle self_type = param_types.front();
            if (checked_.types.is_pointer(self_type)) {
                self_type = checked_.types.get(self_type).pointee;
            }
            if (!checked_.types.same(self_type, method_owner_type)) {
                report(item->params.front().range, "method self parameter must use the impl type or a pointer to it");
            }
        }
    }
    if (is_valid(return_type)) {
        validate_function_return_type(*item, return_type);
    }

    current_type_substitution_ = previous_substitution;
    current_module_ = previous_module;

    if (!is_valid(return_type) || (info.is_method && !is_valid(method_owner_type))) {
        return nullptr;
    }

    std::string c_name = info.is_method
        ? this->method_c_symbol_name(method_owner_type, info.name)
        : this->generic_c_name(info, args);
    if (info.is_method) {
        for (base::usize i = info.impl_generic_param_count; i < args.size(); ++i) {
            c_name.append(SEMA_GENERIC_C_NAME_ARGUMENT_SEPARATOR);
            append_sanitized_c_name_fragment(c_name, this->checked_.types.c_name(args[i]));
        }
    }

    std::string instance_name = info.is_method
        ? this->checked_.types.display_name(method_owner_type)
        : this->generic_display_name(info, args);
    if (info.is_method) {
        instance_name.append(SEMA_GENERIC_METHOD_DISPLAY_SEPARATOR);
        instance_name += info.name;
    }
    if (info.is_method && args.size() > info.impl_generic_param_count) {
        instance_name.append(SEMA_GENERIC_TEMPLATE_OPEN);
        for (base::usize i = info.impl_generic_param_count; i < args.size(); ++i) {
            if (i != info.impl_generic_param_count) {
                instance_name.append(SEMA_GENERIC_ARGUMENT_SEPARATOR);
            }
            instance_name += this->checked_.types.display_name(args[i]);
        }
        instance_name.append(SEMA_GENERIC_TEMPLATE_CLOSE);
    }

    FunctionSignature signature;
    signature.name = info.name;
    signature.c_name = c_name;
    signature.module = info.module;
    signature.method_owner_type = method_owner_type;
    signature.return_type = return_type;
    signature.param_types = param_types;
    signature.range = item->range;
    signature.has_definition = true;
    signature.visibility = info.visibility;
    signature.definition_item = info.item;
    signature.is_method = info.is_method;
    signature.has_self_param = has_self_param;

    const base::u32 instance_index = static_cast<base::u32>(checked_.generic_function_instances.size());
    GenericFunctionInstanceInfo instance;
    instance.name = std::move(instance_name);
    instance.c_name = signature.c_name;
    instance.module = info.module;
    instance.item = info.item;
    instance.method_owner_type = method_owner_type;
    instance.args = args;
    instance.return_type = return_type;
    instance.param_types = param_types;
    instance.visibility = info.visibility;
    instance.is_method = info.is_method;
    instance.has_self_param = has_self_param;
    checked_.generic_function_instances.push_back(std::move(instance));
    generic_function_instances_[instance_key] = instance_index;

    std::unordered_map<base::u32, TypeHandle> syntax_type_handles;
    std::unordered_map<base::u32, TypeHandle> expr_types;
    std::unordered_map<base::u32, std::string> expr_c_names;
    std::unordered_map<base::u32, std::string> pattern_c_names;
    std::unordered_map<base::u32, std::unordered_set<std::string>> pattern_case_sets;
    std::unordered_map<base::u32, TypeHandle> stmt_local_types;
    auto* const previous_syntax_type_handles = current_generic_syntax_type_handles_;
    auto* const previous_expr_types = current_generic_expr_types_;
    auto* const previous_expr_c_names = current_generic_expr_c_names_;
    auto* const previous_pattern_c_names = current_generic_pattern_c_names_;
    auto* const previous_pattern_case_sets = current_generic_pattern_case_sets_;
    auto* const previous_stmt_local_types = current_generic_stmt_local_types_;
    current_generic_syntax_type_handles_ = &syntax_type_handles;
    current_generic_expr_types_ = &expr_types;
    current_generic_expr_c_names_ = &expr_c_names;
    current_generic_pattern_c_names_ = &pattern_c_names;
    current_generic_pattern_case_sets_ = &pattern_case_sets;
    current_generic_stmt_local_types_ = &stmt_local_types;

    FunctionBodyState& state = generic_function_body_states_[instance_key];
    analyze_function_body_with_signature(*item, instance_key, signature, state, &substitution);
    current_generic_syntax_type_handles_ = previous_syntax_type_handles;
    current_generic_expr_types_ = previous_expr_types;
    current_generic_expr_c_names_ = previous_expr_c_names;
    current_generic_pattern_c_names_ = previous_pattern_c_names;
    current_generic_pattern_case_sets_ = previous_pattern_case_sets;
    current_generic_stmt_local_types_ = previous_stmt_local_types;

    GenericFunctionInstanceInfo& stored = checked_.generic_function_instances[instance_index];
    stored.syntax_type_handles = std::move(syntax_type_handles);
    stored.expr_types = std::move(expr_types);
    stored.expr_c_names = std::move(expr_c_names);
    stored.pattern_c_names = std::move(pattern_c_names);
    stored.pattern_case_sets = std::move(pattern_case_sets);
    stored.stmt_local_types = std::move(stmt_local_types);
    return &stored;
}

bool SemanticAnalyzer::infer_generic_function_args(
    const GenericFunctionTemplateInfo& info,
    const std::vector<TypeHandle>& arg_types,
    const TypeHandle expected_type,
    std::vector<TypeHandle>& inferred,
    const base::SourceRange range
) {
    const syntax::ItemNode* item = item_from_id(module_, info.item);
    if (item == nullptr) {
        return false;
    }
    const std::string_view context = info.is_method ? "generic method" : "generic function";
    bool ok = true;
    if (syntax::is_valid(item->return_type) && is_valid(expected_type)) {
        if (!infer_generic_args_from_type_pattern(
                item->return_type,
                expected_type,
                info.params,
                inferred,
                range,
                context,
                info.module
            )) {
            ok = false;
        }
    }
    const base::usize count = arg_types.size() < item->params.size() ? arg_types.size() : item->params.size();
    for (base::usize i = 0; i < count; ++i) {
        if (!infer_generic_args_from_type_pattern(
                item->params[i].type,
                arg_types[i],
                info.params,
                inferred,
                range,
                context,
                info.module
            )) {
            ok = false;
        }
    }
    return ok;
}

const GenericEnumInstanceInfo* SemanticAnalyzer::generic_enum_instance(const TypeHandle type) const noexcept {
    if (!is_valid(type)) {
        return nullptr;
    }
    const auto found = generic_enum_instance_infos_.find(type.value);
    return found == generic_enum_instance_infos_.end() ? nullptr : &found->second;
}

const GenericStructInstanceInfo* SemanticAnalyzer::generic_struct_instance(const TypeHandle type) const noexcept {
    if (!is_valid(type)) {
        return nullptr;
    }
    const auto found = generic_struct_instance_infos_.find(type.value);
    return found == generic_struct_instance_infos_.end() ? nullptr : &found->second;
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
    const GenericEnumTemplateInfo* info = nullptr;
    if (enum_name.scope_name.empty()) {
        info = find_generic_enum_template_in_visible_modules(enum_name.text, callee.range, false);
    } else {
        const syntax::ModuleId scope_module = resolve_import_alias(enum_name.scope_name, enum_name.scope_range, false);
        if (syntax::is_valid(scope_module)) {
            info = find_generic_enum_template_in_module(scope_module, enum_name.text, callee.range, false);
        }
    }
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
        if (syntax::is_valid(case_decl->payload_type) && arg_types.size() == SEMA_GENERIC_ENUM_CONSTRUCTOR_PAYLOAD_ARITY) {
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
