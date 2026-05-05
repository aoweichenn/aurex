#include "aurex/sema/sema.hpp"

#include "aurex/sema/function_registry.hpp"
#include "aurex/syntax/module.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <utility>

namespace aurex::sema {

namespace {

[[nodiscard]] BuiltinType map_builtin(const syntax::PrimitiveTypeKind kind) noexcept {
    switch (kind) {
    case syntax::PrimitiveTypeKind::void_: return BuiltinType::void_;
    case syntax::PrimitiveTypeKind::bool_: return BuiltinType::bool_;
    case syntax::PrimitiveTypeKind::i8: return BuiltinType::i8;
    case syntax::PrimitiveTypeKind::u8: return BuiltinType::u8;
    case syntax::PrimitiveTypeKind::i16: return BuiltinType::i16;
    case syntax::PrimitiveTypeKind::u16: return BuiltinType::u16;
    case syntax::PrimitiveTypeKind::i32: return BuiltinType::i32;
    case syntax::PrimitiveTypeKind::u32: return BuiltinType::u32;
    case syntax::PrimitiveTypeKind::i64: return BuiltinType::i64;
    case syntax::PrimitiveTypeKind::u64: return BuiltinType::u64;
    case syntax::PrimitiveTypeKind::isize: return BuiltinType::isize;
    case syntax::PrimitiveTypeKind::usize: return BuiltinType::usize;
    case syntax::PrimitiveTypeKind::f32: return BuiltinType::f32;
    case syntax::PrimitiveTypeKind::f64: return BuiltinType::f64;
    case syntax::PrimitiveTypeKind::str: return BuiltinType::str;
    }
    return BuiltinType::void_;
}

[[nodiscard]] PointerMutability map_mutability(const syntax::PointerMutability mutability) noexcept {
    return mutability == syntax::PointerMutability::mut ? PointerMutability::mut : PointerMutability::const_;
}

[[nodiscard]] bool is_function_item(const syntax::ItemNode& item) noexcept {
    return item.kind == syntax::ItemKind::fn_decl;
}

[[nodiscard]] bool is_main_argv_type(const TypeTable& types, const TypeHandle type) noexcept {
    if (!types.is_pointer(type)) {
        return false;
    }
    const TypeHandle outer_pointee = types.get(type).pointee;
    if (!types.is_pointer(outer_pointee)) {
        return false;
    }
    return types.same(types.get(outer_pointee).pointee, types.builtin(BuiltinType::u8));
}

[[nodiscard]] base::u64 align_forward(const base::u64 offset, const base::u64 alignment) noexcept {
    if (alignment == 0) {
        return offset;
    }
    const base::u64 mask = alignment - 1;
    return (offset + mask) & ~mask;
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

} // namespace

SemanticAnalyzer::SemanticAnalyzer(const syntax::AstModule& module, base::DiagnosticSink& diagnostics) noexcept
    : module_(module), diagnostics_(diagnostics) {}

base::Result<CheckedModule> SemanticAnalyzer::analyze() {
    checked_.expr_types.assign(module_.exprs.size(), invalid_type_handle);
    checked_.expr_c_names.assign(module_.exprs.size(), {});
    checked_.pattern_c_names.assign(module_.patterns.size(), {});
    checked_.pattern_case_sets.assign(module_.patterns.size(), {});
    checked_.syntax_type_handles.assign(module_.types.size(), invalid_type_handle);
    checked_.stmt_local_types.assign(module_.stmts.size(), invalid_type_handle);
    checked_.item_c_names.assign(module_.items.size(), {});
    register_type_names();
    resolve_type_alias_decls();
    analyze_struct_properties();
    register_value_names();
    validate_function_prototypes();

    for (const syntax::ItemNode& item : module_.items) {
        if (is_function_item(item) && !item.is_extern_c && !item.is_prototype && syntax::is_valid(item.body)) {
            analyze_function_body(item);
        }
    }

    analyze_entry_points();
    analyze_const_decls();

    if (diagnostics_.has_error()) {
        return base::Result<CheckedModule>::fail({base::ErrorCode::sema_error, "semantic analysis failed"});
    }
    return base::Result<CheckedModule>::ok(std::move(checked_));
}

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
            info.visibility = item.visibility;
            auto struct_inserted = checked_.structs.emplace(key, std::move(info));
            if (!struct_inserted.second) {
                report(item.range, "duplicate struct definition in module " + module_name(owner) + ": " + std::string(item.name));
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
            TypeHandle method_owner_type = invalid_type_handle;
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
            if (!item.generic_params.empty()) {
                continue;
            }
            const TypeHandle enum_type = resolve_type(item.enum_base_type);
            if (!checked_.types.is_integer(enum_type)) {
                report(item.range, "enum base type must be an integer type");
            }
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
                checked_.enum_cases.emplace(enum_case_key, EnumCaseInfo {
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
        if (!item.generic_params.empty()) {
            continue;
        }
        current_module_ = item_module(item);
        const std::string key = module_key(current_module_, item.name);
        bool contains_array = false;
        bool copyable = true;
        for (const syntax::FieldDecl& field : item.fields) {
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
        const auto found = named_types_.find(key);
        if (found != named_types_.end()) {
            checked_.types.set_record_properties(found->second, contains_array, copyable && !contains_array);
        }
    }
    current_module_ = syntax::invalid_module_id;
}

void SemanticAnalyzer::analyze_const_decls() {
    for (const syntax::ItemNode& item : module_.items) {
        if (item.kind != syntax::ItemKind::const_decl) {
            continue;
        }
        current_module_ = item_module(item);
        const TypeHandle declared = resolve_type(item.const_type);
        const bool previous_const_initializer = in_const_initializer_;
        in_const_initializer_ = true;
        const TypeHandle actual = analyze_expr(item.const_value);
        in_const_initializer_ = previous_const_initializer;
        if (!is_valid_storage_type(declared)) {
            report(item.range, "const type is not valid storage");
        }
        if (!can_assign(declared, actual, item.const_value)) {
            report(item.range, "const initializer type does not match declared type");
        }
    }
    current_module_ = syntax::invalid_module_id;
}

void SemanticAnalyzer::analyze_function_body(const syntax::ItemNode& function) {
    const syntax::ModuleId previous_module = current_module_;
    const TypeHandle previous_function_return_type = current_function_return_type_;
    const int previous_loop_depth = loop_depth_;
    const SymbolTable previous_symbols = symbols_;
    current_module_ = item_module(function);
    const std::string key = function_key(function);
    const auto found = checked_.functions.find(key);
    if (found == checked_.functions.end()) {
        current_module_ = previous_module;
        loop_depth_ = previous_loop_depth;
        symbols_ = previous_symbols;
        return;
    }
    if (found->second.has_conflict) {
        current_module_ = previous_module;
        loop_depth_ = previous_loop_depth;
        symbols_ = previous_symbols;
        return;
    }
    FunctionBodyState& state = function_body_states_[key];
    if (state == FunctionBodyState::analyzing) {
        report(function.range, "cannot infer recursive function return type without an explicit return type");
        current_module_ = previous_module;
        loop_depth_ = previous_loop_depth;
        symbols_ = previous_symbols;
        return;
    }
    if (state == FunctionBodyState::analyzed) {
        current_module_ = previous_module;
        loop_depth_ = previous_loop_depth;
        symbols_ = previous_symbols;
        return;
    }
    state = FunctionBodyState::analyzing;
    loop_depth_ = 0;
    const bool infer_return_type = !syntax::is_valid(function.return_type);
    ReturnTypeInference return_inference;
    TypeHandle expected_return = found->second.return_type;
    if (infer_return_type) {
        expected_return = invalid_type_handle;
    }
    current_function_return_type_ = expected_return;

    symbols_.push_scope();
    for (const syntax::ParamDecl& param : function.params) {
        static_cast<void>(symbols_.insert(Symbol {
            SymbolKind::parameter,
            std::string(param.name),
            {},
            syntax::invalid_module_id,
            resolve_type(param.type),
            param.range,
            false,
            syntax::Visibility::private_,
        }, diagnostics_));
    }
    analyze_block(function.body, expected_return, infer_return_type ? &return_inference : nullptr);
    symbols_.pop_scope();
    if (infer_return_type) {
        finalize_inferred_return(function, key, return_inference);
    }
    state = FunctionBodyState::analyzed;
    current_module_ = previous_module;
    current_function_return_type_ = previous_function_return_type;
    loop_depth_ = previous_loop_depth;
    symbols_ = previous_symbols;
}

void SemanticAnalyzer::analyze_block(
    const syntax::StmtId block,
    const TypeHandle expected_return,
    ReturnTypeInference* const return_inference
) {
    if (!syntax::is_valid(block) || block.value >= module_.stmts.size()) {
        return;
    }
    symbols_.push_scope();
    const syntax::StmtNode& stmt = module_.stmts[block.value];
    if (stmt.kind != syntax::StmtKind::block) {
        return;
    }
    for (syntax::StmtId child : stmt.statements) {
        analyze_stmt(child, expected_return, return_inference);
    }
    symbols_.pop_scope();
}

void SemanticAnalyzer::analyze_stmt(
    const syntax::StmtId stmt_id,
    const TypeHandle expected_return,
    ReturnTypeInference* const return_inference
) {
    if (!syntax::is_valid(stmt_id) || stmt_id.value >= module_.stmts.size()) {
        return;
    }
    const syntax::StmtNode& stmt = module_.stmts[stmt_id.value];
    switch (stmt.kind) {
    case syntax::StmtKind::let:
    case syntax::StmtKind::var: {
        const bool has_declared_type = syntax::is_valid(stmt.declared_type);
        const TypeHandle declared_type = has_declared_type ? resolve_type(stmt.declared_type) : invalid_type_handle;
        const TypeHandle init = analyze_expr(stmt.init, declared_type);
        const TypeHandle local_type = has_declared_type ? declared_type : init;
        if (stmt_id.value < checked_.stmt_local_types.size()) {
            checked_.stmt_local_types[stmt_id.value] = local_type;
        }
        if (!has_declared_type && !is_valid(local_type)) {
            report(stmt.range, "local variable type cannot be inferred");
        }
        if (is_valid(local_type) && !is_valid_storage_type(local_type)) {
            report(stmt.range, "local variable type is not valid storage");
        }
        if (has_declared_type && !can_assign(local_type, init, stmt.init)) {
            report(stmt.range, "initializer type does not match declared type");
        }
        if (is_valid(local_type) && !checked_.types.is_copyable(local_type)) {
            report(stmt.range, "non-copyable storage type cannot be implicitly copied");
        }
        static_cast<void>(symbols_.insert(Symbol {
            SymbolKind::local,
            std::string(stmt.name),
            {},
            syntax::invalid_module_id,
            local_type,
            stmt.range,
            stmt.kind == syntax::StmtKind::var,
            syntax::Visibility::private_,
        }, diagnostics_));
        break;
    }
    case syntax::StmtKind::assign: {
        if (!is_writable_place(stmt.lhs)) {
            report(module_.exprs[stmt.lhs.value].range, "left side of assignment must be writable");
        }
        const TypeHandle lhs = analyze_expr(stmt.lhs);
        const TypeHandle rhs = analyze_expr(stmt.rhs, lhs);
        if (!can_assign(lhs, rhs, stmt.rhs)) {
            report(stmt.range, "assignment type mismatch");
        }
        if (!checked_.types.is_copyable(lhs)) {
            report(stmt.range, "array or array-containing type cannot be assigned");
        }
        break;
    }
    case syntax::StmtKind::if_: {
        const TypeHandle condition = analyze_expr(stmt.condition);
        if (!checked_.types.is_bool(condition)) {
            report(module_.exprs[stmt.condition.value].range, "if condition must be bool");
        }
        analyze_block(stmt.then_block, expected_return, return_inference);
        if (syntax::is_valid(stmt.else_block)) {
            analyze_block(stmt.else_block, expected_return, return_inference);
        }
        if (syntax::is_valid(stmt.else_if)) {
            analyze_stmt(stmt.else_if, expected_return, return_inference);
        }
        break;
    }
    case syntax::StmtKind::while_: {
        const TypeHandle condition = analyze_expr(stmt.condition);
        if (!checked_.types.is_bool(condition)) {
            report(module_.exprs[stmt.condition.value].range, "while condition must be bool");
        }
        ++loop_depth_;
        analyze_block(stmt.body, expected_return, return_inference);
        --loop_depth_;
        break;
    }
    case syntax::StmtKind::return_: {
        const TypeHandle actual = syntax::is_valid(stmt.return_value)
            ? analyze_expr(stmt.return_value, expected_return)
            : checked_.types.builtin(BuiltinType::void_);
        if (return_inference != nullptr) {
            record_inferred_return(stmt_id, actual, *return_inference);
        } else if (!can_assign(expected_return, actual, stmt.return_value)) {
            report(stmt.range, "return type mismatch");
        }
        break;
    }
    case syntax::StmtKind::expr:
        if (syntax::is_valid(stmt.init) &&
            stmt.init.value < module_.exprs.size() &&
            module_.exprs[stmt.init.value].kind != syntax::ExprKind::call) {
            report(module_.exprs[stmt.init.value].range, "expression statement must be a function call");
        }
        static_cast<void>(analyze_expr(stmt.init));
        break;
    case syntax::StmtKind::block:
        analyze_block(stmt_id, expected_return, return_inference);
        break;
    case syntax::StmtKind::break_:
    case syntax::StmtKind::continue_:
        if (loop_depth_ == 0) {
            report(stmt.range, "break and continue are only valid inside while loops");
        }
        break;
    }
}

void SemanticAnalyzer::record_inferred_return(
    const syntax::StmtId stmt_id,
    const TypeHandle actual,
    ReturnTypeInference& inference
) {
    inference.returns.push_back(stmt_id);
    if (!is_valid(actual)) {
        if (syntax::is_valid(stmt_id) && stmt_id.value < module_.stmts.size()) {
            const syntax::StmtNode& stmt = module_.stmts[stmt_id.value];
            report(stmt.range, "function return type cannot be inferred");
        }
        return;
    }
    if (!is_valid(inference.inferred_type)) {
        inference.inferred_type = actual;
        return;
    }
    if (!checked_.types.same(inference.inferred_type, actual)) {
        if (syntax::is_valid(stmt_id) && stmt_id.value < module_.stmts.size()) {
            const syntax::StmtNode& stmt = module_.stmts[stmt_id.value];
            report(stmt.range, "inferred function return types do not match");
        }
    }
}

void SemanticAnalyzer::finalize_inferred_return(
    const syntax::ItemNode& function,
    const std::string& key,
    ReturnTypeInference& inference
) {
    TypeHandle return_type = inference.inferred_type;
    if (inference.returns.empty()) {
        return_type = checked_.types.builtin(BuiltinType::void_);
    }
    if (!is_valid(return_type)) {
        return_type = checked_.types.builtin(BuiltinType::void_);
    }
    validate_function_return_type(function, return_type);
    if (const auto found = checked_.functions.find(key); found != checked_.functions.end()) {
        found->second.return_type = return_type;
    }
    if (const auto global = global_values_.find(key); global != global_values_.end()) {
        global->second.type = return_type;
    }
}

void SemanticAnalyzer::validate_function_return_type(const syntax::ItemNode& function, const TypeHandle return_type) {
    if (checked_.types.is_array(return_type)) {
        report(function.range, "array type cannot be used as a function return type");
    }
    if (checked_.types.contains_array(return_type)) {
        report(function.range, "struct containing array cannot be returned by value");
    }
}

void SemanticAnalyzer::ensure_function_return_known(
    const FunctionSignature& signature,
    const base::SourceRange use_range
) {
    if (is_valid(signature.return_type) || signature.is_extern_c) {
        return;
    }
    const std::string key = signature.is_method
        ? method_key(signature.module, signature.method_owner_type, signature.name)
        : module_key(signature.module, signature.name);
    const FunctionBodyState state = function_body_states_.contains(key)
        ? function_body_states_.at(key)
        : FunctionBodyState::not_started;
    if (state == FunctionBodyState::analyzing) {
        report(use_range, "cannot infer recursive function return type without an explicit return type");
        return;
    }
    const auto item_found = function_definition_items_.find(key);
    if (item_found == function_definition_items_.end() ||
        !syntax::is_valid(item_found->second) ||
        item_found->second.value >= module_.items.size()) {
        report(use_range, "function return type cannot be inferred");
        return;
    }
    analyze_function_body(module_.items[item_found->second.value]);
}

TypeHandle SemanticAnalyzer::analyze_expr(const syntax::ExprId expr_id) {
    return analyze_expr(expr_id, invalid_type_handle);
}

TypeHandle SemanticAnalyzer::analyze_expr(const syntax::ExprId expr_id, const TypeHandle expected_type) {
    if (!syntax::is_valid(expr_id) || expr_id.value >= module_.exprs.size()) {
        return invalid_type_handle;
    }

    const syntax::ExprNode& expr = module_.exprs[expr_id.value];
    switch (expr.kind) {
    case syntax::ExprKind::integer_literal:
        return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::i32));
    case syntax::ExprKind::bool_literal:
        return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::bool_));
    case syntax::ExprKind::null_literal:
        return record_expr_type(expr_id, invalid_type_handle);
    case syntax::ExprKind::string_literal:
        return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::str));
    case syntax::ExprKind::c_string_literal:
        return record_expr_type(expr_id, checked_.types.pointer(PointerMutability::const_, checked_.types.builtin(BuiltinType::u8)));
    case syntax::ExprKind::byte_literal:
        return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::u8));
    case syntax::ExprKind::name: {
        const Symbol* symbol = find_symbol(expr.text, expr.range);
        if (symbol == nullptr) {
            return record_expr_type(expr_id, invalid_type_handle);
        }
        if (!symbol->c_name.empty() && expr_id.value < checked_.expr_c_names.size()) {
            checked_.expr_c_names[expr_id.value] = symbol->c_name;
        }
        return record_expr_type(expr_id, symbol->type);
    }
    case syntax::ExprKind::call: {
        if (!syntax::is_valid(expr.callee) ||
            (module_.exprs[expr.callee.value].kind != syntax::ExprKind::name &&
             module_.exprs[expr.callee.value].kind != syntax::ExprKind::field)) {
            report(expr.range, "callee must be a function name");
            return record_expr_type(expr_id, invalid_type_handle);
        }
        const bool generic_enum_constructor =
            module_.exprs[expr.callee.value].kind == syntax::ExprKind::field &&
            syntax::is_valid(module_.exprs[expr.callee.value].object) &&
            module_.exprs[expr.callee.value].object.value < module_.exprs.size() &&
            module_.exprs[module_.exprs[expr.callee.value].object.value].kind == syntax::ExprKind::name &&
            find_generic_enum_template_in_visible_modules(
                module_.exprs[module_.exprs[expr.callee.value].object.value].text,
                module_.exprs[expr.callee.value].range,
                false
            ) != nullptr;
        const syntax::ExprNode& callee = module_.exprs[expr.callee.value];
        const std::string name = callee.kind == syntax::ExprKind::field
            ? std::string(callee.field_name)
            : std::string(callee.text);
        const base::SourceRange callee_range = callee.range;
        if (const EnumCaseInfo* enum_case = find_enum_constructor(expr.callee, false); enum_case != nullptr) {
            if (!is_valid(enum_case->payload_type)) {
                report(expr.range, "enum case constructor requires a payload case: " + enum_case->name);
            }
            if (expr.args.size() != 1) {
                report(expr.range, "enum payload constructor requires exactly one argument: " + enum_case->name);
            }
            TypeHandle actual = invalid_type_handle;
            if (!expr.args.empty()) {
                actual = analyze_expr(expr.args.front());
                if (!can_assign(enum_case->payload_type, actual, expr.args.front())) {
                    report(module_.exprs[expr.args.front().value].range, "enum payload constructor argument type mismatch");
                }
                if (is_copy_forbidden_value(enum_case->payload_type)) {
                    report(module_.exprs[expr.args.front().value].range, "non-copyable array storage cannot be used as enum payload");
                }
            }
            if (expr.callee.value < checked_.expr_c_names.size()) {
                checked_.expr_c_names[expr.callee.value] = enum_case->c_name;
            }
            return record_expr_type(expr_id, enum_case->type);
        }
        if (generic_enum_constructor) {
            std::vector<TypeHandle> arg_types;
            arg_types.reserve(expr.args.size());
            for (syntax::ExprId arg : expr.args) {
                arg_types.push_back(analyze_expr(arg));
            }
            const EnumCaseInfo* enum_case = instantiate_generic_enum_constructor(expr.callee, arg_types, expected_type, true);
            if (enum_case == nullptr) {
                return record_expr_type(expr_id, invalid_type_handle);
            }
            if (!is_valid(enum_case->payload_type)) {
                report(expr.range, "enum case constructor requires a payload case: " + enum_case->name);
            }
            if (expr.args.size() != 1) {
                report(expr.range, "enum payload constructor requires exactly one argument: " + enum_case->name);
            }
            if (!arg_types.empty()) {
                if (!can_assign(enum_case->payload_type, arg_types.front(), expr.args.front())) {
                    report(module_.exprs[expr.args.front().value].range, "enum payload constructor argument type mismatch");
                }
                if (is_copy_forbidden_value(enum_case->payload_type)) {
                    report(module_.exprs[expr.args.front().value].range, "non-copyable array storage cannot be used as enum payload");
                }
            }
            if (expr.callee.value < checked_.expr_c_names.size()) {
                checked_.expr_c_names[expr.callee.value] = enum_case->c_name;
            }
            return record_expr_type(expr_id, enum_case->type);
        }
        if (callee.kind == syntax::ExprKind::field) {
            const FunctionSignature* signature = nullptr;
            TypeHandle receiver_type = invalid_type_handle;
            bool has_receiver = false;
            bool receiver_valid = true;
            if (syntax::is_valid(callee.object) &&
                callee.object.value < module_.exprs.size() &&
                module_.exprs[callee.object.value].kind == syntax::ExprKind::name) {
                const syntax::ExprNode& object = module_.exprs[callee.object.value];
                const TypeHandle associated_owner =
                    find_type_in_visible_modules(object.text, object.range, false, false);
                if (is_valid(associated_owner)) {
                    signature = find_method_in_visible_modules(associated_owner, callee.field_name, callee.range, false);
                    if (signature == nullptr) {
                        return record_expr_type(expr_id, invalid_type_handle);
                    }
                    if (signature->has_self_param) {
                        report(callee.range, "method requires a receiver: " + checked_.types.display_name(associated_owner) + "." + name);
                        receiver_valid = false;
                    }
                }
            }
            if (signature == nullptr) {
                has_receiver = true;
                receiver_type = analyze_expr(callee.object);
                TypeHandle owner_type = receiver_type;
                if (checked_.types.is_pointer(owner_type)) {
                    owner_type = checked_.types.get(owner_type).pointee;
                }
                signature = find_method_in_visible_modules(owner_type, callee.field_name, callee.range, true);
                if (signature == nullptr) {
                    return record_expr_type(expr_id, invalid_type_handle);
                }
                receiver_valid = method_receiver_matches(*signature, receiver_type, callee.object);
            }
            ensure_function_return_known(*signature, callee.range);
            if (expr.callee.value < checked_.expr_c_names.size()) {
                checked_.expr_c_names[expr.callee.value] = signature->c_name;
            }
            const base::usize receiver_count = has_receiver ? 1 : 0;
            const base::usize expected_count =
                signature->param_types.size() >= receiver_count
                    ? signature->param_types.size() - receiver_count
                    : 0;
            if (!receiver_valid || signature->param_types.size() < receiver_count) {
                return record_expr_type(expr_id, invalid_type_handle);
            }
            if (expected_count != expr.args.size()) {
                report(expr.range, "argument count mismatch in call to " + name);
            }
            const base::usize count = expr.args.size() < expected_count ? expr.args.size() : expected_count;
            for (base::usize i = 0; i < count; ++i) {
                const TypeHandle expected = signature->param_types[i + receiver_count];
                const TypeHandle actual = analyze_expr(expr.args[i]);
                if (!can_assign(expected, actual, expr.args[i])) {
                    report(module_.exprs[expr.args[i].value].range, "argument type mismatch in call to " + name);
                }
                if (is_copy_forbidden_value(expected)) {
                    report(module_.exprs[expr.args[i].value].range, "non-copyable array storage cannot be passed by value");
                }
            }
            return record_expr_type(expr_id, signature->return_type);
        }
        const FunctionSignature* signature = find_function_in_visible_modules(name, callee_range);
        if (signature == nullptr) {
            return record_expr_type(expr_id, invalid_type_handle);
        }
        ensure_function_return_known(*signature, module_.exprs[expr.callee.value].range);
        if (expr.callee.value < checked_.expr_c_names.size()) {
            checked_.expr_c_names[expr.callee.value] = signature->c_name;
        }
        if (signature->param_types.size() != expr.args.size()) {
            report(expr.range, "argument count mismatch in call to " + name);
        }
        const base::usize count = expr.args.size() < signature->param_types.size() ? expr.args.size() : signature->param_types.size();
        for (base::usize i = 0; i < count; ++i) {
            const TypeHandle actual = analyze_expr(expr.args[i]);
            if (!can_assign(signature->param_types[i], actual, expr.args[i])) {
                report(module_.exprs[expr.args[i].value].range, "argument type mismatch in call to " + name);
            }
            if (is_copy_forbidden_value(signature->param_types[i])) {
                report(module_.exprs[expr.args[i].value].range, "non-copyable array storage cannot be passed by value");
            }
        }
        return record_expr_type(expr_id, signature->return_type);
    }
    case syntax::ExprKind::try_expr:
        return analyze_try_expr(expr_id, expr);
    case syntax::ExprKind::if_expr:
        return analyze_if_expr(expr_id, expr);
    case syntax::ExprKind::block_expr:
        return analyze_block_expr(expr_id, expr);
    case syntax::ExprKind::match_expr:
        return analyze_match_expr(expr_id, expr);
    case syntax::ExprKind::unary: {
        const TypeHandle operand = analyze_expr(expr.unary_operand);
        if (expr.unary_op == syntax::UnaryOp::logical_not && !checked_.types.is_bool(operand)) {
            report(expr.range, "logical not requires bool operand");
        }
        if ((expr.unary_op == syntax::UnaryOp::numeric_negate || expr.unary_op == syntax::UnaryOp::bitwise_not) &&
            !checked_.types.is_integer(operand) && !checked_.types.is_float(operand)) {
            report(expr.range, "numeric unary operator requires numeric operand");
        }
        if (expr.unary_op == syntax::UnaryOp::dereference) {
            if (!checked_.types.is_pointer(operand)) {
                report(expr.range, "dereference requires pointer operand");
                return record_expr_type(expr_id, invalid_type_handle);
            }
            return record_expr_type(expr_id, checked_.types.get(operand).pointee);
        }
        if (expr.unary_op == syntax::UnaryOp::address_of) {
            if (!is_place_expr(expr.unary_operand)) {
                report(expr.range, "address-of requires a place expression");
            }
            const PointerMutability mutability = is_writable_place(expr.unary_operand)
                ? PointerMutability::mut
                : PointerMutability::const_;
            return record_expr_type(expr_id, checked_.types.pointer(mutability, operand));
        }
        return record_expr_type(expr_id, operand);
    }
    case syntax::ExprKind::binary: {
        const TypeHandle lhs = analyze_expr(expr.binary_lhs);
        const TypeHandle rhs = analyze_expr(expr.binary_rhs);
        const bool is_equality =
            expr.binary_op == syntax::BinaryOp::equal ||
            expr.binary_op == syntax::BinaryOp::not_equal;
        const bool is_null_pointer_comparison =
            is_equality &&
            ((checked_.types.is_pointer(lhs) && is_null_literal(expr.binary_rhs)) ||
             (checked_.types.is_pointer(rhs) && is_null_literal(expr.binary_lhs)));
        if (!checked_.types.same(lhs, rhs) && !is_null_pointer_comparison) {
            report(expr.range, "binary operands must have the same type");
        }
        switch (expr.binary_op) {
        case syntax::BinaryOp::less:
        case syntax::BinaryOp::less_equal:
        case syntax::BinaryOp::greater:
        case syntax::BinaryOp::greater_equal:
            if (!checked_.types.is_integer(lhs) && !checked_.types.is_float(lhs)) {
                report(expr.range, "comparison operator requires numeric operands");
            }
            return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::bool_));
        case syntax::BinaryOp::equal:
        case syntax::BinaryOp::not_equal: {
            const bool scalar =
                checked_.types.is_bool(lhs) ||
                checked_.types.is_integer(lhs) ||
                checked_.types.is_float(lhs) ||
                checked_.types.is_pointer(lhs) ||
                (is_valid(lhs) &&
                 checked_.types.get(lhs).kind == TypeKind::enum_ &&
                 !is_valid(checked_.types.get(lhs).enum_payload_storage));
            if (!scalar && !is_null_pointer_comparison) {
                report(expr.range, "equality operator requires scalar operands");
            }
            return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::bool_));
        }
        case syntax::BinaryOp::logical_and:
        case syntax::BinaryOp::logical_or:
            if (!checked_.types.is_bool(lhs) || !checked_.types.is_bool(rhs)) {
                report(expr.range, "logical operator requires bool operands");
            }
            return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::bool_));
        case syntax::BinaryOp::bit_and:
        case syntax::BinaryOp::bit_xor:
        case syntax::BinaryOp::bit_or:
        case syntax::BinaryOp::shl:
        case syntax::BinaryOp::shr:
        case syntax::BinaryOp::mod:
            if (!checked_.types.is_integer(lhs)) {
                report(expr.range, "integer operator requires integer operands");
            }
            return record_expr_type(expr_id, lhs);
        default:
            if (!checked_.types.is_integer(lhs) && !checked_.types.is_float(lhs)) {
                report(expr.range, "binary operator requires numeric operands");
            }
            return record_expr_type(expr_id, lhs);
        }
    }
    case syntax::ExprKind::field: {
        if (syntax::is_valid(expr.object) &&
            expr.object.value < module_.exprs.size() &&
            module_.exprs[expr.object.value].kind == syntax::ExprKind::name) {
            const syntax::ExprNode& object = module_.exprs[expr.object.value];
            if (find_generic_enum_template_in_visible_modules(object.text, expr.range, false) != nullptr) {
                const EnumCaseInfo* enum_case = nullptr;
                if (const GenericEnumInstanceInfo* expected_instance = generic_enum_instance(expected_type);
                    expected_instance != nullptr && expected_instance->name == object.text) {
                    enum_case = find_enum_case_by_type_and_case(expected_type, expr.field_name);
                }
                if (enum_case == nullptr) {
                    enum_case = instantiate_generic_enum_constructor(expr_id, {}, expected_type, true);
                }
                if (enum_case == nullptr) {
                    return record_expr_type(expr_id, invalid_type_handle);
                }
                if (is_valid(enum_case->payload_type)) {
                    report(expr.range, "enum payload constructor requires a call: " + enum_case->name);
                    return record_expr_type(expr_id, invalid_type_handle);
                }
                if (expr_id.value < checked_.expr_c_names.size()) {
                    checked_.expr_c_names[expr_id.value] = enum_case->c_name;
                }
                return record_expr_type(expr_id, enum_case->type);
            }
            if (const EnumCaseInfo* enum_case = find_enum_case_by_scoped_name(object.text, expr.field_name, expr.range, false);
                enum_case != nullptr) {
                if (is_valid(enum_case->payload_type)) {
                    report(expr.range, "enum payload constructor requires a call: " + enum_case->name);
                    return record_expr_type(expr_id, invalid_type_handle);
                }
                if (expr_id.value < checked_.expr_c_names.size()) {
                    checked_.expr_c_names[expr_id.value] = enum_case->c_name;
                }
                return record_expr_type(expr_id, enum_case->type);
            }
        }
        TypeHandle object = analyze_expr(expr.object);
        if (checked_.types.is_pointer(object)) {
            object = checked_.types.get(object).pointee;
        }
        const StructInfo* info = find_struct(object);
        if (info == nullptr || info->is_opaque) {
            report(expr.range, "field access requires a non-opaque struct value");
            return record_expr_type(expr_id, invalid_type_handle);
        }
        for (const StructFieldInfo& field : info->fields) {
            if (field.name == expr.field_name) {
                if (!can_access(info->module, field.visibility)) {
                    report(expr.range, "field is private: " + std::string(expr.field_name));
                    return record_expr_type(expr_id, invalid_type_handle);
                }
                return record_expr_type(expr_id, field.type);
            }
        }
        report(expr.range, "unknown field: " + std::string(expr.field_name));
        return record_expr_type(expr_id, invalid_type_handle);
    }
    case syntax::ExprKind::index: {
        const TypeHandle object = analyze_expr(expr.object);
        const TypeHandle index = analyze_expr(expr.index);
        if (!checked_.types.is_integer(index)) {
            report(module_.exprs[expr.index.value].range, "array index must be an integer");
        }
        if (checked_.types.is_array(object)) {
            return record_expr_type(expr_id, checked_.types.get(object).array_element);
        }
        if (checked_.types.is_pointer(object)) {
            return record_expr_type(expr_id, checked_.types.get(object).pointee);
        }
        report(expr.range, "indexing requires array or pointer value");
        return record_expr_type(expr_id, invalid_type_handle);
    }
    case syntax::ExprKind::struct_literal: {
        TypeHandle struct_type = invalid_type_handle;
        if (!expr.struct_type_args.empty()) {
            if (const GenericStructTemplateInfo* template_info =
                    find_generic_struct_template_in_visible_modules(expr.struct_name, expr.range);
                template_info != nullptr) {
                struct_type = instantiate_generic_struct_from_syntax(
                    *template_info,
                    expr.struct_type_args,
                    expr.range,
                    false
                );
            }
        } else {
            if (const GenericStructTemplateInfo* template_info =
                    find_generic_struct_template_in_visible_modules(expr.struct_name, expr.range, false);
                template_info != nullptr) {
                report(expr.range, "generic struct literal requires explicit type arguments: " + template_info->name);
                return record_expr_type(expr_id, invalid_type_handle);
            }
            struct_type = find_type_in_visible_modules(expr.struct_name, expr.range, false);
        }
        if (!is_valid(struct_type)) {
            return record_expr_type(expr_id, invalid_type_handle);
        }
        const StructInfo* info = find_struct(struct_type);
        if (info == nullptr || info->is_opaque) {
            report(expr.range, "struct literal requires a non-opaque struct type");
            return record_expr_type(expr_id, invalid_type_handle);
        }
        for (const syntax::FieldInit& init : expr.field_inits) {
            const StructFieldInfo* field_info = nullptr;
            for (const StructFieldInfo& field : info->fields) {
                if (field.name == init.name) {
                    field_info = &field;
                    break;
                }
            }
            if (field_info == nullptr) {
                report(init.range, "unknown field in struct literal: " + std::string(init.name));
                continue;
            }
            if (!can_access(info->module, field_info->visibility)) {
                report(init.range, "field is private: " + std::string(init.name));
                continue;
            }
            const TypeHandle actual = analyze_expr(init.value);
            if (!can_assign(field_info->type, actual, init.value)) {
                report(init.range, "struct literal field type mismatch");
            }
        }
        return record_expr_type(expr_id, struct_type);
    }
    case syntax::ExprKind::cast:
    case syntax::ExprKind::ptr_cast:
    case syntax::ExprKind::bit_cast: {
        const TypeHandle source = analyze_expr(expr.cast_expr);
        const TypeHandle target = resolve_type(expr.cast_type);
        if (!is_valid_cast(expr.kind, target, source)) {
            report(expr.range, "invalid explicit conversion");
        }
        return record_expr_type(expr_id, target);
    }
    case syntax::ExprKind::size_of:
    case syntax::ExprKind::align_of: {
        const TypeHandle queried = resolve_type(expr.cast_type);
        if (is_valid(queried) && checked_.types.get(queried).kind == TypeKind::opaque_struct) {
            report(expr.range, "opaque struct cannot be queried by size_of or align_of directly");
        }
        return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::usize));
    }
    case syntax::ExprKind::ptr_addr: {
        const TypeHandle value = analyze_expr(expr.cast_expr);
        if (!checked_.types.is_pointer(value)) {
            report(expr.range, "ptr_addr requires a pointer value");
        }
        return record_expr_type(expr_id, checked_.types.builtin(BuiltinType::usize));
    }
    case syntax::ExprKind::ptr_from_addr: {
        const TypeHandle target = resolve_type(expr.cast_type);
        const TypeHandle address = analyze_expr(expr.cast_expr);
        if (!checked_.types.is_pointer(target)) {
            report(expr.range, "ptr_from_addr target type must be a pointer");
        }
        if (!checked_.types.is_integer(address)) {
            report(module_.exprs[expr.cast_expr.value].range, "ptr_from_addr address must be an integer");
        }
        return record_expr_type(expr_id, target);
    }
    case syntax::ExprKind::invalid:
        return record_expr_type(expr_id, invalid_type_handle);
    }
    return record_expr_type(expr_id, invalid_type_handle);
}

