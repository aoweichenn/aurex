#include "aurex/sema/sema.hpp"

#include "aurex/sema/function_registry.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace aurex::sema {

namespace {

[[nodiscard]] bool is_main_argv_type(const TypeTable& types, const TypeHandle type) noexcept {
    if (!types.is_pointer(type)) {
        return false;
    }
    const TypeInfo& outer = types.get(type);
    if (outer.pointer_mutability != PointerMutability::mut) {
        return false;
    }
    const TypeHandle outer_pointee = outer.pointee;
    if (!types.is_pointer(outer_pointee)) {
        return false;
    }
    const TypeInfo& inner = types.get(outer_pointee);
    return inner.pointer_mutability == PointerMutability::mut &&
           types.same(inner.pointee, types.builtin(BuiltinType::u8));
}

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

[[nodiscard]] bool is_const_evaluable_binary_op(const syntax::BinaryOp op) noexcept {
    switch (op) {
    case syntax::BinaryOp::add:
    case syntax::BinaryOp::sub:
    case syntax::BinaryOp::mul:
    case syntax::BinaryOp::less:
    case syntax::BinaryOp::less_equal:
    case syntax::BinaryOp::greater:
    case syntax::BinaryOp::greater_equal:
    case syntax::BinaryOp::equal:
    case syntax::BinaryOp::not_equal:
    case syntax::BinaryOp::bit_and:
    case syntax::BinaryOp::bit_xor:
    case syntax::BinaryOp::bit_or:
        return true;
    case syntax::BinaryOp::div:
    case syntax::BinaryOp::mod:
    case syntax::BinaryOp::shl:
    case syntax::BinaryOp::shr:
    case syntax::BinaryOp::logical_and:
    case syntax::BinaryOp::logical_or:
        return false;
    }
    return false;
}

struct AbiFunctionInfo {
    std::string name;
    TypeHandle return_type = invalid_type_handle;
    std::vector<TypeHandle> param_types;
    base::SourceRange range {};
    bool is_extern_c = false;
    bool is_variadic = false;
};

struct AbiSymbolInfo {
    std::string name;
    base::SourceRange range {};
    bool is_function = false;
    AbiFunctionInfo function;
};

} // namespace

