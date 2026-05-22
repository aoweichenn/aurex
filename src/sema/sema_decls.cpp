#include <aurex/sema/function_registry.hpp>
#include <aurex/sema/sema_messages.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <sema/internal/sema_core.hpp>

namespace aurex::sema {

namespace {

constexpr char SEMA_ENUM_SYNTHETIC_PAYLOAD_SUFFIX[] = ".payload";
constexpr char SEMA_ENUM_SYNTHETIC_PAYLOAD_C_SUFFIX[] = "_payload";
constexpr char SEMA_ENUM_SYNTHETIC_PAYLOAD_FIELD_PREFIX[] = "_";
constexpr std::string_view SEMA_STABLE_TYPE_ALIAS_INCREMENTAL_TAG = "|type_alias";
constexpr std::string_view SEMA_STABLE_STRUCT_INCREMENTAL_TAG = "|struct";

[[nodiscard]] bool is_main_argv_type(const TypeTable& types, const TypeHandle type) noexcept
{
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
    return inner.pointer_mutability == PointerMutability::mut
        && types.same(inner.pointee, types.builtin(BuiltinType::u8));
}

[[nodiscard]] TypeHandle payload_storage_type(TypeTable& types, const base::u64 size, const base::u64 alignment)
{
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

[[nodiscard]] bool is_const_evaluable_binary_op(const syntax::BinaryOp op) noexcept
{
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
        case syntax::BinaryOp::div:
        case syntax::BinaryOp::mod:
        case syntax::BinaryOp::shl:
        case syntax::BinaryOp::shr:
        case syntax::BinaryOp::logical_and:
        case syntax::BinaryOp::logical_or:
            return true;
    }
    return false;
}

enum class ConstDependencyState : base::u8 {
    UNVISITED = 0,
    VISITING = 1,
    VISITED = 2,
};

enum class ConstEvalStage : base::u8 {
    ENTER = 0,
    AFTER_UNARY = 1,
    AFTER_BINARY_LHS = 2,
    AFTER_BINARY_RHS = 3,
    AFTER_STRUCT_LITERAL = 4,
    AFTER_ARRAY_LITERAL = 5,
    AFTER_TUPLE_LITERAL = 6,
    AFTER_CAST = 7,
};

struct ConstDependencyFrame {
    ModuleLookupKey key;
    bool entered = false;
};

struct ConstEvalFrame {
    syntax::ExprId expr_id = syntax::INVALID_EXPR_ID;
    ConstEvalStage stage = ConstEvalStage::ENTER;
    base::usize child_count = 0;
    bool lhs_result = true;
};

struct AbiFunctionInfo {
    InternedText name;
    TypeHandle return_type = INVALID_TYPE_HANDLE;
    std::vector<TypeHandle> param_types;
    base::SourceRange range{};
    bool is_extern_c = false;
    bool is_variadic = false;
};

[[nodiscard]] ModuleLookupKey const_dependency_key(const syntax::ModuleId module, const IdentId name_id) noexcept
{
    return ModuleLookupKey{
        syntax::is_valid(module) ? module.value : SEMA_LOOKUP_INVALID_KEY_PART,
        name_id,
    };
}

struct AbiSymbolInfo {
    InternedText name;
    base::SourceRange range{};
    bool is_function = false;
    AbiFunctionInfo function;
};

[[nodiscard]] bool is_top_level_type_item(const syntax::ItemKind kind) noexcept
{
    return kind == syntax::ItemKind::struct_decl || kind == syntax::ItemKind::enum_decl
        || kind == syntax::ItemKind::opaque_struct_decl || kind == syntax::ItemKind::type_alias;
}

[[nodiscard]] bool is_top_level_value_item(const syntax::ItemNode& item) noexcept
{
    if (item.kind == syntax::ItemKind::const_decl) {
        return true;
    }
    return item.kind == syntax::ItemKind::fn_decl && !syntax::is_valid(item.impl_type);
}

} // namespace

void SemanticAnalyzerCore::validate_module_namespace_conflicts() const
{
    std::unordered_map<ModuleLookupKey, base::SourceRange, ModuleLookupKeyHash> type_names;
    std::unordered_map<ModuleLookupKey, base::SourceRange, ModuleLookupKeyHash> value_names;
    type_names.reserve(this->ctx_.module.items.size());
    value_names.reserve(this->ctx_.module.items.size());
    for (base::u32 index = 0; index < this->ctx_.module.items.size(); ++index) {
        const syntax::ItemNode item = this->ctx_.module.items[index];
        const syntax::ModuleId owner = this->item_module(syntax::ItemId{index});
        if (is_top_level_type_item(item.kind)) {
            type_names.emplace(this->module_lookup_key(owner, item.name_id), item.range);
        } else if (is_top_level_value_item(item)) {
            value_names.emplace(this->module_lookup_key(owner, item.name_id), item.range);
        }
    }

    for (base::u32 index = 0; index < this->ctx_.module.items.size(); ++index) {
        const syntax::ItemNode item = this->ctx_.module.items[index];
        if (!is_top_level_value_item(item)) {
            continue;
        }
        const syntax::ModuleId owner = this->item_module(syntax::ItemId{index});
        const ModuleLookupKey key = this->module_lookup_key(owner, item.name_id);
        if (type_names.contains(key)) {
            this->report_duplicate(
                item.range, sema_duplicate_namespace_member_message(this->module_name(owner), item.name));
        }
    }

    for (const syntax::ModuleInfo& module_info : this->ctx_.module.modules) {
        const auto* const begin = this->ctx_.module.modules.data();
        const syntax::ModuleId owner{static_cast<base::u32>(&module_info - begin)};
        std::unordered_set<IdentId, IdentIdHash> module_aliases;
        module_aliases.reserve(module_info.imports.size());
        for (const syntax::ResolvedImport& import : module_info.imports) {
            if (!module_aliases.insert(import.alias_id).second) {
                this->report_lookup(import.alias_range, sema_ambiguous_import_alias_message(import.alias));
            }
            const ModuleLookupKey key = this->module_lookup_key(owner, import.alias_id);
            if (type_names.contains(key) || value_names.contains(key)) {
                this->report_duplicate(import.alias_range,
                    sema_duplicate_namespace_member_message(this->module_name(owner), import.alias));
            }
        }
    }
}

void SemanticAnalyzerCore::register_type_names()
{
    const auto report_duplicate_type = [&](const ModuleLookupKey key, const syntax::ModuleId owner,
                                           const base::SourceRange& range, const std::string_view name) {
        this->report_duplicate(range, sema_duplicate_type_definition_message(this->module_name(owner), name));
        for (base::u32 index = 0; index < this->ctx_.module.items.size(); ++index) {
            const syntax::ItemNode candidate = this->ctx_.module.items[index];
            if (candidate.name_id != key.name || this->item_module(syntax::ItemId{index}).value != key.module
                || candidate.range.begin >= range.begin) {
                continue;
            }
            this->report_note(
                candidate.range, SemanticDiagnosticKind::duplicate, sema_previous_declaration_note_message(name));
            return;
        }
    };

    for (base::u32 item_index = 0; item_index < this->ctx_.module.items.size(); ++item_index) {
        const syntax::ItemNode item = this->ctx_.module.items[item_index];
        if (this->has_generic_params(item)) {
            this->register_generic_template(item, syntax::ItemId{item_index});
            continue;
        }
        if (this->has_generic_constraints(item)) {
            for (const syntax::GenericConstraintDecl& constraint : item.where_constraints) {
                this->report_lookup(
                    constraint.param_range, sema_unknown_generic_constraint_param_message(constraint.param_name));
            }
        }
        const syntax::ModuleId owner = this->item_module(syntax::ItemId{item_index});
        const ModuleLookupKey key = this->module_lookup_key(owner, item.name_id);
        const std::string qualified = this->qualified_name(owner, item.name);
        const std::string c_name = this->c_symbol_name(owner, item.name);
        TypeHandle handle = INVALID_TYPE_HANDLE;
        if (item.kind == syntax::ItemKind::type_alias) {
            TypeAliasInfo alias;
            alias.name = this->source_name_text(item.name_id, item.name);
            alias.name_id = item.name_id;
            alias.module = owner;
            alias.target = item.alias_type;
            alias.range = item.range;
            alias.visibility = item.visibility;
            alias.stable_id = this->stable_definition_id(owner, StableSymbolKind::type, item.name_id, item.name);
            alias.incremental_key = this->stable_incremental_key(
                alias.stable_id, std::string(item.name) + std::string(SEMA_STABLE_TYPE_ALIAS_INCREMENTAL_TAG));
            auto alias_inserted = this->state_.checked.type_aliases.emplace(key, alias);
            if (!alias_inserted.second) {
                report_duplicate_type(key, owner, item.range, item.name);
            } else {
                this->index_type_alias(alias_inserted.first->second);
            }
            if (this->state_.types.named_types.contains(key)) {
                report_duplicate_type(key, owner, item.range, item.name);
            }
            if (this->state_.generics.struct_templates.contains(key)) {
                report_duplicate_type(key, owner, item.range, item.name);
            }
            if (this->state_.generics.enum_templates.contains(key)
                || this->state_.generics.type_alias_templates.contains(key)) {
                report_duplicate_type(key, owner, item.range, item.name);
            }
            continue;
        }
        if (item.kind == syntax::ItemKind::struct_decl) {
            if (this->state_.generics.struct_templates.contains(key)
                || this->state_.generics.enum_templates.contains(key)
                || this->state_.generics.type_alias_templates.contains(key)) {
                report_duplicate_type(key, owner, item.range, item.name);
                continue;
            }
            handle = this->state_.checked.types.named_struct(qualified, c_name, false);
        } else if (item.kind == syntax::ItemKind::enum_decl) {
            if (this->state_.generics.struct_templates.contains(key)
                || this->state_.generics.enum_templates.contains(key)
                || this->state_.generics.type_alias_templates.contains(key)) {
                report_duplicate_type(key, owner, item.range, item.name);
                continue;
            }
            handle = this->state_.checked.types.named_enum(qualified, c_name);
        } else if (item.kind == syntax::ItemKind::opaque_struct_decl) {
            handle = this->state_.checked.types.opaque_struct(qualified, c_name);
        }

        if (!is_valid(handle)) {
            continue;
        }
        if (item_index < this->state_.checked.item_c_name_ids.size()) {
            this->state_.checked.item_c_name_ids[item_index] = this->state_.checked.intern_c_name(c_name);
        }
        auto inserted = this->state_.types.named_types.emplace(key, handle);
        if (!inserted.second) {
            report_duplicate_type(key, owner, item.range, item.name);
            continue;
        }
        this->index_named_type(owner, item.name_id, handle, item.visibility);
        if (this->state_.checked.type_aliases.contains(key)) {
            report_duplicate_type(key, owner, item.range, item.name);
            continue;
        }

        if (item.kind == syntax::ItemKind::struct_decl || item.kind == syntax::ItemKind::opaque_struct_decl) {
            StructInfo info = this->state_.checked.make_struct_info();
            info.name = this->source_name_text(item.name_id, item.name);
            info.name_id = item.name_id;
            info.c_name = this->state_.checked.intern_text(c_name);
            info.module = owner;
            info.type = handle;
            info.is_opaque = item.kind == syntax::ItemKind::opaque_struct_decl;
            info.visibility = item.visibility;
            info.stable_id = this->stable_definition_id(owner, StableSymbolKind::type, item.name_id, item.name);
            info.incremental_key = this->stable_incremental_key(
                info.stable_id, std::string(item.name) + std::string(SEMA_STABLE_STRUCT_INCREMENTAL_TAG));
            auto struct_inserted = this->state_.checked.structs.emplace(key, std::move(info));
            if (!struct_inserted.second) {
                this->report_duplicate(
                    item.range, sema_duplicate_struct_definition_message(this->module_name(owner), item.name));
            } else {
                this->state_.types.struct_infos_by_type[handle.value] = &struct_inserted.first->second;
            }
        }
    }
}

void SemanticAnalyzerCore::resolve_type_alias_decls()
{
    for (const auto& entry : this->state_.checked.type_aliases) {
        static_cast<void>(this->resolve_type_alias(entry.second, false));
    }
}

void SemanticAnalyzerCore::register_enum_cases_for_item(const syntax::ItemNode& item, const syntax::ModuleId owner,
    const TypeHandle named_enum_type, std::string enum_display_name, const std::string& case_prefix,
    const std::string& c_prefix, const syntax::Visibility visibility)
{
    const auto make_enum_display_name = [&]() {
        if (!is_valid(named_enum_type)) {
            return enum_display_name;
        }
        const TypeInfo& enum_info = this->state_.checked.types.get(named_enum_type);
        return this->state_.checked.types.display_name(enum_display_name, enum_info.generic_args);
    };
    std::unordered_map<IdentId, base::SourceRange, IdentIdHash> seen_cases;
    seen_cases.reserve(item.enum_cases.size());
    for (const syntax::EnumCaseDecl& enum_case : item.enum_cases) {
        auto inserted_case = seen_cases.emplace(enum_case.name_id, enum_case.range);
        if (!inserted_case.second) {
            this->report_duplicate(
                enum_case.range, sema_duplicate_enum_case_message(make_enum_display_name(), enum_case.name));
            this->report_note(inserted_case.first->second, SemanticDiagnosticKind::duplicate,
                sema_previous_declaration_note_message(enum_case.name));
        }
    }

    const TypeHandle enum_type = syntax::is_valid(item.enum_base_type)
        ? this->resolve_type(item.enum_base_type)
        : this->state_.checked.types.builtin(BuiltinType::u32);
    if (syntax::is_valid(item.enum_base_type) && !this->state_.checked.types.is_integer(enum_type)) {
        this->report_general(item.range, std::string(SEMA_ENUM_BASE_INTEGER));
    }
    if (is_valid(named_enum_type)) {
        this->state_.checked.types.set_enum_underlying(named_enum_type, enum_type);
    }

    std::unordered_set<base::u64> seen_values;
    seen_values.reserve(item.enum_cases.size());
    TypeHandle payload_storage = INVALID_TYPE_HANDLE;
    base::u64 payload_size = 0;
    base::u64 payload_align = 1;
    bool contains_array_payload = false;
    base::u64 next_discriminant = 0;
    std::unordered_map<IdentId, base::SourceRange, IdentIdHash> registered_cases;
    registered_cases.reserve(item.enum_cases.size());
    for (const syntax::EnumCaseDecl& enum_case : item.enum_cases) {
        auto registered_case = registered_cases.emplace(enum_case.name_id, enum_case.range);
        const bool duplicate_case_name = !registered_case.second;
        if (!duplicate_case_name && this->type_member_name_exists(named_enum_type, enum_case.name_id, enum_case.name)) {
            this->report_duplicate(
                enum_case.range, sema_duplicate_type_member_message(make_enum_display_name(), enum_case.name));
        }
        const std::string full_name = case_prefix + std::string(enum_case.name);
        const IdentId full_name_id = this->ctx_.module.intern_identifier(full_name);
        const ModuleLookupKey enum_case_key = this->module_lookup_key(owner, full_name_id);
        const bool has_payload = !enum_case.payload_types.empty() || syntax::is_valid(enum_case.payload_type);
        std::vector<TypeHandle> payload_types;
        payload_types.reserve(enum_case.payload_types.empty() ? 1 : enum_case.payload_types.size());
        if (enum_case.payload_types.empty()) {
            if (syntax::is_valid(enum_case.payload_type)) {
                payload_types.push_back(this->resolve_type(enum_case.payload_type));
            }
        } else {
            for (const syntax::TypeId payload_syntax_type : enum_case.payload_types) {
                payload_types.push_back(this->resolve_type(payload_syntax_type));
            }
        }

        TypeHandle payload_type = INVALID_TYPE_HANDLE;
        if (payload_types.size() == 1) {
            payload_type = payload_types.front();
        } else if (payload_types.size() > 1) {
            const std::string payload_type_name =
                this->qualified_name(owner, full_name + SEMA_ENUM_SYNTHETIC_PAYLOAD_SUFFIX);
            const std::string payload_type_c_name = this->c_symbol_name(
                owner, c_prefix + std::string(enum_case.name) + SEMA_ENUM_SYNTHETIC_PAYLOAD_C_SUFFIX);
            bool payload_contains_array = false;
            for (const TypeHandle field_type : payload_types) {
                payload_contains_array =
                    payload_contains_array || this->state_.checked.types.contains_array(field_type);
            }
            payload_type =
                this->state_.checked.types.named_struct(payload_type_name, payload_type_c_name, payload_contains_array);
            StructInfo payload_info = this->state_.checked.make_struct_info();
            payload_info.name = this->state_.checked.intern_text(payload_type_name);
            payload_info.c_name = this->state_.checked.intern_text(payload_type_c_name);
            payload_info.module = owner;
            payload_info.type = payload_type;
            payload_info.visibility = syntax::Visibility::private_;
            payload_info.stable_id = sema::stable_definition_id(
                this->stable_module_id(owner), StableSymbolKind::synthetic, payload_type_name);
            payload_info.incremental_key = this->stable_incremental_key(payload_info.stable_id, payload_type_name);
            payload_info.fields.reserve(payload_types.size());
            for (base::usize i = 0; i < payload_types.size(); ++i) {
                const std::string field_name =
                    std::string(SEMA_ENUM_SYNTHETIC_PAYLOAD_FIELD_PREFIX) + std::to_string(i);
                payload_info.fields.push_back(StructFieldInfo{
                    this->state_.checked.intern_text(field_name),
                    this->ctx_.module.intern_identifier(field_name),
                    this->state_.checked.intern_text(field_name),
                    owner,
                    payload_types[i],
                    enum_case.range,
                    syntax::Visibility::public_,
                    sema::stable_member_key(
                        payload_info.stable_id, StableSymbolKind::struct_field, field_name, static_cast<base::u32>(i)),
                });
            }
            const auto payload_inserted = this->state_.checked.structs.emplace(
                this->module_lookup_key(
                    owner, this->ctx_.module.intern_identifier(full_name + SEMA_ENUM_SYNTHETIC_PAYLOAD_SUFFIX)),
                std::move(payload_info));
            if (payload_inserted.second) {
                this->state_.types.struct_infos_by_type[payload_type.value] = &payload_inserted.first->second;
            }
        }

        const std::string value_text =
            enum_case.value_text.empty() ? std::to_string(next_discriminant) : std::string(enum_case.value_text);
        base::u64 discriminant = next_discriminant;
        const bool parsed_discriminant = this->parse_integer_literal_text(value_text, discriminant);
        if (!parsed_discriminant) {
            this->report_general(enum_case.range, std::string(SEMA_ENUM_DISCRIMINANT_OUT_OF_RANGE));
        } else if (!this->integer_literal_fits_type(enum_type, value_text)) {
            this->report_general(enum_case.range, std::string(SEMA_ENUM_DISCRIMINANT_DOES_NOT_FIT));
        } else if (!seen_values.insert(discriminant).second) {
            this->report_duplicate(enum_case.range, sema_duplicate_enum_discriminant_message(make_enum_display_name()));
        }
        next_discriminant = discriminant == std::numeric_limits<base::u64>::max() ? discriminant : discriminant + 1;
        if (has_payload) {
            for (const TypeHandle payload_field_type : payload_types) {
                if (!this->is_valid_storage_type(payload_field_type)) {
                    this->report_general(enum_case.range, std::string(SEMA_ENUM_PAYLOAD_STORAGE));
                }
                if (!this->check_m2_value_abi(payload_field_type, ValueAbiContext::enum_payload, enum_case.range)) {
                    contains_array_payload = true;
                }
            }
            const base::u64 case_size = this->abi_size(payload_type);
            const base::u64 case_align = this->abi_align(payload_type);
            if (!is_valid(payload_storage) || case_size > payload_size
                || (case_size == payload_size && case_align > payload_align)) {
                payload_storage = payload_type;
            }
            payload_size = std::max(payload_size, case_size);
            payload_align = std::max(payload_align, case_align);
        }

        EnumCaseInfo case_info = this->state_.checked.make_enum_case_info();
        case_info.name = this->source_name_text(full_name_id, full_name);
        case_info.name_id = full_name_id;
        case_info.c_name =
            this->state_.checked.intern_text(this->c_symbol_name(owner, c_prefix + std::string(enum_case.name)));
        case_info.module = owner;
        case_info.type = named_enum_type;
        case_info.payload_type = payload_type;
        case_info.payload_types = this->state_.checked.copy_type_handle_list(payload_types);
        case_info.value_text = this->state_.checked.intern_text(value_text);
        case_info.range = enum_case.range;
        case_info.enum_name = this->source_name_text(item.name_id, enum_display_name);
        case_info.case_name = this->source_name_text(enum_case.name_id, enum_case.name);
        case_info.case_name_id = enum_case.name_id;
        case_info.visibility = visibility;
        case_info.stable_id =
            sema::stable_definition_id(this->stable_module_id(owner), StableSymbolKind::enum_case, full_name);
        case_info.stable_case_key =
            this->stable_member_key(this->stable_definition_id(owner, StableSymbolKind::type, item.name_id, item.name),
                StableSymbolKind::enum_case, enum_case.name_id, enum_case.name);
        case_info.incremental_key = this->stable_incremental_key(case_info.stable_id, value_text);
        const auto case_inserted = this->state_.checked.enum_cases.emplace(enum_case_key, std::move(case_info));
        if (!case_inserted.second) {
            this->report_duplicate(
                enum_case.range, sema_duplicate_enum_case_message(make_enum_display_name(), enum_case.name));
            this->report_note(case_inserted.first->second.range, SemanticDiagnosticKind::duplicate,
                sema_previous_declaration_note_message(enum_case.name));
            continue;
        }
        this->index_enum_case(case_inserted.first->second);
    }
    if (is_valid(named_enum_type) && is_valid(payload_storage)) {
        this->state_.checked.types.set_enum_payload_layout(named_enum_type,
            payload_storage_type(this->state_.checked.types, payload_size, payload_align), payload_size, payload_align);
    }
    if (is_valid(named_enum_type)) {
        this->state_.checked.types.set_record_contains_array(named_enum_type, contains_array_payload);
    }
}

void SemanticAnalyzerCore::register_value_names()
{
    FunctionRegistry functions(this->state_.checked, this->state_.functions.global_values, this->ctx_.diagnostics,
        this->owned_module_.has_value() ? nullptr : &this->ctx_.module.identifiers);
    for (base::u32 item_index = 0; item_index < this->ctx_.module.items.size(); ++item_index) {
        const syntax::ItemNode item = this->ctx_.module.items[item_index];
        if (this->has_generic_params(item)) {
            continue;
        }
        this->state_.flow.current_module = this->item_module(syntax::ItemId{item_index});
        const ModuleLookupKey item_type_key = this->module_lookup_key(this->state_.flow.current_module, item.name_id);
        FunctionLookupKey key = this->function_lookup_key(this->state_.flow.current_module, item.name_id);
        std::string c_name = this->c_symbol_name(this->state_.flow.current_module, item.name);
        if (item.kind == syntax::ItemKind::fn_decl) {
            const bool is_method = syntax::is_valid(item.impl_type);
            TypeHandle method_owner_type = INVALID_TYPE_HANDLE;
            if (!is_method && this->state_.generics.function_templates.contains(item_type_key)) {
                this->report_duplicate(item.range,
                    sema_duplicate_function_definition_message(
                        this->module_name(this->state_.flow.current_module), item.name));
                continue;
            }
            if (item.is_variadic && !item.is_extern_c) {
                this->report_general(item.range, std::string(SEMA_VARIADIC_EXTERN_C_ONLY));
            }
            if (is_method) {
                method_owner_type = this->resolve_type(item.impl_type);
                if (is_valid(method_owner_type)) {
                    const TypeKind owner_kind = this->state_.checked.types.get(method_owner_type).kind;
                    if (owner_kind != TypeKind::struct_ && owner_kind != TypeKind::enum_
                        && owner_kind != TypeKind::opaque_struct) {
                        this->report_general(item.range, std::string(SEMA_IMPL_TARGET_NAMED_TYPE));
                    }
                }
                key =
                    this->method_function_lookup_key(this->state_.flow.current_module, method_owner_type, item.name_id);
                c_name = this->method_c_symbol_name(method_owner_type, item.name);
            }
            const bool has_explicit_return = syntax::is_valid(item.return_type);
            TypeHandle return_type = INVALID_TYPE_HANDLE;
            if (has_explicit_return) {
                return_type = this->resolve_type(item.return_type);
            } else if (item.is_extern_c || item.is_export_c) {
                this->report_general(item.range, std::string(SEMA_C_ABI_RETURN_TYPE_EXPLICIT));
                return_type = this->state_.checked.types.builtin(BuiltinType::void_);
            } else if (item.is_prototype) {
                this->report_general(item.range, std::string(SEMA_PROTOTYPE_RETURN_TYPE_EXPLICIT));
                return_type = this->state_.checked.types.builtin(BuiltinType::void_);
            } else if (item.visibility == syntax::Visibility::public_) {
                this->report_general(item.range, std::string(SEMA_PUBLIC_FUNCTION_RETURN_TYPE_EXPLICIT));
            }
            std::vector<TypeHandle> param_types;
            param_types.reserve(item.params.size());
            for (const syntax::ParamDecl& param : item.params) {
                TypeHandle param_type = this->resolve_type(param.type);
                if (!this->is_valid_storage_type(param_type)) {
                    this->report_general(param.range, std::string(SEMA_FUNCTION_PARAMETER_STORAGE));
                }
                static_cast<void>(this->check_m2_value_abi(param_type, ValueAbiContext::parameter, param.range));
                param_types.push_back(param_type);
            }
            if (is_method) {
                bool saw_self = false;
                for (base::usize i = 0; i < item.params.size(); ++i) {
                    if (item.params[i].name != "self") {
                        continue;
                    }
                    if (i != 0) {
                        this->report_general(item.params[i].range, std::string(SEMA_METHOD_SELF_FIRST));
                    }
                    saw_self = true;
                }
                if (saw_self && !param_types.empty() && is_valid(method_owner_type)) {
                    TypeHandle self_type = param_types.front();
                    if (this->state_.checked.types.is_pointer(self_type)
                        || this->state_.checked.types.is_reference(self_type)) {
                        self_type = this->state_.checked.types.get(self_type).pointee;
                    }
                    if (!this->state_.checked.types.same(self_type, method_owner_type)) {
                        this->report_general(item.params.front().range, std::string(SEMA_METHOD_SELF_TYPE));
                    }
                }
                if (this->type_member_name_exists(method_owner_type, item.name_id, item.name)) {
                    this->report_duplicate(item.range,
                        sema_duplicate_type_member_message(
                            this->state_.checked.types.display_name(method_owner_type), item.name));
                }
            }
            if (has_explicit_return && is_valid(return_type)) {
                this->validate_function_return_type(item, return_type);
            }
            std::string stable_function_name(item.name);
            StableSymbolKind stable_function_kind = StableSymbolKind::function;
            if (is_method && is_valid(method_owner_type)) {
                stable_function_name =
                    this->state_.checked.types.display_name(method_owner_type) + "." + std::string(item.name);
                stable_function_kind = StableSymbolKind::method;
            }
            const StableDefId stable_id = sema::stable_definition_id(
                this->stable_module_id(this->state_.flow.current_module), stable_function_kind, stable_function_name);
            const IncrementalKey incremental_key = this->stable_incremental_key(stable_id,
                this->function_incremental_fingerprint(
                    stable_function_name, return_type, param_types, is_method, item.is_variadic));
            functions.register_function(FunctionRegistrationRequest{
                item,
                this->state_.flow.current_module,
                key,
                c_name,
                method_owner_type,
                return_type,
                param_types,
                syntax::ItemId{item_index},
                stable_id,
                incremental_key,
            });
            if (const auto found = this->state_.checked.functions.find(key);
                found != this->state_.checked.functions.end()) {
                this->index_function_lookup(found->second);
                this->index_function_value(found->second);
            }
            if (!item.is_prototype && !item.is_extern_c) {
                this->state_.functions.definition_items[key] = syntax::ItemId{item_index};
            }
            this->state_.functions.body_states[key] = FunctionBodyState::not_started;
        } else if (item.kind == syntax::ItemKind::const_decl) {
            TypeHandle type = this->resolve_type(item.const_type);
            if (item_index < this->state_.checked.item_c_name_ids.size()) {
                this->state_.checked.item_c_name_ids[item_index] = this->state_.checked.intern_c_name(c_name);
            }
            const auto inserted = this->state_.functions.global_values.emplace(key,
                Symbol{
                    SymbolKind::const_,
                    this->source_name_text(item.name_id, item.name),
                    item.name_id,
                    this->state_.checked.intern_text(c_name),
                    this->state_.flow.current_module,
                    type,
                    item.range,
                    false,
                    item.visibility,
                    this->stable_definition_id(
                        this->state_.flow.current_module, StableSymbolKind::value, item.name_id, item.name),
                });
            if (!inserted.second) {
                this->report_duplicate(item.range,
                    sema_duplicate_value_definition_message(
                        this->module_name(this->state_.flow.current_module), item.name));
                this->report_note(inserted.first->second.range, SemanticDiagnosticKind::duplicate,
                    sema_previous_declaration_note_message(item.name));
            } else {
                this->index_global_value(inserted.first->second);
            }
        } else if (item.kind == syntax::ItemKind::enum_decl) {
            const auto type_found = this->state_.types.named_types.find(item_type_key);
            this->register_enum_cases_for_item(item, this->state_.flow.current_module,
                type_found == this->state_.types.named_types.end() ? INVALID_TYPE_HANDLE : type_found->second,
                std::string(item.name), std::string(item.name) + "_", std::string(item.name) + "_", item.visibility);
        }
    }
    this->state_.flow.current_module = syntax::INVALID_MODULE_ID;
}

void SemanticAnalyzerCore::validate_function_prototypes() const
{
    for (const auto& entry : this->state_.checked.functions) {
        const FunctionSignature& signature = entry.second;
        if (signature.is_extern_c) {
            continue;
        }
        if (signature.has_conflict) {
            continue;
        }
        if (signature.has_prototype && !signature.has_definition) {
            this->report_duplicate(signature.range, sema_function_prototype_missing_definition_message(signature.name));
        }
    }
}

void SemanticAnalyzerCore::validate_abi_symbols() const
{
    std::unordered_map<std::string_view, AbiSymbolInfo> symbols;
    symbols.reserve(this->state_.checked.functions.size() + this->state_.functions.global_values.size());

    const auto same_function_type = [&](const AbiFunctionInfo& lhs, const AbiFunctionInfo& rhs) {
        if (!this->state_.checked.types.same(lhs.return_type, rhs.return_type) || lhs.is_variadic != rhs.is_variadic
            || lhs.param_types.size() != rhs.param_types.size()) {
            return false;
        }
        for (base::usize i = 0; i < lhs.param_types.size(); ++i) {
            if (!this->state_.checked.types.same(lhs.param_types[i], rhs.param_types[i])) {
                return false;
            }
        }
        return true;
    };

    const auto report_previous_abi_declaration = [&](const std::string_view symbol, const AbiSymbolInfo& prior) {
        this->report_note(
            prior.range, SemanticDiagnosticKind::duplicate, sema_previous_declaration_note_message(symbol));
    };

    const auto insert_function = [&](const std::string_view symbol, AbiFunctionInfo function) {
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
                this->report_type(function.range, sema_extern_c_abi_conflict_message(symbol));
                report_previous_abi_declaration(symbol, prior);
            }
            return;
        }
        this->report_duplicate(function.range, sema_duplicate_abi_symbol_message(symbol));
        report_previous_abi_declaration(symbol, prior);
    };

    for (const auto& entry : this->state_.checked.functions) {
        const FunctionSignature& signature = entry.second;
        if (signature.has_conflict) {
            continue;
        }
        insert_function(signature.c_name,
            AbiFunctionInfo{
                signature.name,
                signature.return_type,
                std::vector<TypeHandle>(signature.param_types.begin(), signature.param_types.end()),
                signature.range,
                signature.is_extern_c,
                signature.is_variadic,
            });
    }

    for (const auto& entry : this->state_.functions.global_values) {
        const Symbol& symbol = entry.second;
        if (symbol.kind == SymbolKind::function || symbol.c_name.empty()) {
            continue;
        }
        const std::string_view symbol_name = symbol.c_name;
        const auto found = symbols.find(symbol_name);
        if (found == symbols.end()) {
            AbiSymbolInfo info;
            info.name = symbol.name;
            info.range = symbol.range;
            info.is_function = false;
            symbols.emplace(symbol_name, std::move(info));
            continue;
        }
        this->report_duplicate(symbol.range, sema_duplicate_abi_symbol_message(symbol.c_name));
        report_previous_abi_declaration(symbol_name, found->second);
    }
}