TypeHandle SemanticAnalyzer::analyze_try_expr(const syntax::ExprId expr_id, const syntax::ExprNode& expr) {
    if (in_const_initializer_) {
        report(expr.range, "try expression cannot be used in const initializer");
    }

    const TypeHandle source_type = analyze_expr(expr.unary_operand);
    const GenericEnumInstanceInfo* const source_instance = generic_enum_instance(source_type);
    if (source_instance == nullptr) {
        report(expr.range, "try expression requires Result<T, E> or Option<T>");
        return record_expr_type(expr_id, invalid_type_handle);
    }

    if (source_instance->name == "Result" && source_instance->args.size() == 2) {
        const EnumCaseInfo* const ok_case = find_enum_case_by_type_and_case(source_type, "ok");
        const EnumCaseInfo* const err_case = find_enum_case_by_type_and_case(source_type, "err");
        if (ok_case == nullptr || err_case == nullptr || !is_valid(ok_case->payload_type) || !is_valid(err_case->payload_type)) {
            report(expr.range, "try expression Result type must define ok(T) and err(E) cases");
            return record_expr_type(expr_id, invalid_type_handle);
        }

        const GenericEnumInstanceInfo* const return_instance = generic_enum_instance(current_function_return_type_);
        if (return_instance == nullptr ||
            return_instance->name != "Result" ||
            return_instance->module.value != source_instance->module.value ||
            return_instance->args.size() != 2) {
            report(expr.range, "try expression on Result<T, E> requires enclosing function to return Result<U, E>");
            return record_expr_type(expr_id, ok_case->payload_type);
        }
        if (!checked_.types.same(return_instance->args[1], source_instance->args[1])) {
            report(expr.range, "try expression Result error type must match enclosing Result error type");
        }
        const EnumCaseInfo* const return_err_case = find_enum_case_by_type_and_case(current_function_return_type_, "err");
        if (return_err_case == nullptr || !is_valid(return_err_case->payload_type)) {
            report(expr.range, "enclosing Result return type must define err(E)");
        } else if (!checked_.types.same(return_err_case->payload_type, err_case->payload_type)) {
            report(expr.range, "try expression Result error payload type must match enclosing Result error payload type");
        }
        return record_expr_type(expr_id, ok_case->payload_type);
    }

    if (source_instance->name == "Option" && source_instance->args.size() == 1) {
        const EnumCaseInfo* const some_case = find_enum_case_by_type_and_case(source_type, "some");
        const EnumCaseInfo* const none_case = find_enum_case_by_type_and_case(source_type, "none");
        if (some_case == nullptr || none_case == nullptr || !is_valid(some_case->payload_type) || is_valid(none_case->payload_type)) {
            report(expr.range, "try expression Option type must define some(T) and none cases");
            return record_expr_type(expr_id, invalid_type_handle);
        }

        const GenericEnumInstanceInfo* const return_instance = generic_enum_instance(current_function_return_type_);
        if (return_instance == nullptr ||
            return_instance->name != "Option" ||
            return_instance->module.value != source_instance->module.value ||
            return_instance->args.size() != 1) {
            report(expr.range, "try expression on Option<T> requires enclosing function to return Option<U>");
            return record_expr_type(expr_id, some_case->payload_type);
        }
        const EnumCaseInfo* const return_none_case = find_enum_case_by_type_and_case(current_function_return_type_, "none");
        if (return_none_case == nullptr || is_valid(return_none_case->payload_type)) {
            report(expr.range, "enclosing Option return type must define none");
        }
        return record_expr_type(expr_id, some_case->payload_type);
    }

    report(expr.range, "try expression requires Result<T, E> or Option<T>");
    return record_expr_type(expr_id, invalid_type_handle);
}