void SemanticAnalyzer::register_type_names() {
    for (const syntax::ItemNode& item : module_.items) {
        const syntax::ModuleId owner = item_module(item);
        const std::string key = module_key(owner, item.name);
        const std::string qualified = qualified_name(owner, item.name);
        const std::string c_name = c_symbol_name(owner, item.name);
        TypeHandle handle = invalid_type_handle;
        if (item.kind == syntax::ItemKind::type_alias) {
            TypeAliasInfo alias;
            alias.name = std::string(item.name);
            alias.module = owner;
            alias.target = item.alias_type;
            alias.range = item.range;
            alias.visibility = item.visibility;
            auto alias_inserted = checked_.type_aliases.emplace(key, std::move(alias));
            if (!alias_inserted.second) {
                report(item.range, "duplicate type definition in module " + module_name(owner) + ": " + std::string(item.name));
            }
            if (named_types_.contains(key)) {
                report(item.range, "duplicate type definition in module " + module_name(owner) + ": " + std::string(item.name));
            }
            continue;
        }
        if (item.kind == syntax::ItemKind::struct_decl && !item.generic_params.empty()) {
            GenericStructTemplateInfo info;
            info.name = std::string(item.name);
            info.module = owner;
            const auto* const begin = module_.items.data();
            const base::usize item_index = static_cast<base::usize>(&item - begin);
            info.item = syntax::ItemId {static_cast<base::u32>(item_index)};
            info.range = item.range;
            info.visibility = item.visibility;
            info.is_noncopy = item.is_noncopy;
            for (std::string_view param : item.generic_params) {
                const std::string param_name(param);
                if (std::find(info.params.begin(), info.params.end(), param_name) != info.params.end()) {
                    report(item.range, "duplicate generic parameter in struct " + std::string(item.name) + ": " + param_name);
                }
                info.params.push_back(param_name);
            }
            auto inserted = generic_struct_templates_.emplace(key, std::move(info));
            if (!inserted.second) {
                report(item.range, "duplicate type definition in module " + module_name(owner) + ": " + std::string(item.name));
            }
            if (named_types_.contains(key) || checked_.type_aliases.contains(key)) {
                report(item.range, "duplicate type definition in module " + module_name(owner) + ": " + std::string(item.name));
            }
            continue;
        }
        if (item.kind == syntax::ItemKind::enum_decl && !item.generic_params.empty()) {
            GenericEnumTemplateInfo info;
            info.name = std::string(item.name);
            info.module = owner;
            const auto* const begin = module_.items.data();
            const base::usize item_index = static_cast<base::usize>(&item - begin);
            info.item = syntax::ItemId {static_cast<base::u32>(item_index)};
            info.range = item.range;
            info.visibility = item.visibility;
            for (std::string_view param : item.generic_params) {
                const std::string param_name(param);
                if (std::find(info.params.begin(), info.params.end(), param_name) != info.params.end()) {
                    report(item.range, "duplicate generic parameter in enum " + std::string(item.name) + ": " + param_name);
                }
                info.params.push_back(param_name);
            }
            auto inserted = generic_enum_templates_.emplace(key, std::move(info));
            if (!inserted.second) {
                report(item.range, "duplicate type definition in module " + module_name(owner) + ": " + std::string(item.name));
            }
            if (named_types_.contains(key) || checked_.type_aliases.contains(key)) {
                report(item.range, "duplicate type definition in module " + module_name(owner) + ": " + std::string(item.name));
            }
            continue;
        }
        if (item.kind == syntax::ItemKind::struct_decl) {
            handle = checked_.types.named_struct(qualified, c_name, false);
        } else if (item.kind == syntax::ItemKind::enum_decl) {
            handle = checked_.types.named_enum(qualified, c_name);
        } else if (item.kind == syntax::ItemKind::opaque_struct_decl) {
            handle = checked_.types.opaque_struct(qualified, c_name);
        }

        if (!is_valid(handle)) {
            continue;
        }
        const auto* const begin = module_.items.data();
        const base::usize item_index = static_cast<base::usize>(&item - begin);
        if (item_index < checked_.item_c_names.size()) {
            checked_.item_c_names[item_index] = c_name;
        }
        auto inserted = named_types_.emplace(key, handle);
        if (!inserted.second) {
            report(item.range, "duplicate type definition in module " + module_name(owner) + ": " + std::string(item.name));
            continue;
        }
        type_visibilities_[key] = item.visibility;
        if (checked_.type_aliases.contains(key)) {
            report(item.range, "duplicate type definition in module " + module_name(owner) + ": " + std::string(item.name));
            continue;
        }

        if (item.kind == syntax::ItemKind::struct_decl || item.kind == syntax::ItemKind::opaque_struct_decl) {
            StructInfo info;
            info.name = std::string(item.name);
            info.c_name = c_name;
            info.module = owner;
            info.type = handle;
            info.is_opaque = item.kind == syntax::ItemKind::opaque_struct_decl;
            info.is_noncopy = item.is_noncopy;
            info.visibility = item.visibility;
            auto struct_inserted = checked_.structs.emplace(key, std::move(info));
            if (!struct_inserted.second) {
                report(item.range, "duplicate struct definition in module " + module_name(owner) + ": " + std::string(item.name));
            } else {
                struct_infos_by_type_[handle.value] = &struct_inserted.first->second;
            }
        }
    }
}

void SemanticAnalyzer::resolve_type_alias_decls() {
    for (const auto& entry : checked_.type_aliases) {
        static_cast<void>(resolve_type_alias(entry.second, false));
    }
}