void SemanticAnalyzerCore::analyze_entry_points() const
{
    constexpr syntax::ModuleId root_module{0};
    const FunctionSignature* aurex_entry = nullptr;
    const FunctionSignature* c_entry = nullptr;

    for (const auto& entry : this->state_.checked.functions) {
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
        this->report_general(aurex_entry->range, std::string(SEMA_ORDINARY_MAIN_EXPORTED_C_MAIN));
    }
    if (aurex_entry->c_name == "main") {
        this->report_general(aurex_entry->range, std::string(SEMA_ORDINARY_MAIN_ABI_NAME));
    }
    const TypeHandle i32_type = this->state_.checked.types.builtin(BuiltinType::i32);
    const TypeHandle void_type = this->state_.checked.types.builtin(BuiltinType::void_);
    if (aurex_entry->param_types.empty()) {
        // fn main() -> i32
    } else if (aurex_entry->param_types.size() == 2) {
        if (!this->state_.checked.types.same(aurex_entry->param_types[0], i32_type)
            || !is_main_argv_type(this->state_.checked.types, aurex_entry->param_types[1])) {
            this->report_general(aurex_entry->range, std::string(SEMA_MAIN_PARAMETERS_EXACT));
        }
    } else {
        this->report_general(aurex_entry->range, std::string(SEMA_MAIN_PARAMETERS));
    }
    if (!this->state_.checked.types.same(aurex_entry->return_type, i32_type)
        && !this->state_.checked.types.same(aurex_entry->return_type, void_type)) {
        this->report_general(aurex_entry->range, std::string(SEMA_MAIN_RETURN));
    }
}