TypeHandle SemanticAnalyzer::analyze_if_expr(const syntax::ExprId expr_id, const syntax::ExprNode& expr) {
    if (in_const_initializer_) {
        report(expr.range, "if expression cannot be used in const initializer");
    }
    const TypeHandle condition = analyze_expr(expr.condition);
    if (!checked_.types.is_bool(condition)) {
        report(module_.exprs[expr.condition.value].range, "if expression condition must be bool");
    }

    const TypeHandle then_type = analyze_expr(expr.then_expr);
    const TypeHandle else_type = analyze_expr(expr.else_expr);
    if (!checked_.types.same(then_type, else_type)) {
        report(expr.range, "if expression branches must have the same type");
        return record_expr_type(expr_id, invalid_type_handle);
    }
    if (is_valid(then_type) && checked_.types.is_void(then_type)) {
        report(expr.range, "if expression branches cannot be void");
        return record_expr_type(expr_id, invalid_type_handle);
    }
    return record_expr_type(expr_id, then_type);
}

TypeHandle SemanticAnalyzer::analyze_block_expr(const syntax::ExprId expr_id, const syntax::ExprNode& expr) {
    if (in_const_initializer_) {
        report(expr.range, "block expression cannot be used in const initializer");
    }
    if (!syntax::is_valid(expr.block_result)) {
        report(expr.range, "block expression requires a final expression");
        return record_expr_type(expr_id, invalid_type_handle);
    }

    symbols_.push_scope();
    if (syntax::is_valid(expr.block) && expr.block.value < module_.stmts.size()) {
        const syntax::StmtNode& block = module_.stmts[expr.block.value];
        for (syntax::StmtId child : block.statements) {
            analyze_stmt(child, checked_.types.builtin(BuiltinType::void_), nullptr);
        }
    }
    const TypeHandle result = analyze_expr(expr.block_result);
    symbols_.pop_scope();

    if (!is_valid(result)) {
        return record_expr_type(expr_id, invalid_type_handle);
    }
    if (checked_.types.is_void(result)) {
        report(expr.range, "block expression result cannot be void");
        return record_expr_type(expr_id, invalid_type_handle);
    }
    return record_expr_type(expr_id, result);
}