void SemanticAnalyzer::register_value_names() {
    FunctionRegistry functions(checked_, global_values_, diagnostics_);
    for (const syntax::ItemNode& item : module_.items) {
        current_module_ = item_module(item);
        std::string key = module_key(current_module_, item.name);
        std::string c_name = c_symbol_name(current_module_, item.name);
        if (item.kind == syntax::ItemKind::fn_decl) {
            const bool is_method = syntax::is_valid(item.impl_type);
            if (!item.generic_params.empty()) {
                if (item.is_extern_c) {
                    report(item.range, "generic extern c functions are not supported");
                }
                if (item.is_export_c) {
                    report(item.range, "generic export c functions are not supported");
                }
                if (item.is_prototype) {
                    report(item.range, "generic function prototypes are not supported");
                }
                if (item.is_variadic) {
                    report(item.range, "generic variadic functions are not supported");
                }
                if (!syntax::is_valid(item.return_type)) {
                    report(item.range, "generic function return type must be explicit: " + std::string(item.name));
                }
                GenericFunctionTemplateInfo info;
                info.name = std::string(item.name);
                info.module = current_module_;
                const auto* const begin = module_.items.data();
                const base::usize item_index = static_cast<base::usize>(&item - begin);
                info.item = syntax::ItemId {static_cast<base::u32>(item_index)};
                info.impl_type = item.impl_type;
                info.range = item.range;
                info.visibility = item.visibility;
                info.is_method = is_method;
                info.impl_generic_param_count = item.impl_generic_param_count;
                for (std::string_view param : item.generic_params) {
                    const std::string param_name(param);
                    if (std::find(info.params.begin(), info.params.end(), param_name) != info.params.end()) {
                        report(item.range, "duplicate generic parameter in function " + std::string(item.name) + ": " + param_name);
                    }
                    info.params.push_back(param_name);
                }
                if (is_method) {
                    const base::u32 method_index = static_cast<base::u32>(generic_method_templates_.size());
                    generic_method_templates_.push_back(std::move(info));
                    generic_method_template_indices_.emplace(std::string(item.name), method_index);
                    continue;
                }
                const auto inserted = generic_function_templates_.emplace(key, std::move(info));
                if (!inserted.second || global_values_.contains(key)) {
                    report(item.range, "duplicate value definition in module " + module_name(current_module_) + ": " + std::string(item.name));
                }
                continue;
            }
            TypeHandle method_owner_type = invalid_type_handle;
            if (item.is_variadic && !item.is_extern_c) {
                report(item.range, "variadic functions are only supported for extern c declarations");
            }
            if (is_method) {
                method_owner_type = resolve_type(item.impl_type);
                if (is_valid(method_owner_type)) {
                    const TypeKind owner_kind = checked_.types.get(method_owner_type).kind;
                    if (owner_kind != TypeKind::struct_ &&
                        owner_kind != TypeKind::enum_ &&
                        owner_kind != TypeKind::opaque_struct) {
                        report(item.range, "impl target must be a named type");
                    }
                }
                key = method_key(current_module_, method_owner_type, item.name);
                c_name = method_c_symbol_name(method_owner_type, item.name);
            }
            if (generic_function_templates_.contains(key)) {
                report(item.range, "duplicate value definition in module " + module_name(current_module_) + ": " + std::string(item.name));
            }
            const bool has_explicit_return = syntax::is_valid(item.return_type);
            TypeHandle return_type = invalid_type_handle;
            if (has_explicit_return) {
                return_type = resolve_type(item.return_type);
            } else if (item.is_extern_c || item.is_export_c) {
                report(item.range, "C ABI function return type must be explicit");
                return_type = checked_.types.builtin(BuiltinType::void_);
            } else if (item.is_prototype) {
                report(item.range, "function prototype return type must be explicit");
                return_type = checked_.types.builtin(BuiltinType::void_);
            } else {
                return_type = invalid_type_handle;
            }
            std::vector<TypeHandle> param_types;
            for (const syntax::ParamDecl& param : item.params) {
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
            if (is_method) {
                bool saw_self = false;
                for (base::usize i = 0; i < item.params.size(); ++i) {
                    if (item.params[i].name != "self") {
                        continue;
                    }
                    if (i != 0) {
                        report(item.params[i].range, "method self parameter must be first");
                    }
                    saw_self = true;
                }
                if (saw_self && !param_types.empty() && is_valid(method_owner_type)) {
                    TypeHandle self_type = param_types.front();
                    if (checked_.types.is_pointer(self_type)) {
                        self_type = checked_.types.get(self_type).pointee;
                    }
                    if (!checked_.types.same(self_type, method_owner_type)) {
                        report(item.params.front().range, "method self parameter must use the impl type or a pointer to it");
                    }
                }
            }
            if (has_explicit_return && is_valid(return_type)) {
                validate_function_return_type(item, return_type);
            }
            const auto* const begin = module_.items.data();
            const base::usize item_index = static_cast<base::usize>(&item - begin);
            functions.register_function(
                item,
                current_module_,
                key,
                c_name,
                method_owner_type,
                return_type,
                std::move(param_types),
                syntax::ItemId {static_cast<base::u32>(item_index)}
            );
            if (!item.is_prototype && !item.is_extern_c) {
                function_definition_items_[key] = syntax::ItemId {static_cast<base::u32>(item_index)};
            }
            function_body_states_[key] = FunctionBodyState::not_started;
        } else if (item.kind == syntax::ItemKind::const_decl) {
            TypeHandle type = resolve_type(item.const_type);
            const auto* const begin = module_.items.data();
            const base::usize item_index = static_cast<base::usize>(&item - begin);
            if (item_index < checked_.item_c_names.size()) {
                checked_.item_c_names[item_index] = c_name;
            }
            const auto inserted = global_values_.emplace(key, Symbol {
                SymbolKind::const_,
                std::string(item.name),
                c_name,
                current_module_,
                type,
                item.range,
                false,
                item.visibility,
            });
            if (!inserted.second) {
                report(item.range, "duplicate value definition in module " + module_name(current_module_) + ": " + std::string(item.name));
            }
        } else if (item.kind == syntax::ItemKind::enum_decl) {
            std::unordered_set<std::string> seen_cases;
            for (const syntax::EnumCaseDecl& enum_case : item.enum_cases) {
                if (!seen_cases.insert(std::string(enum_case.name)).second) {
                    report(enum_case.range, "duplicate enum case: " + std::string(item.name) + "." + std::string(enum_case.name));
                }
            }
            if (!item.generic_params.empty()) {
                continue;
            }
            const TypeHandle enum_type = resolve_type(item.enum_base_type);
            if (!checked_.types.is_integer(enum_type)) {
                report(item.range, "enum base type must be an integer type");
            }
            std::unordered_set<base::u64> seen_values;
            const auto type_found = named_types_.find(key);
            const TypeHandle named_enum_type = type_found == named_types_.end() ? enum_type : type_found->second;
            if (is_valid(named_enum_type)) {
                checked_.types.set_enum_underlying(named_enum_type, enum_type);
            }
            TypeHandle payload_storage = invalid_type_handle;
            base::u64 payload_size = 0;
            base::u64 payload_align = 1;
            for (const syntax::EnumCaseDecl& enum_case : item.enum_cases) {
                const std::string full_name = std::string(item.name) + "_" + std::string(enum_case.name);
                const std::string enum_case_key = module_key(current_module_, full_name);
                const bool has_payload = syntax::is_valid(enum_case.payload_type);
                const TypeHandle payload_type = has_payload ? resolve_type(enum_case.payload_type) : invalid_type_handle;
                base::u64 discriminant = 0;
                const bool parsed_discriminant = parse_integer_literal_text(enum_case.value_text, discriminant);
                if (!parsed_discriminant) {
                    report(enum_case.range, "enum discriminant literal is out of range");
                } else if (!integer_literal_fits_type(enum_type, enum_case.value_text)) {
                    report(enum_case.range, "enum discriminant does not fit enum base type");
                } else if (!seen_values.insert(discriminant).second) {
                    report(enum_case.range, "duplicate enum discriminant value in " + std::string(item.name));
                }
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
                        report(enum_case.range, "enum payload cannot contain array storage");
                    }
                }
                const auto case_inserted = checked_.enum_cases.emplace(enum_case_key, EnumCaseInfo {
                    full_name,
                    c_symbol_name(current_module_, full_name),
                    current_module_,
                    named_enum_type,
                    payload_type,
                    std::string(enum_case.value_text),
                    enum_case.range,
                    std::string(item.name),
                    std::string(enum_case.name),
                    item.visibility,
                });
                if (!case_inserted.second) {
                    report(enum_case.range, "duplicate enum case: " + std::string(item.name) + "." + std::string(enum_case.name));
                    continue;
                }
                index_enum_case(case_inserted.first->second);
                if (!has_payload) {
                    const auto value_inserted = global_values_.emplace(enum_case_key, Symbol {
                        SymbolKind::enum_case,
                        full_name,
                        c_symbol_name(current_module_, full_name),
                        current_module_,
                        named_enum_type,
                        enum_case.range,
                        false,
                        item.visibility,
                    });
                    if (!value_inserted.second) {
                        report(enum_case.range, "duplicate value definition in module " + module_name(current_module_) + ": " + full_name);
                    }
                }
            }
            if (is_valid(named_enum_type) && is_valid(payload_storage)) {
                checked_.types.set_enum_payload_layout(
                    named_enum_type,
                    payload_storage_type(checked_.types, payload_size, payload_align),
                    payload_size,
                    payload_align
                );
            }
        }
    }
    current_module_ = syntax::invalid_module_id;
}