void SemanticAnalyzerCore::analyze_struct_properties()
{
    for (base::u32 index = 0; index < this->ctx_.module.items.size(); ++index) {
        if (this->ctx_.module.items.kind(index) != syntax::ItemKind::struct_decl) {
            continue;
        }
        const syntax::ItemNode item = this->ctx_.module.items[index];
        if (this->has_generic_params(item)) {
            continue;
        }
        this->state_.flow.current_module = this->item_module(syntax::ItemId{index});
        const ModuleLookupKey key = this->module_lookup_key(this->state_.flow.current_module, item.name_id);
        bool contains_array = false;
        std::unordered_map<IdentId, base::SourceRange, IdentIdHash> seen_fields;
        seen_fields.reserve(item.fields.size());
        StructInfo* struct_info = nullptr;
        if (const auto struct_found = this->state_.checked.structs.find(key);
            struct_found != this->state_.checked.structs.end()) {
            struct_info = &struct_found->second;
            struct_info->fields.reserve(item.fields.size());
        }
        for (const syntax::FieldDecl& field : item.fields) {
            const auto inserted_field = seen_fields.emplace(field.name_id, field.range);
            if (!inserted_field.second) {
                this->report_duplicate(field.range, sema_duplicate_struct_field_message(field.name));
                this->report_note(inserted_field.first->second, SemanticDiagnosticKind::duplicate,
                    sema_previous_declaration_note_message(field.name));
                continue;
            }
            const TypeHandle field_type = this->resolve_type(field.type);
            if (!this->is_valid_storage_type(field_type)) {
                this->report_general(field.range, std::string(SEMA_FIELD_STORAGE));
            }
            if (struct_info != nullptr) {
                struct_info->fields.push_back(StructFieldInfo{
                    this->source_name_text(field.name_id, field.name),
                    field.name_id,
                    {},
                    this->state_.flow.current_module,
                    field_type,
                    field.range,
                    field.visibility,
                    this->stable_member_key(
                        struct_info->stable_id, StableSymbolKind::struct_field, field.name_id, field.name),
                });
            }
            if (this->state_.checked.types.contains_array(field_type)) {
                contains_array = true;
            }
        }
        const auto found = this->state_.types.named_types.find(key);
        if (found != this->state_.types.named_types.end()) {
            this->state_.checked.types.set_record_contains_array(found->second, contains_array);
        }
    }
    this->state_.flow.current_module = syntax::INVALID_MODULE_ID;
}