TypeHandle SemanticAnalyzer::resolve_type(const syntax::TypeId type_id) {
    return resolve_type(type_id, false);
}

TypeHandle SemanticAnalyzer::resolve_type(const syntax::TypeId type_id, const bool opaque_allowed_as_pointee) {
    return resolve_type_with_substitution(type_id, nullptr, opaque_allowed_as_pointee);
}

TypeHandle SemanticAnalyzer::resolve_type_with_substitution(
    const syntax::TypeId type_id,
    const GenericTypeSubstitution* const substitution,
    const bool opaque_allowed_as_pointee
) {
    if (!syntax::is_valid(type_id) || type_id.value >= module_.types.size()) {
        return invalid_type_handle;
    }

    if (substitution == nullptr &&
        type_id.value < checked_.syntax_type_handles.size() &&
        is_valid(checked_.syntax_type_handles[type_id.value])) {
        const TypeHandle cached = checked_.syntax_type_handles[type_id.value];
        if (checked_.types.get(cached).kind == TypeKind::opaque_struct && !opaque_allowed_as_pointee) {
            report(module_.types[type_id.value].range, "opaque struct can only be used as a pointer target");
        }
        return cached;
    }

    const syntax::TypeNode& type = module_.types[type_id.value];
    TypeHandle resolved = invalid_type_handle;
    switch (type.kind) {
    case syntax::TypeKind::primitive:
        resolved = checked_.types.builtin(map_builtin(type.primitive));
        break;
    case syntax::TypeKind::pointer:
        resolved = checked_.types.pointer(
            map_mutability(type.pointer_mutability),
            resolve_type_with_substitution(type.pointee, substitution, true)
        );
        break;
    case syntax::TypeKind::array:
        resolved = checked_.types.array(type.array_count, resolve_type_with_substitution(type.array_element, substitution, false));
        break;
    case syntax::TypeKind::named: {
        if (substitution != nullptr && type.type_args.empty()) {
            if (const auto found = substitution->types.find(std::string(type.name)); found != substitution->types.end()) {
                resolved = found->second;
                break;
            }
        }
        if (!type.type_args.empty()) {
            if (const GenericStructTemplateInfo* info =
                    find_generic_struct_template_in_visible_modules(type.name, type.range, false);
                info != nullptr) {
                resolved = instantiate_generic_struct_from_syntax(*info, type.type_args, type.range, opaque_allowed_as_pointee);
                break;
            }
            if (const GenericEnumTemplateInfo* info = find_generic_enum_template_in_visible_modules(type.name, type.range);
                info != nullptr) {
                resolved = instantiate_generic_enum_from_syntax(*info, type.type_args, type.range, opaque_allowed_as_pointee);
                break;
            }
            report(type.range, "type arguments require a generic type: " + std::string(type.name));
            break;
        }
        if (const GenericStructTemplateInfo* info = find_generic_struct_template_in_visible_modules(type.name, type.range, false);
            info != nullptr) {
            report(type.range, "generic struct type requires type arguments: " + info->name);
            break;
        }
        if (const GenericEnumTemplateInfo* info = find_generic_enum_template_in_visible_modules(type.name, type.range, false);
            info != nullptr) {
            report(type.range, "generic enum type requires type arguments: " + info->name);
            break;
        }
        resolved = find_type_in_visible_modules(type.name, type.range, opaque_allowed_as_pointee);
        if (!is_valid(resolved)) {
            break;
        }
        if (checked_.types.get(resolved).kind == TypeKind::opaque_struct && !opaque_allowed_as_pointee) {
            report(type.range, "opaque struct can only be used as a pointer target");
        }
        break;
    }
    }
    if (substitution == nullptr && type_id.value < checked_.syntax_type_handles.size()) {
        checked_.syntax_type_handles[type_id.value] = resolved;
    }
    return resolved;
}