void SemanticAnalyzer::validate_function_prototypes() {
    for (const auto& entry : checked_.functions) {
        const FunctionSignature& signature = entry.second;
        if (signature.is_extern_c) {
            continue;
        }
        if (signature.has_conflict) {
            continue;
        }
        if (signature.has_prototype && !signature.has_definition) {
            report(signature.range, "function prototype has no definition: " + signature.name);
        }
    }
}

void SemanticAnalyzer::validate_abi_symbols() {
    std::unordered_map<std::string, AbiSymbolInfo> symbols;

    const auto same_function_type = [&](const AbiFunctionInfo& lhs, const AbiFunctionInfo& rhs) {
        if (!checked_.types.same(lhs.return_type, rhs.return_type) ||
            lhs.is_variadic != rhs.is_variadic ||
            lhs.param_types.size() != rhs.param_types.size()) {
            return false;
        }
        for (base::usize i = 0; i < lhs.param_types.size(); ++i) {
            if (!checked_.types.same(lhs.param_types[i], rhs.param_types[i])) {
                return false;
            }
        }
        return true;
    };

    const auto insert_function = [&](const std::string& symbol, AbiFunctionInfo function) {
        if (symbol.empty()) {
            return;
        }
        const auto found = symbols.find(symbol);
        if (found == symbols.end()) {
            AbiSymbolInfo info;
            info.name = function.name;
            info.range = function.range;
            info.is_function = true;
            info.function = std::move(function);
            symbols.emplace(symbol, std::move(info));
            return;
        }

        const AbiSymbolInfo& prior = found->second;
        if (prior.is_function && prior.function.is_extern_c && function.is_extern_c) {
            if (!same_function_type(prior.function, function)) {
                report(function.range, "extern C ABI symbol redeclared with incompatible signature: " + symbol);
            }
            return;
        }
        report(function.range, "duplicate ABI symbol: " + symbol);
    };

    for (const auto& entry : checked_.functions) {
        const FunctionSignature& signature = entry.second;
        if (signature.has_conflict) {
            continue;
        }
        insert_function(signature.c_name, AbiFunctionInfo {
            signature.name,
            signature.return_type,
            signature.param_types,
            signature.range,
            signature.is_extern_c,
            signature.is_variadic,
        });
    }

    for (const GenericFunctionInstanceInfo& instance : checked_.generic_function_instances) {
        insert_function(instance.c_name, AbiFunctionInfo {
            instance.name,
            instance.return_type,
            instance.param_types,
            instance.item.value < module_.items.size() ? module_.items[instance.item.value].range : base::SourceRange {},
            false,
            false,
        });
    }

    for (const auto& entry : global_values_) {
        const Symbol& symbol = entry.second;
        if (symbol.kind == SymbolKind::function || symbol.c_name.empty()) {
            continue;
        }
        const auto found = symbols.find(symbol.c_name);
        if (found == symbols.end()) {
            AbiSymbolInfo info;
            info.name = symbol.name;
            info.range = symbol.range;
            info.is_function = false;
            symbols.emplace(symbol.c_name, std::move(info));
            continue;
        }
        report(symbol.range, "duplicate ABI symbol: " + symbol.c_name);
    }
}