void SemanticAnalyzerCore::analyze_const_decls()
{
    SemaMap<ModuleLookupKey, ModuleLookupList, ModuleLookupKeyHash> dependencies_by_const =
        make_sema_map<ModuleLookupKey, ModuleLookupList, ModuleLookupKeyHash>(
            *this->state_.arena, ModuleLookupKeyHash{});
    SemaMap<ModuleLookupKey, base::SourceRange, ModuleLookupKeyHash> const_ranges =
        make_sema_map<ModuleLookupKey, base::SourceRange, ModuleLookupKeyHash>(
            *this->state_.arena, ModuleLookupKeyHash{});
    SemaMap<ModuleLookupKey, InternedText, ModuleLookupKeyHash> const_names =
        make_sema_map<ModuleLookupKey, InternedText, ModuleLookupKeyHash>(*this->state_.arena, ModuleLookupKeyHash{});
    dependencies_by_const.reserve(this->ctx_.module.items.size());
    const_ranges.reserve(this->ctx_.module.items.size());
    const_names.reserve(this->ctx_.module.items.size());

    for (base::u32 index = 0; index < this->ctx_.module.items.size(); ++index) {
        if (this->ctx_.module.items.kind(index) != syntax::ItemKind::const_decl) {
            continue;
        }
        const syntax::ItemNode item = this->ctx_.module.items[index];
        this->state_.flow.current_module = this->item_module(syntax::ItemId{index});
        const IdentId const_name_id =
            is_valid(item.name_id) ? item.name_id : this->ctx_.module.identifiers.find(item.name);
        const ModuleLookupKey const_key = const_dependency_key(this->state_.flow.current_module, const_name_id);
        if (!is_valid(const_key)) {
            continue;
        }
        const_ranges[const_key] = item.range;
        const_names[const_key] =
            this->state_.checked.intern_text(this->qualified_name(this->state_.flow.current_module, item.name));
        const TypeHandle declared = this->resolve_type(item.const_type);
        const bool previous_const_initializer = this->state_.flow.in_const_initializer;
        this->state_.flow.in_const_initializer = true;
        const TypeHandle actual = this->analyze_expr(item.const_value, declared);
        this->state_.flow.in_const_initializer = previous_const_initializer;
        ModuleLookupSet dependencies =
            make_sema_set<ModuleLookupKey, ModuleLookupKeyHash>(*this->state_.arena, ModuleLookupKeyHash{});
        if (!this->is_const_evaluable_expr(item.const_value, dependencies)) {
            const base::SourceRange range =
                syntax::is_valid(item.const_value) && item.const_value.value < this->ctx_.module.exprs.size()
                ? this->ctx_.module.exprs.range(item.const_value.value)
                : item.range;
            this->report_general(range, std::string(SEMA_CONST_NOT_COMPILE_TIME));
        }
        ModuleLookupList dependency_list = make_sema_vector<ModuleLookupKey>(*this->state_.arena);
        dependency_list.reserve(dependencies.size());
        dependency_list.insert(dependency_list.end(), dependencies.begin(), dependencies.end());
        if (const auto found = dependencies_by_const.find(const_key); found != dependencies_by_const.end()) {
            found->second = std::move(dependency_list);
        } else {
            dependencies_by_const.emplace(const_key, std::move(dependency_list));
        }
        if (!this->is_valid_storage_type(declared)) {
            this->report_general(item.range, std::string(SEMA_CONST_TYPE_STORAGE));
        }
        if (!this->can_assign(declared, actual, item.const_value)) {
            this->report_type_mismatch(item.range, std::string(SEMA_CONST_TYPE_MISMATCH), declared, actual);
        }
    }

    constexpr base::u8 SEMA_CONST_DEP_STATE_VISITING = static_cast<base::u8>(ConstDependencyState::VISITING);
    constexpr base::u8 SEMA_CONST_DEP_STATE_VISITED = static_cast<base::u8>(ConstDependencyState::VISITED);

    SemaMap<ModuleLookupKey, base::u8, ModuleLookupKeyHash> states =
        make_sema_map<ModuleLookupKey, base::u8, ModuleLookupKeyHash>(*this->state_.arena, ModuleLookupKeyHash{});
    std::vector<ConstDependencyFrame> stack;
    states.reserve(dependencies_by_const.size());
    stack.reserve(dependencies_by_const.size());
    for (const auto& entry : dependencies_by_const) {
        if (states[entry.first] == SEMA_CONST_DEP_STATE_VISITED) {
            continue;
        }
        stack.push_back(ConstDependencyFrame{entry.first, false});
        while (!stack.empty()) {
            ConstDependencyFrame frame = stack.back();
            stack.pop_back();

            base::u8& state = states[frame.key];
            if (frame.entered) {
                state = SEMA_CONST_DEP_STATE_VISITED;
                continue;
            }
            if (state == SEMA_CONST_DEP_STATE_VISITED) {
                continue;
            }
            if (state == SEMA_CONST_DEP_STATE_VISITING) {
                const auto range = const_ranges.find(frame.key);
                const auto name = const_names.find(frame.key);
                const std::string_view display_name = name == const_names.end()
                    ? this->ctx_.module.identifiers.text(frame.key.name)
                    : name->second.view();
                this->report_general(range == const_ranges.end() ? base::SourceRange{} : range->second,
                    sema_cyclic_const_initializer_message(display_name));
                state = SEMA_CONST_DEP_STATE_VISITED;
                continue;
            }
            state = SEMA_CONST_DEP_STATE_VISITING;
            stack.push_back(ConstDependencyFrame{frame.key, true});
            if (const auto found = dependencies_by_const.find(frame.key); found != dependencies_by_const.end()) {
                for (auto it = found->second.rbegin(); it != found->second.rend(); ++it) {
                    if (dependencies_by_const.contains(*it)) {
                        stack.push_back(ConstDependencyFrame{*it, false});
                    }
                }
            }
        }
    }
    this->state_.flow.current_module = syntax::INVALID_MODULE_ID;
}