TypeHandle SemanticAnalyzer::resolve_type_alias(const TypeAliasInfo& alias, const bool opaque_allowed_as_pointee) {
    const std::string key = module_key(alias.module, alias.name);
    if (const auto found = resolved_type_aliases_.find(key); found != resolved_type_aliases_.end()) {
        return found->second;
    }
    if (std::find(resolving_type_aliases_.begin(), resolving_type_aliases_.end(), key) != resolving_type_aliases_.end()) {
        report(alias.range, "cyclic type alias: " + alias.name);
        resolved_type_aliases_[key] = invalid_type_handle;
        return invalid_type_handle;
    }
    resolving_type_aliases_.push_back(key);
    const syntax::ModuleId previous_module = current_module_;
    current_module_ = alias.module;
    const TypeHandle resolved = resolve_type(alias.target, opaque_allowed_as_pointee);
    current_module_ = previous_module;
    resolving_type_aliases_.pop_back();
    resolved_type_aliases_[key] = resolved;
    return resolved;
}

bool SemanticAnalyzer::can_assign(const TypeHandle dst, const TypeHandle src, const syntax::ExprId value) const noexcept {
    if (!is_valid(dst) || !is_valid(src)) {
        return is_valid(dst) && is_null_literal(value) && checked_.types.is_pointer(dst);
    }
    if (checked_.types.is_integer(dst) && checked_.types.is_integer(src) && is_integer_literal(value)) {
        return true;
    }
    return checked_.types.same(dst, src);
}