void SemanticAnalyzer::analyze_entry_points() {
    const syntax::ModuleId root_module {0};
    const FunctionSignature* aurex_entry = nullptr;
    const FunctionSignature* c_entry = nullptr;

    for (const auto& entry : checked_.functions) {
        const FunctionSignature& function = entry.second;
        if (function.module.value != root_module.value) {
            continue;
        }
        if (function.is_method) {
            continue;
        }
        if (function.is_extern_c) {
            continue;
        }
        if (function.is_export_c && (function.name == "main" || function.c_name == "main")) {
            c_entry = &function;
            continue;
        }
        if (!function.is_export_c && function.name == "main") {
            aurex_entry = &function;
        }
    }

    if (aurex_entry == nullptr) {
        return;
    }
    if (c_entry != nullptr) {
        report(
            aurex_entry->range,
            "ordinary fn main cannot be combined with an exported C main entry"
        );
    }
    if (aurex_entry->c_name == "main") {
        report(aurex_entry->range, "ordinary fn main cannot use ABI name 'main'");
    }
    const TypeHandle i32_type = checked_.types.builtin(BuiltinType::i32);
    const TypeHandle void_type = checked_.types.builtin(BuiltinType::void_);
    if (aurex_entry->param_types.empty()) {
        // fn main() -> i32
    } else if (aurex_entry->param_types.size() == 2) {
        if (!checked_.types.same(aurex_entry->param_types[0], i32_type) ||
            !is_main_argv_type(checked_.types, aurex_entry->param_types[1])) {
            report(aurex_entry->range, "ordinary fn main parameters must be (argc: i32, argv: *mut *mut u8)");
        }
    } else {
        report(aurex_entry->range, "ordinary fn main must use either no parameters or (argc: i32, argv: *mut *mut u8)");
    }
    if (!checked_.types.same(aurex_entry->return_type, i32_type) &&
        !checked_.types.same(aurex_entry->return_type, void_type)) {
        report(aurex_entry->range, "ordinary fn main must return i32 or void");
    }
}