bool SemanticAnalyzerCore::is_const_evaluable_expr(const syntax::ExprId expr_id, ModuleLookupSet& dependencies)
{
    if (!syntax::is_valid(expr_id) || expr_id.value >= this->ctx_.module.exprs.size()) {
        return false;
    }
    std::vector<ConstEvalFrame> stack;
    std::vector<bool> values;
    stack.push_back(ConstEvalFrame{expr_id, ConstEvalStage::ENTER, 0});
    while (!stack.empty()) {
        const ConstEvalFrame frame = stack.back();
        stack.pop_back();

        if (!syntax::is_valid(frame.expr_id) || frame.expr_id.value >= this->ctx_.module.exprs.size()) {
            values.push_back(false);
            continue;
        }
        const ExprView expr = this->expr_view(frame.expr_id);
        switch (frame.stage) {
            case ConstEvalStage::ENTER:
                switch (expr.kind) {
                    case syntax::ExprKind::integer_literal:
                    case syntax::ExprKind::float_literal:
                    case syntax::ExprKind::bool_literal:
                    case syntax::ExprKind::null_literal:
                    case syntax::ExprKind::string_literal:
                    case syntax::ExprKind::raw_string_literal:
                    case syntax::ExprKind::c_string_literal:
                    case syntax::ExprKind::byte_string_literal:
                    case syntax::ExprKind::byte_literal:
                    case syntax::ExprKind::char_literal:
                    case syntax::ExprKind::size_of:
                    case syntax::ExprKind::align_of:
                        values.push_back(true);
                        break;
                    case syntax::ExprKind::name: {
                        const Symbol* symbol = nullptr;
                        if (!expr.scope_name.empty()) {
                            const syntax::ModuleId module =
                                this->resolve_import_alias(expr.scope_name, expr.scope_range, false);
                            symbol = syntax::is_valid(module)
                                ? this->find_symbol_in_module(module, expr.text_id, expr.text, expr.range, false)
                                : nullptr;
                        } else {
                            if (const Symbol* local = this->state_.names.symbols.find(expr.text_id); local != nullptr) {
                                symbol = local;
                            } else {
                                const ModuleLookupKey lookup_key =
                                    this->find_module_lookup_key(this->state_.flow.current_module, expr.text_id);
                                if (is_valid(lookup_key)) {
                                    if (const auto found = this->state_.names.global_values_by_name.find(lookup_key);
                                        found != this->state_.names.global_values_by_name.end()) {
                                        symbol = found->second;
                                    }
                                }
                            }
                        }
                        if (symbol == nullptr) {
                            values.push_back(false);
                            break;
                        }
                        if (symbol->kind != SymbolKind::const_) {
                            values.push_back(false);
                            break;
                        }
                        const IdentId dependency_name_id = is_valid(symbol->name_id)
                            ? symbol->name_id
                            : this->ctx_.module.identifiers.find(symbol->name);
                        const ModuleLookupKey dependency_key = const_dependency_key(symbol->module, dependency_name_id);
                        if (is_valid(dependency_key)) {
                            dependencies.insert(dependency_key);
                        }
                        values.push_back(true);
                        break;
                    }
                    case syntax::ExprKind::field: {
                        bool const_evaluable = false;
                        const std::string_view expr_c_name = this->cached_expr_c_name(frame.expr_id);
                        if (!expr_c_name.empty()) {
                            for (const auto& entry : this->state_.checked.enum_cases) {
                                if (entry.second.c_name == expr_c_name) {
                                    const_evaluable = true;
                                    break;
                                }
                            }
                        }
                        values.push_back(const_evaluable);
                        break;
                    }
                    case syntax::ExprKind::struct_literal:
                        if (expr.field_inits.empty()) {
                            values.push_back(true);
                            break;
                        }
                        stack.push_back(ConstEvalFrame{
                            frame.expr_id, ConstEvalStage::AFTER_STRUCT_LITERAL, expr.field_inits.size()});
                        for (auto it = expr.field_inits.rbegin(); it != expr.field_inits.rend(); ++it) {
                            stack.push_back(ConstEvalFrame{it->value, ConstEvalStage::ENTER, 0});
                        }
                        break;
                    case syntax::ExprKind::array_literal:
                        if (syntax::is_valid(expr.array_repeat_value)) {
                            if (!syntax::is_valid(expr.array_repeat_count)) {
                                values.push_back(false);
                                break;
                            }
                            stack.push_back(ConstEvalFrame{frame.expr_id, ConstEvalStage::AFTER_ARRAY_LITERAL, 2});
                            stack.push_back(ConstEvalFrame{expr.array_repeat_count, ConstEvalStage::ENTER, 0});
                            stack.push_back(ConstEvalFrame{expr.array_repeat_value, ConstEvalStage::ENTER, 0});
                            break;
                        }
                        if (expr.array_elements.empty()) {
                            values.push_back(true);
                            break;
                        }
                        stack.push_back(ConstEvalFrame{
                            frame.expr_id,
                            ConstEvalStage::AFTER_ARRAY_LITERAL,
                            expr.array_elements.size(),
                        });
                        for (auto it = expr.array_elements.rbegin(); it != expr.array_elements.rend(); ++it) {
                            stack.push_back(ConstEvalFrame{*it, ConstEvalStage::ENTER, 0});
                        }
                        break;
                    case syntax::ExprKind::tuple_literal:
                        stack.push_back(ConstEvalFrame{
                            frame.expr_id,
                            ConstEvalStage::AFTER_TUPLE_LITERAL,
                            expr.tuple_elements.size(),
                        });
                        for (auto it = expr.tuple_elements.rbegin(); it != expr.tuple_elements.rend(); ++it) {
                            stack.push_back(ConstEvalFrame{*it, ConstEvalStage::ENTER, 0});
                        }
                        break;
                    case syntax::ExprKind::unsafe_block:
                        values.push_back(false);
                        break;
                    case syntax::ExprKind::unary:
                        if (expr.unary_op != syntax::UnaryOp::logical_not
                            && expr.unary_op != syntax::UnaryOp::numeric_negate
                            && expr.unary_op != syntax::UnaryOp::bitwise_not) {
                            values.push_back(false);
                            break;
                        }
                        stack.push_back(ConstEvalFrame{frame.expr_id, ConstEvalStage::AFTER_UNARY, 0});
                        stack.push_back(ConstEvalFrame{expr.unary_operand, ConstEvalStage::ENTER, 0});
                        break;
                    case syntax::ExprKind::binary:
                        if (!is_const_evaluable_binary_op(expr.binary_op)) {
                            values.push_back(false);
                            break;
                        }
                        stack.push_back(ConstEvalFrame{frame.expr_id, ConstEvalStage::AFTER_BINARY_LHS, 0});
                        stack.push_back(ConstEvalFrame{expr.binary_lhs, ConstEvalStage::ENTER, 0});
                        break;
                    case syntax::ExprKind::cast:
                    case syntax::ExprKind::pcast:
                    case syntax::ExprKind::bcast:
                    case syntax::ExprKind::ptr_addr:
                    case syntax::ExprKind::paddr:
                        stack.push_back(ConstEvalFrame{frame.expr_id, ConstEvalStage::AFTER_CAST, 0});
                        stack.push_back(ConstEvalFrame{expr.cast_expr, ConstEvalStage::ENTER, 0});
                        break;
                    default:
                        values.push_back(false);
                        break;
                }
                break;
            case ConstEvalStage::AFTER_UNARY: {
                const bool child = values.back();
                values.pop_back();
                values.push_back(child);
                break;
            }
            case ConstEvalStage::AFTER_BINARY_LHS: {
                const bool lhs = values.back();
                values.pop_back();
                stack.push_back(ConstEvalFrame{frame.expr_id, ConstEvalStage::AFTER_BINARY_RHS, 0, lhs});
                stack.push_back(ConstEvalFrame{expr.binary_rhs, ConstEvalStage::ENTER, 0});
                break;
            }
            case ConstEvalStage::AFTER_BINARY_RHS: {
                const bool rhs = values.back();
                values.pop_back();
                values.push_back(frame.lhs_result && rhs);
                break;
            }
            case ConstEvalStage::AFTER_STRUCT_LITERAL: {
                bool ok = true;
                for (base::usize index = 0; index < frame.child_count; ++index) {
                    ok = values.back() && ok;
                    values.pop_back();
                }
                values.push_back(ok);
                break;
            }
            case ConstEvalStage::AFTER_ARRAY_LITERAL: {
                bool ok = true;
                for (base::usize index = 0; index < frame.child_count; ++index) {
                    ok = values.back() && ok;
                    values.pop_back();
                }
                values.push_back(ok);
                break;
            }
            case ConstEvalStage::AFTER_TUPLE_LITERAL: {
                bool ok = true;
                for (base::usize index = 0; index < frame.child_count; ++index) {
                    ok = values.back() && ok;
                    values.pop_back();
                }
                values.push_back(ok);
                break;
            }
            case ConstEvalStage::AFTER_CAST: {
                const bool child = values.back();
                values.pop_back();
                values.push_back(child);
                break;
            }
        }
    }
    return !values.empty() && values.back();
}

} // namespace aurex::sema