bool SemanticAnalyzer::is_valid_storage_type(const TypeHandle type) const noexcept {
    return is_valid(type) && !checked_.types.is_void(type) && checked_.types.get(type).kind != TypeKind::opaque_struct;
}

bool SemanticAnalyzer::is_valid_cast(const syntax::ExprKind kind, const TypeHandle dst, const TypeHandle src) const noexcept {
    if (!is_valid(dst) || !is_valid(src)) {
        return false;
    }

    if (kind == syntax::ExprKind::cast) {
        return (checked_.types.is_integer(dst) || checked_.types.is_float(dst) || checked_.types.is_bool(dst)) &&
               (checked_.types.is_integer(src) || checked_.types.is_float(src) || checked_.types.is_bool(src));
    }
    if (kind == syntax::ExprKind::ptr_cast) {
        return checked_.types.is_pointer(dst) && checked_.types.is_pointer(src);
    }
    if (kind == syntax::ExprKind::bit_cast) {
        return checked_.types.is_copyable(dst) && checked_.types.is_copyable(src) && abi_size(dst) == abi_size(src);
    }
    return false;
}

base::u64 SemanticAnalyzer::abi_size(const TypeHandle type) const noexcept {
    if (!is_valid(type)) {
        return 0;
    }
    const TypeInfo& info = checked_.types.get(type);
    switch (info.kind) {
    case TypeKind::builtin:
        switch (info.builtin) {
        case BuiltinType::void_: return 0;
        case BuiltinType::bool_: return sizeof(bool);
        case BuiltinType::i8:
        case BuiltinType::u8: return sizeof(std::uint8_t);
        case BuiltinType::i16:
        case BuiltinType::u16: return sizeof(std::uint16_t);
        case BuiltinType::i32:
        case BuiltinType::u32: return sizeof(std::uint32_t);
        case BuiltinType::i64:
        case BuiltinType::u64: return sizeof(std::uint64_t);
        case BuiltinType::isize: return sizeof(std::ptrdiff_t);
        case BuiltinType::usize: return sizeof(std::size_t);
        case BuiltinType::f32: return sizeof(float);
        case BuiltinType::f64: return sizeof(double);
        case BuiltinType::str: return sizeof(void*) + sizeof(std::size_t);
        }
        return 0;
    case TypeKind::pointer:
        return sizeof(void*);
    case TypeKind::array:
        return info.array_count * abi_size(info.array_element);
    case TypeKind::enum_: {
        if (!is_valid(info.enum_payload_storage)) {
            return abi_size(info.enum_underlying);
        }
        const base::u64 tag_align = abi_align(info.enum_underlying);
        const base::u64 storage_align = info.enum_payload_align;
        const base::u64 max_align = std::max(tag_align, storage_align);
        const base::u64 storage_offset = align_forward(abi_size(info.enum_underlying), storage_align);
        return align_forward(storage_offset + abi_size(info.enum_payload_storage), max_align);
    }
    case TypeKind::struct_: {
        const StructInfo* struct_info = find_struct(type);
        if (struct_info == nullptr) {
            return 0;
        }
        base::u64 offset = 0;
        base::u64 max_align = 1;
        for (const StructFieldInfo& field : struct_info->fields) {
            const base::u64 field_align = abi_align(field.type);
            max_align = std::max(max_align, field_align);
            offset = align_forward(offset, field_align);
            offset += abi_size(field.type);
        }
        return align_forward(offset, max_align);
    }
    case TypeKind::opaque_struct:
        return 0;
    }
    return 0;
}

base::u64 SemanticAnalyzer::abi_align(const TypeHandle type) const noexcept {
    if (!is_valid(type)) {
        return 1;
    }
    const TypeInfo& info = checked_.types.get(type);
    switch (info.kind) {
    case TypeKind::builtin:
        switch (info.builtin) {
        case BuiltinType::void_: return 1;
        case BuiltinType::bool_: return alignof(bool);
        case BuiltinType::i8:
        case BuiltinType::u8: return alignof(std::uint8_t);
        case BuiltinType::i16:
        case BuiltinType::u16: return alignof(std::uint16_t);
        case BuiltinType::i32:
        case BuiltinType::u32: return alignof(std::uint32_t);
        case BuiltinType::i64:
        case BuiltinType::u64: return alignof(std::uint64_t);
        case BuiltinType::isize: return alignof(std::ptrdiff_t);
        case BuiltinType::usize: return alignof(std::size_t);
        case BuiltinType::f32: return alignof(float);
        case BuiltinType::f64: return alignof(double);
        case BuiltinType::str: return alignof(void*);
        }
        return 1;
    case TypeKind::pointer:
        return alignof(void*);
    case TypeKind::array:
        return abi_align(info.array_element);
    case TypeKind::enum_:
        if (!is_valid(info.enum_payload_storage)) {
            return abi_align(info.enum_underlying);
        }
        return std::max(abi_align(info.enum_underlying), info.enum_payload_align);
    case TypeKind::struct_: {
        const StructInfo* struct_info = find_struct(type);
        if (struct_info == nullptr) {
            return 1;
        }
        base::u64 max_align = 1;
        for (const StructFieldInfo& field : struct_info->fields) {
            max_align = std::max(max_align, abi_align(field.type));
        }
        return max_align;
    }
    case TypeKind::opaque_struct:
        return 1;
    }
    return 1;
}

bool SemanticAnalyzer::is_integer_literal(const syntax::ExprId expr_id) const noexcept {
    return syntax::is_valid(expr_id) &&
           expr_id.value < module_.exprs.size() &&
           module_.exprs[expr_id.value].kind == syntax::ExprKind::integer_literal;
}

bool SemanticAnalyzer::is_null_literal(const syntax::ExprId expr_id) const noexcept {
    return syntax::is_valid(expr_id) &&
           expr_id.value < module_.exprs.size() &&
           module_.exprs[expr_id.value].kind == syntax::ExprKind::null_literal;
}

bool SemanticAnalyzer::is_place_expr(const syntax::ExprId expr_id) {
    if (!syntax::is_valid(expr_id) || expr_id.value >= module_.exprs.size()) {
        return false;
    }
    const syntax::ExprNode& expr = module_.exprs[expr_id.value];
    switch (expr.kind) {
    case syntax::ExprKind::name:
        return find_symbol(expr.text, expr.range) != nullptr;
    case syntax::ExprKind::field:
    case syntax::ExprKind::index:
        return is_place_expr(expr.object);
    case syntax::ExprKind::unary:
        return expr.unary_op == syntax::UnaryOp::dereference;
    default:
        return false;
    }
}

bool SemanticAnalyzer::is_writable_place(const syntax::ExprId expr_id) {
    if (!syntax::is_valid(expr_id) || expr_id.value >= module_.exprs.size()) {
        return false;
    }
    const syntax::ExprNode& expr = module_.exprs[expr_id.value];
    switch (expr.kind) {
    case syntax::ExprKind::name: {
        const Symbol* symbol = find_symbol(expr.text, expr.range);
        return symbol != nullptr && symbol->is_mutable;
    }
    case syntax::ExprKind::field: {
        const TypeHandle object = analyze_expr(expr.object);
        if (checked_.types.is_pointer(object)) {
            return checked_.types.get(object).pointer_mutability == PointerMutability::mut;
        }
        return is_writable_place(expr.object);
    }
    case syntax::ExprKind::index: {
        const TypeHandle object = analyze_expr(expr.object);
        if (checked_.types.is_pointer(object)) {
            return checked_.types.get(object).pointer_mutability == PointerMutability::mut;
        }
        return is_writable_place(expr.object);
    }
    case syntax::ExprKind::unary: {
        if (expr.unary_op != syntax::UnaryOp::dereference) {
            return false;
        }
        const TypeHandle pointer = analyze_expr(expr.unary_operand);
        return checked_.types.is_pointer(pointer) &&
               checked_.types.get(pointer).pointer_mutability == PointerMutability::mut;
    }
    default:
        return false;
    }
}

bool SemanticAnalyzer::is_copy_forbidden_value(const TypeHandle type) const noexcept {
    return is_valid(type) && (!checked_.types.is_copyable(type) || checked_.types.contains_array(type));
}