void SemanticAnalyzer::analyze_struct_properties() {
    for (const syntax::ItemNode& item : module_.items) {
        if (item.kind != syntax::ItemKind::struct_decl) {
            continue;
        }
        current_module_ = item_module(item);
        const std::string key = module_key(current_module_, item.name);
        bool contains_array = false;
        bool copyable = !item.is_noncopy;
        std::unordered_set<std::string> seen_fields;
        for (const syntax::FieldDecl& field : item.fields) {
            if (!seen_fields.insert(std::string(field.name)).second) {
                report(field.range, "duplicate struct field: " + std::string(field.name));
                continue;
            }
            if (!item.generic_params.empty()) {
                continue;
            }
            const TypeHandle field_type = resolve_type(field.type);
            if (!is_valid_storage_type(field_type)) {
                report(field.range, "field type is not valid storage");
            }
            if (const auto struct_found = checked_.structs.find(key); struct_found != checked_.structs.end()) {
                struct_found->second.fields.push_back(StructFieldInfo {
                    std::string(field.name),
                    {},
                    syntax::invalid_module_id,
                    field_type,
                    field.range,
                    field.visibility,
                });
            }
            if (checked_.types.contains_array(field_type)) {
                contains_array = true;
            }
            if (!checked_.types.is_copyable(field_type)) {
                copyable = false;
            }
        }
        if (!item.generic_params.empty()) {
            continue;
        }
        const auto found = named_types_.find(key);
        if (found != named_types_.end()) {
            checked_.types.set_record_properties(found->second, contains_array, copyable && !contains_array);
        }
    }
    current_module_ = syntax::invalid_module_id;
}

void SemanticAnalyzer::analyze_const_decls() {
    std::unordered_map<std::string, std::vector<std::string>> dependencies_by_const;
    std::unordered_map<std::string, base::SourceRange> const_ranges;
    std::unordered_map<std::string, std::string> const_names;

    for (const syntax::ItemNode& item : module_.items) {
        if (item.kind != syntax::ItemKind::const_decl) {
            continue;
        }
        current_module_ = item_module(item);
        const std::string const_key = module_key(current_module_, item.name);
        const_ranges[const_key] = item.range;
        const_names[const_key] = qualified_name(current_module_, item.name);
        const TypeHandle declared = resolve_type(item.const_type);
        const bool previous_const_initializer = in_const_initializer_;
        in_const_initializer_ = true;
        const TypeHandle actual = analyze_expr(item.const_value, declared);
        in_const_initializer_ = previous_const_initializer;
        std::unordered_set<std::string> dependencies;
        if (!is_const_evaluable_expr(item.const_value, dependencies)) {
            const base::SourceRange range =
                syntax::is_valid(item.const_value) && item.const_value.value < module_.exprs.size()
                    ? module_.exprs[item.const_value.value].range
                    : item.range;
            report(range, "const initializer is not compile-time constant");
        }
        dependencies_by_const[const_key] = std::vector<std::string>(dependencies.begin(), dependencies.end());
        if (!is_valid_storage_type(declared)) {
            report(item.range, "const type is not valid storage");
        }
        if (!can_assign(declared, actual, item.const_value)) {
            report(item.range, "const initializer type does not match declared type");
        }
    }

    std::unordered_map<std::string, base::u8> states;
    const auto visit = [&](const auto& self, const std::string& key) -> void {
        const base::u8 state = states[key];
        if (state == 2) {
            return;
        }
        if (state == 1) {
            const auto range = const_ranges.find(key);
            report(
                range == const_ranges.end() ? base::SourceRange {} : range->second,
                "cyclic const initializer: " + (const_names.contains(key) ? const_names[key] : key)
            );
            states[key] = 2;
            return;
        }
        states[key] = 1;
        if (const auto found = dependencies_by_const.find(key); found != dependencies_by_const.end()) {
            for (const std::string& dependency : found->second) {
                if (dependencies_by_const.contains(dependency)) {
                    self(self, dependency);
                }
            }
        }
        states[key] = 2;
    };
    for (const auto& entry : dependencies_by_const) {
        visit(visit, entry.first);
    }
    current_module_ = syntax::invalid_module_id;
}