const StructInfo* SemanticAnalyzer::find_struct(const TypeHandle type) const noexcept {
    if (!is_valid(type)) {
        return nullptr;
    }
    for (const auto& entry : checked_.structs) {
        if (checked_.types.same(entry.second.type, type)) {
            return &entry.second;
        }
    }
    return nullptr;
}

syntax::ModuleId SemanticAnalyzer::item_module(const syntax::ItemNode& item) const noexcept {
    const auto* const begin = module_.items.data();
    const auto* const end = begin + module_.items.size();
    if (&item < begin || &item >= end) {
        return syntax::invalid_module_id;
    }
    const base::usize index = static_cast<base::usize>(&item - begin);
    if (index >= module_.item_modules.size()) {
        return syntax::invalid_module_id;
    }
    return module_.item_modules[index];
}

std::vector<syntax::ModuleId> SemanticAnalyzer::visible_modules(const syntax::ModuleId module) const {
    std::vector<syntax::ModuleId> result;
    if (!syntax::is_valid(module)) {
        return result;
    }
    result.push_back(module);
    if (module.value >= module_.modules.size()) {
        return result;
    }
    std::unordered_set<base::u32> seen;
    seen.insert(module.value);
    for (const syntax::ResolvedImport& import : module_.modules[module.value].imports) {
        if (!syntax::is_valid(import.module)) {
            continue;
        }
        if (seen.insert(import.module.value).second) {
            result.push_back(import.module);
        }
        append_public_reexports(import.module, result, seen);
    }
    return result;
}

void SemanticAnalyzer::append_public_reexports(
    const syntax::ModuleId module,
    std::vector<syntax::ModuleId>& result,
    std::unordered_set<base::u32>& seen
) const {
    if (!syntax::is_valid(module) || module.value >= module_.modules.size()) {
        return;
    }
    for (const syntax::ResolvedImport& import : module_.modules[module.value].imports) {
        if (import.visibility != syntax::Visibility::public_ || !syntax::is_valid(import.module)) {
            continue;
        }
        if (seen.insert(import.module.value).second) {
            result.push_back(import.module);
            append_public_reexports(import.module, result, seen);
        }
    }
}

std::string SemanticAnalyzer::module_name(const syntax::ModuleId module) const {
    if (!syntax::is_valid(module) || module.value >= module_.modules.size()) {
        return "<unknown>";
    }
    return syntax::module_path_to_string(module_.modules[module.value].path);
}

std::string SemanticAnalyzer::qualified_name(const syntax::ModuleId module, const std::string_view name) const {
    const std::string module_text = module_name(module);
    if (module_text.empty() || module_text == "<unknown>") {
        return std::string(name);
    }
    return module_text + "." + std::string(name);
}

std::string SemanticAnalyzer::c_symbol_name(const syntax::ModuleId module, const std::string_view name) const {
    if (!syntax::is_valid(module) || module.value >= module_.modules.size()) {
        return std::string(name);
    }
    return syntax::mangle_c_symbol(module_.modules[module.value].path, name);
}

std::string SemanticAnalyzer::module_key(const syntax::ModuleId module, const std::string_view name) const {
    return std::to_string(module.value) + ":" + std::string(name);
}

std::string SemanticAnalyzer::function_key(const syntax::ItemNode& function) const {
    const syntax::ModuleId module = item_module(function);
    if (!syntax::is_valid(function.impl_type)) {
        return module_key(module, function.name);
    }
    const TypeHandle owner_type =
        function.impl_type.value < checked_.syntax_type_handles.size()
            ? checked_.syntax_type_handles[function.impl_type.value]
            : invalid_type_handle;
    return method_key(module, owner_type, function.name);
}

std::string SemanticAnalyzer::method_key(
    const syntax::ModuleId module,
    const TypeHandle owner_type,
    const std::string_view name
) const {
    return module_key(module, checked_.types.display_name(owner_type) + "." + std::string(name));
}

std::string SemanticAnalyzer::method_c_symbol_name(
    const TypeHandle owner_type,
    const std::string_view name
) const {
    return checked_.types.c_name(owner_type) + "_" + std::string(name);
}

bool SemanticAnalyzer::can_access(const syntax::ModuleId owner, const syntax::Visibility visibility) const noexcept {
    return owner.value == current_module_.value || visibility == syntax::Visibility::public_;
}

bool SemanticAnalyzer::method_receiver_matches(
    const FunctionSignature& signature,
    const TypeHandle receiver_type,
    const syntax::ExprId receiver
) {
    if (!signature.has_self_param || signature.param_types.empty()) {
        return false;
    }
    const TypeHandle self_type = signature.param_types.front();
    if (checked_.types.same(self_type, receiver_type)) {
        if (is_copy_forbidden_value(self_type)) {
            report(module_.exprs[receiver.value].range, "non-copyable array storage cannot be passed by value");
            return false;
        }
        return true;
    }
    if (!checked_.types.is_pointer(self_type)) {
        return false;
    }
    const TypeHandle pointee = checked_.types.get(self_type).pointee;
    if (checked_.types.is_pointer(receiver_type)) {
        return checked_.types.same(self_type, receiver_type);
    }
    if (!checked_.types.same(pointee, receiver_type)) {
        return false;
    }
    if (!is_place_expr(receiver)) {
        report(module_.exprs[receiver.value].range, "method receiver must be a place expression");
        return false;
    }
    if (checked_.types.get(self_type).pointer_mutability == PointerMutability::mut &&
        !is_writable_place(receiver)) {
        report(module_.exprs[receiver.value].range, "mutable method receiver requires writable storage");
        return false;
    }
    return true;
}

const FunctionSignature* SemanticAnalyzer::find_method_in_visible_modules(
    const TypeHandle owner_type,
    const std::string_view name,
    const base::SourceRange range,
    const bool require_self
) {
    const FunctionSignature* imported_result = nullptr;
    syntax::ModuleId result_module = syntax::invalid_module_id;
    for (syntax::ModuleId module : visible_modules(current_module_)) {
        const auto found = checked_.functions.find(method_key(module, owner_type, name));
        if (found == checked_.functions.end()) {
            continue;
        }
        const FunctionSignature& signature = found->second;
        if (!signature.is_method || (require_self && !signature.has_self_param)) {
            continue;
        }
        if (!can_access(module, signature.visibility)) {
            continue;
        }
        if (imported_result != nullptr) {
            report(range, "ambiguous method '" + checked_.types.display_name(owner_type) + "." + std::string(name) +
                "' from modules " + module_name(result_module) + " and " + module_name(module));
            return nullptr;
        }
        imported_result = &signature;
        result_module = module;
    }
    if (imported_result == nullptr) {
        report(range, "unknown method: " + checked_.types.display_name(owner_type) + "." + std::string(name));
    }
    return imported_result;
}

TypeHandle SemanticAnalyzer::find_type_in_visible_modules(
    const std::string_view name,
    const base::SourceRange range,
    const bool opaque_allowed_as_pointee,
    const bool report_unknown
) {
    if (const auto found = named_types_.find(module_key(current_module_, name)); found != named_types_.end()) {
        return found->second;
    }
    if (const auto found = checked_.type_aliases.find(module_key(current_module_, name)); found != checked_.type_aliases.end()) {
        return resolve_type_alias(found->second, opaque_allowed_as_pointee);
    }

    TypeHandle imported_result = invalid_type_handle;
    syntax::ModuleId result_module = syntax::invalid_module_id;
    for (syntax::ModuleId module : visible_modules(current_module_)) {
        if (module.value == current_module_.value) {
            continue;
        }
            const auto found = named_types_.find(module_key(module, name));
            TypeHandle candidate = invalid_type_handle;
            if (found != named_types_.end()) {
                const auto visibility = type_visibilities_.find(module_key(module, name));
                if (visibility != type_visibilities_.end() && !can_access(module, visibility->second)) {
                    continue;
                }
                candidate = found->second;
            } else {
                const auto alias_found = checked_.type_aliases.find(module_key(module, name));
                if (alias_found == checked_.type_aliases.end()) {
                    continue;
                }
                if (!can_access(module, alias_found->second.visibility)) {
                    continue;
                }
                candidate = resolve_type_alias(alias_found->second, opaque_allowed_as_pointee);
            }
            if (is_valid(imported_result)) {
                report(range, "ambiguous type name '" + std::string(name) + "' from modules " + module_name(result_module) + " and " + module_name(module));
                return invalid_type_handle;
            }
            imported_result = candidate;
            result_module = module;
    }
    if (!is_valid(imported_result) && report_unknown) {
        report(range, "unknown type: " + std::string(name));
    }
    return imported_result;
}

const FunctionSignature* SemanticAnalyzer::find_function_in_visible_modules(const std::string_view name, const base::SourceRange range) {
    if (const auto found = checked_.functions.find(module_key(current_module_, name)); found != checked_.functions.end()) {
        return &found->second;
    }

    const FunctionSignature* imported_result = nullptr;
    syntax::ModuleId result_module = syntax::invalid_module_id;
    for (syntax::ModuleId module : visible_modules(current_module_)) {
        if (module.value == current_module_.value) {
            continue;
        }
            const auto found = checked_.functions.find(module_key(module, name));
            if (found == checked_.functions.end()) {
                continue;
            }
            if (!can_access(module, found->second.visibility)) {
                continue;
            }
            if (imported_result != nullptr) {
                report(range, "ambiguous function name '" + std::string(name) + "' from modules " + module_name(result_module) + " and " + module_name(module));
                return nullptr;
            }
            imported_result = &found->second;
            result_module = module;
    }
    if (imported_result == nullptr) {
        report(range, "unknown function: " + std::string(name));
    }
    return imported_result;
}