bool SemanticAnalyzer::is_const_evaluable_expr(
    const syntax::ExprId expr_id,
    std::unordered_set<std::string>& dependencies
) {
    if (!syntax::is_valid(expr_id) || expr_id.value >= module_.exprs.size()) {
        return false;
    }
    const syntax::ExprNode& expr = module_.exprs[expr_id.value];
    switch (expr.kind) {
    case syntax::ExprKind::integer_literal:
    case syntax::ExprKind::bool_literal:
    case syntax::ExprKind::null_literal:
    case syntax::ExprKind::string_literal:
    case syntax::ExprKind::c_string_literal:
    case syntax::ExprKind::byte_literal:
    case syntax::ExprKind::size_of:
    case syntax::ExprKind::align_of:
        return true;
    case syntax::ExprKind::name: {
        const Symbol* symbol = nullptr;
        if (!expr.scope_name.empty()) {
            const syntax::ModuleId module = resolve_import_alias(expr.scope_name, expr.scope_range, false);
            symbol = syntax::is_valid(module) ? find_symbol_in_module(module, expr.text, expr.range, false) : nullptr;
        } else {
            if (const Symbol* local = symbols_.find(expr.text); local != nullptr) {
                symbol = local;
            } else if (const auto found = global_values_.find(module_key(current_module_, expr.text)); found != global_values_.end()) {
                symbol = &found->second;
            } else {
                for (syntax::ModuleId module : visible_modules(current_module_)) {
                    if (module.value == current_module_.value) {
                        continue;
                    }
                    const auto imported = global_values_.find(module_key(module, expr.text));
                    if (imported != global_values_.end() && can_access(module, imported->second.visibility)) {
                        symbol = &imported->second;
                        break;
                    }
                }
            }
        }
        if (symbol == nullptr) {
            return false;
        }
        if (symbol->kind == SymbolKind::enum_case) {
            return true;
        }
        if (symbol->kind != SymbolKind::const_) {
            return false;
        }
        dependencies.insert(module_key(symbol->module, symbol->name));
        return true;
    }
    case syntax::ExprKind::field: {
        if (expr_id.value < checked_.expr_c_names.size() && !checked_.expr_c_names[expr_id.value].empty()) {
            for (const auto& entry : checked_.enum_cases) {
                if (entry.second.c_name == checked_.expr_c_names[expr_id.value]) {
                    return true;
                }
            }
        }
        return false;
    }
    case syntax::ExprKind::struct_literal: {
        bool ok = true;
        for (const syntax::FieldInit& init : expr.field_inits) {
            ok = is_const_evaluable_expr(init.value, dependencies) && ok;
        }
        return ok;
    }
    case syntax::ExprKind::unary:
        return (expr.unary_op == syntax::UnaryOp::logical_not ||
                expr.unary_op == syntax::UnaryOp::numeric_negate ||
                expr.unary_op == syntax::UnaryOp::bitwise_not) &&
               is_const_evaluable_expr(expr.unary_operand, dependencies);
    case syntax::ExprKind::binary:
        return is_const_evaluable_binary_op(expr.binary_op) &&
               is_const_evaluable_expr(expr.binary_lhs, dependencies) &&
               is_const_evaluable_expr(expr.binary_rhs, dependencies);
    case syntax::ExprKind::cast:
    case syntax::ExprKind::ptr_cast:
    case syntax::ExprKind::bit_cast:
    case syntax::ExprKind::ptr_addr:
    case syntax::ExprKind::ptr_from_addr:
        return is_const_evaluable_expr(expr.cast_expr, dependencies);
    default:
        return false;
    }
}

} // namespace aurex::sema