const EnumCaseInfo* SemanticAnalyzer::find_enum_case_in_visible_modules(
    const std::string_view name,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (const auto found = checked_.enum_cases.find(module_key(current_module_, name)); found != checked_.enum_cases.end()) {
        return &found->second;
    }

    const EnumCaseInfo* imported_result = nullptr;
    syntax::ModuleId result_module = syntax::invalid_module_id;
    for (syntax::ModuleId module : visible_modules(current_module_)) {
        if (module.value == current_module_.value) {
            continue;
        }
            const auto found = checked_.enum_cases.find(module_key(module, name));
            if (found == checked_.enum_cases.end()) {
                continue;
            }
            if (!can_access(module, found->second.visibility)) {
                continue;
            }
            if (imported_result != nullptr) {
                report(range, "ambiguous enum case '" + std::string(name) + "' from modules " + module_name(result_module) + " and " + module_name(module));
                return nullptr;
            }
            imported_result = &found->second;
            result_module = module;
    }
    if (imported_result == nullptr && report_unknown) {
        report(range, "unknown enum case: " + std::string(name));
    }
    return imported_result;
}

const EnumCaseInfo* SemanticAnalyzer::find_enum_case_by_type_and_case(
    const TypeHandle enum_type,
    const std::string_view case_name
) const noexcept {
    for (const auto& entry : checked_.enum_cases) {
        const EnumCaseInfo& candidate = entry.second;
        if (checked_.types.same(candidate.type, enum_type) && candidate.case_name == case_name) {
            return &candidate;
        }
    }
    return nullptr;
}

const EnumCaseInfo* SemanticAnalyzer::find_enum_case_by_scoped_name(
    const std::string_view enum_name,
    const std::string_view case_name,
    const base::SourceRange range,
    const bool report_unknown
) {
    if (!report_unknown &&
        named_types_.find(module_key(current_module_, enum_name)) == named_types_.end() &&
        checked_.type_aliases.find(module_key(current_module_, enum_name)) == checked_.type_aliases.end()) {
        bool imported_type = false;
        {
            for (syntax::ModuleId module : visible_modules(current_module_)) {
                if (module.value == current_module_.value) {
                    continue;
                }
                const auto named = named_types_.find(module_key(module, enum_name));
                const auto alias = checked_.type_aliases.find(module_key(module, enum_name));
                bool accessible_named = false;
                if (named != named_types_.end()) {
                    const auto visibility = type_visibilities_.find(module_key(module, enum_name));
                    accessible_named = visibility == type_visibilities_.end() || can_access(module, visibility->second);
                }
                const bool accessible_alias = alias != checked_.type_aliases.end() && can_access(module, alias->second.visibility);
                if (accessible_named || accessible_alias) {
                    imported_type = true;
                    break;
                }
            }
        }
        if (!imported_type) {
            return nullptr;
        }
    }
    const TypeHandle enum_type = find_type_in_visible_modules(enum_name, range, false);
    if (!is_valid(enum_type) || checked_.types.get(enum_type).kind != TypeKind::enum_) {
        if (is_valid(enum_type) && report_unknown) {
            report(range, "enum case scope must name an enum type");
        }
        return nullptr;
    }
    if (const EnumCaseInfo* result = find_enum_case_by_type_and_case(enum_type, case_name); result != nullptr) {
        return result;
    }
    if (report_unknown) {
        report(range, "unknown enum case: " + std::string(enum_name) + "." + std::string(case_name));
    }
    return nullptr;
}

const EnumCaseInfo* SemanticAnalyzer::find_enum_constructor(const syntax::ExprId callee_id, const bool report_unknown) {
    if (!syntax::is_valid(callee_id) || callee_id.value >= module_.exprs.size()) {
        return nullptr;
    }
    const syntax::ExprNode& callee = module_.exprs[callee_id.value];
    if (callee.kind == syntax::ExprKind::name) {
        return find_enum_case_in_visible_modules(callee.text, callee.range, report_unknown);
    }
    if (callee.kind != syntax::ExprKind::field ||
        !syntax::is_valid(callee.object) ||
        callee.object.value >= module_.exprs.size() ||
        module_.exprs[callee.object.value].kind != syntax::ExprKind::name) {
        return nullptr;
    }
    const syntax::ExprNode& enum_name = module_.exprs[callee.object.value];
    if (find_generic_enum_template_in_visible_modules(enum_name.text, callee.range, false) != nullptr) {
        return nullptr;
    }
    return find_enum_case_by_scoped_name(enum_name.text, callee.field_name, callee.range, report_unknown);
}

const Symbol* SemanticAnalyzer::find_symbol(const std::string_view name, const base::SourceRange range) {
    if (const Symbol* local = symbols_.find(name); local != nullptr) {
        return local;
    }

    if (const auto found = global_values_.find(module_key(current_module_, name)); found != global_values_.end()) {
        return &found->second;
    }

    const Symbol* imported_result = nullptr;
    syntax::ModuleId result_module = syntax::invalid_module_id;
    for (syntax::ModuleId module : visible_modules(current_module_)) {
        if (module.value == current_module_.value) {
            continue;
        }
            const auto found = global_values_.find(module_key(module, name));
            if (found == global_values_.end()) {
                continue;
            }
            if (!can_access(module, found->second.visibility)) {
                continue;
            }
            if (imported_result != nullptr) {
                report(range, "ambiguous name '" + std::string(name) + "' from modules " + module_name(result_module) + " and " + module_name(module));
                return nullptr;
            }
            imported_result = &found->second;
            result_module = module;
    }
    if (imported_result == nullptr) {
        report(range, "unknown name: " + std::string(name));
    }
    return imported_result;
}

TypeHandle SemanticAnalyzer::record_expr_type(const syntax::ExprId expr, const TypeHandle type) noexcept {
    if (syntax::is_valid(expr) && expr.value < checked_.expr_types.size()) {
        checked_.expr_types[expr.value] = type;
    }
    return type;
}

void SemanticAnalyzer::report(base::SourceRange range, std::string message) {
    diagnostics_.push(base::Diagnostic {
        base::Severity::error,
        range,
        std::move(message),
    });
}

std::string dump_checked_module(const CheckedModule& checked) {
    std::ostringstream out;
    out << "checked_module\n";
    out << "  expr_types " << checked.expr_types.size() << "\n";

    std::vector<std::string> function_names;
    function_names.reserve(checked.functions.size());
    for (const auto& entry : checked.functions) {
        function_names.push_back(entry.first);
    }
    std::sort(function_names.begin(), function_names.end());
    out << "  functions " << function_names.size() << "\n";
    for (const std::string& name : function_names) {
        const FunctionSignature& fn = checked.functions.at(name);
        out << "    fn ";
        if (fn.visibility == syntax::Visibility::private_) {
            out << "priv ";
        }
        if (fn.is_method) {
            out << "method " << checked.types.display_name(fn.method_owner_type) << ".";
        }
        out << fn.name << " -> " << checked.types.display_name(fn.return_type);
        if (fn.c_name != fn.name) {
            out << " @c_name=" << fn.c_name;
        }
        if (fn.is_extern_c) {
            out << " extern_c";
        }
        if (fn.is_export_c) {
            out << " export_c";
        }
        out << "\n";
    }

    std::vector<std::string> struct_names;
    struct_names.reserve(checked.structs.size());
    for (const auto& entry : checked.structs) {
        struct_names.push_back(entry.first);
    }
    std::sort(struct_names.begin(), struct_names.end());
    out << "  structs " << struct_names.size() << "\n";
    for (const std::string& name : struct_names) {
        const StructInfo& info = checked.structs.at(name);
        out << "    struct ";
        if (info.visibility == syntax::Visibility::private_) {
            out << "priv ";
        }
        out << info.name;
        if (info.is_opaque) {
            out << " opaque";
        }
        out << " fields=" << info.fields.size() << "\n";
    }

    std::vector<std::string> alias_names;
    alias_names.reserve(checked.type_aliases.size());
    for (const auto& entry : checked.type_aliases) {
        alias_names.push_back(entry.first);
    }
    std::sort(alias_names.begin(), alias_names.end());
    out << "  type_aliases " << alias_names.size() << "\n";
    for (const std::string& name : alias_names) {
        const TypeAliasInfo& alias = checked.type_aliases.at(name);
        TypeHandle resolved = invalid_type_handle;
        if (alias.target.value < checked.syntax_type_handles.size()) {
            resolved = checked.syntax_type_handles[alias.target.value];
        }
        out << "    type ";
        if (alias.visibility == syntax::Visibility::private_) {
            out << "priv ";
        }
        out << alias.name << " = " << checked.types.display_name(resolved) << "\n";
    }

    out << "  enum_cases " << checked.enum_cases.size() << "\n";
    std::vector<std::string> enum_case_names;
    enum_case_names.reserve(checked.enum_cases.size());
    for (const auto& entry : checked.enum_cases) {
        enum_case_names.push_back(entry.first);
    }
    std::sort(enum_case_names.begin(), enum_case_names.end());
    for (const std::string& name : enum_case_names) {
        const EnumCaseInfo& info = checked.enum_cases.at(name);
        out << "    case " << info.name << " : " << checked.types.display_name(info.type);
        if (is_valid(info.payload_type)) {
            out << "(" << checked.types.display_name(info.payload_type) << ")";
        }
        out << " @c_name=" << info.c_name << "\n";
    }
    return out.str();
}

} // namespace aurex::sema
