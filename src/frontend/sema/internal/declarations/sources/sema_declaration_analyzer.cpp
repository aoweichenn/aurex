#include <aurex/frontend/sema/function_registry.hpp>
#include <aurex/frontend/sema/resource_semantics.hpp>
#include <aurex/frontend/sema/sema_messages.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <frontend/sema/internal/borrow/private/contract.hpp>
#include <frontend/sema/internal/declarations/private/sema_declaration_analyzer.hpp>

namespace aurex::sema {

SemanticAnalyzerCore::DeclarationAnalyzer::DeclarationAnalyzer(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

namespace {

constexpr char SEMA_ENUM_SYNTHETIC_PAYLOAD_SUFFIX[] = ".payload";
constexpr char SEMA_ENUM_SYNTHETIC_PAYLOAD_C_SUFFIX[] = "_payload";
constexpr char SEMA_ENUM_SYNTHETIC_PAYLOAD_FIELD_PREFIX[] = "_";
constexpr std::string_view SEMA_STABLE_TYPE_ALIAS_INCREMENTAL_TAG = "|type_alias";
constexpr std::string_view SEMA_STABLE_STRUCT_INCREMENTAL_TAG = "|struct";
constexpr std::size_t SEMA_IMPORT_SCOPE_HASH_MIX = 0x9e3779b97f4a7c15ULL;
constexpr base::usize SEMA_IMPORT_SCOPE_HASH_LEFT_SHIFT = 6;
constexpr base::usize SEMA_IMPORT_SCOPE_HASH_RIGHT_SHIFT = 2;
constexpr base::usize SEMA_EXPORT_SURFACE_INITIAL_STACK_CAPACITY = 8;
constexpr std::string_view SEMA_DERIVE_COPY = "Copy";
constexpr std::string_view SEMA_DERIVE_EQ = "Eq";
constexpr std::string_view SEMA_DERIVE_HASH = "Hash";

struct ImportScopeKey {
    base::u32 source = 0;
    base::usize begin = 0;
    base::usize end = 0;

    [[nodiscard]] friend constexpr bool operator==(
        const ImportScopeKey& lhs, const ImportScopeKey& rhs) noexcept = default;
};

struct ImportScopeKeyHash {
    [[nodiscard]] std::size_t operator()(const ImportScopeKey& key) const noexcept
    {
        std::size_t hash = static_cast<std::size_t>(key.source);
        hash ^= key.begin + SEMA_IMPORT_SCOPE_HASH_MIX + (hash << SEMA_IMPORT_SCOPE_HASH_LEFT_SHIFT)
            + (hash >> SEMA_IMPORT_SCOPE_HASH_RIGHT_SHIFT);
        hash ^= key.end + SEMA_IMPORT_SCOPE_HASH_MIX + (hash << SEMA_IMPORT_SCOPE_HASH_LEFT_SHIFT)
            + (hash >> SEMA_IMPORT_SCOPE_HASH_RIGHT_SHIFT);
        return hash;
    }
};

[[nodiscard]] ImportScopeKey import_scope_key(const std::span<const syntax::ResolvedImport> imports) noexcept
{
    if (imports.empty()) {
        return {};
    }
    const base::SourceRange& range = imports.front().alias_range;
    return ImportScopeKey{range.source.value, range.begin, range.end};
}

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

[[nodiscard]] TypeHandle payload_storage_type(
    TypeTable& types, const base::u64 size, const base::u64 alignment, const SemanticTargetLayout& target)
{
    TypeHandle unit = types.builtin(BuiltinType::u8);
    base::u64 unit_size = target.i8_size;
    if (alignment >= target.i64_align) {
        unit = types.builtin(BuiltinType::u64);
        unit_size = target.i64_size;
    } else if (alignment >= target.i32_align) {
        unit = types.builtin(BuiltinType::u32);
        unit_size = target.i32_size;
    } else if (alignment >= target.i16_align) {
        unit = types.builtin(BuiltinType::u16);
        unit_size = target.i16_size;
    }
    const base::u64 effective_unit_size = std::max<base::u64>(1, unit_size);
    const base::u64 count = std::max<base::u64>(1, (size + effective_unit_size - 1) / effective_unit_size);
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

[[nodiscard]] std::string function_surface_name(const TypeTable& types, const FunctionSignature& signature)
{
    if (signature.is_method) {
        return types.display_name(signature.method_owner_type) + "." + function_display_name(types, signature);
    }
    return std::string(signature.name);
}

[[nodiscard]] std::string struct_field_surface_name(
    const TypeTable& types, const StructInfo& info, const StructFieldInfo& field)
{
    return types.display_name(info.type) + "." + std::string(field.name);
}

[[nodiscard]] std::string enum_case_surface_name(const TypeTable& types, const EnumCaseInfo& info)
{
    std::string display = types.display_name(info.type);
    display += "_";
    display += info.case_name.view();
    return display;
}

[[nodiscard]] std::optional<TypeHandle> cached_checked_syntax_type(
    const CheckedModule& checked, const syntax::TypeId type) noexcept
{
    if (!syntax::is_valid(type) || type.value >= checked.syntax_type_handles.size()) {
        return std::nullopt;
    }
    return checked.syntax_type_handles[type.value];
}

[[nodiscard]] bool visibility_has_export_surface(const syntax::Visibility visibility) noexcept
{
    return syntax::visibility_at_least(visibility, syntax::Visibility::package_);
}

[[nodiscard]] bool is_top_level_type_item(const syntax::ItemNode& item) noexcept
{
    if (item.kind == syntax::ItemKind::type_alias
        && (syntax::is_valid(item.impl_type) || syntax::is_valid(item.trait_type)
            || !syntax::is_valid(item.alias_type))) {
        return false;
    }
    return item.kind == syntax::ItemKind::struct_decl || item.kind == syntax::ItemKind::enum_decl
        || item.kind == syntax::ItemKind::opaque_struct_decl || item.kind == syntax::ItemKind::type_alias
        || item.kind == syntax::ItemKind::trait_decl;
}

[[nodiscard]] bool is_top_level_value_item(const syntax::ItemNode& item) noexcept
{
    if (item.kind == syntax::ItemKind::const_decl) {
        return true;
    }
    return item.kind == syntax::ItemKind::fn_decl && !syntax::is_valid(item.impl_type);
}

[[nodiscard]] std::optional<CapabilityKind> parse_derive_capability(const std::string_view name) noexcept
{
    if (name == SEMA_DERIVE_COPY) {
        return CapabilityKind::copy;
    }
    if (name == SEMA_DERIVE_EQ) {
        return CapabilityKind::eq;
    }
    if (name == SEMA_DERIVE_HASH) {
        return CapabilityKind::hash;
    }
    return std::nullopt;
}

} // namespace

void SemanticAnalyzerCore::DeclarationAnalyzer::validate_module_namespace_conflicts() const
{
    std::unordered_map<ModuleLookupKey, base::SourceRange, ModuleLookupKeyHash> type_names;
    std::unordered_map<ModuleLookupKey, base::SourceRange, ModuleLookupKeyHash> value_names;
    type_names.reserve(this->core_.ctx_.module.items.size());
    value_names.reserve(this->core_.ctx_.module.items.size());
    for (base::u32 index = 0; index < this->core_.ctx_.module.items.size(); ++index) {
        const syntax::ItemNode item = this->core_.ctx_.module.items[index];
        const syntax::ModuleId owner = this->core_.item_module(syntax::ItemId{index});
        if (is_top_level_type_item(item)) {
            type_names.emplace(this->core_.module_lookup_key(owner, item.name_id), item.range);
        } else if (is_top_level_value_item(item)) {
            value_names.emplace(this->core_.module_lookup_key(owner, item.name_id), item.range);
        }
    }

    for (base::u32 index = 0; index < this->core_.ctx_.module.items.size(); ++index) {
        const syntax::ItemNode item = this->core_.ctx_.module.items[index];
        if (!is_top_level_value_item(item)) {
            continue;
        }
        const syntax::ModuleId owner = this->core_.item_module(syntax::ItemId{index});
        const ModuleLookupKey key = this->core_.module_lookup_key(owner, item.name_id);
        if (type_names.contains(key)) {
            this->core_.report_duplicate(
                item.range, sema_duplicate_namespace_member_message(this->core_.module_name(owner), item.name));
        }
    }

    std::unordered_set<ImportScopeKey, ImportScopeKeyHash> validated_import_scopes;
    const auto validate_import_scope = [&](const syntax::ModuleId owner,
                                           const std::span<const syntax::ResolvedImport> imports) {
        if (imports.empty()) {
            return;
        }
        if (!validated_import_scopes.insert(import_scope_key(imports)).second) {
            return;
        }
        std::unordered_set<IdentId, IdentIdHash> module_aliases;
        module_aliases.reserve(imports.size());
        for (const syntax::ResolvedImport& import : imports) {
            if (!module_aliases.insert(import.alias_id).second) {
                this->core_.report_lookup(import.alias_range, sema_ambiguous_import_alias_message(import.alias));
            }
            const ModuleLookupKey key = this->core_.module_lookup_key(owner, import.alias_id);
            if (type_names.contains(key) || value_names.contains(key)) {
                this->core_.report_duplicate(import.alias_range,
                    sema_duplicate_namespace_member_message(this->core_.module_name(owner), import.alias));
            }
        }
    };

    for (const syntax::ModuleInfo& module_info : this->core_.ctx_.module.modules) {
        const auto* const begin = this->core_.ctx_.module.modules.data();
        const syntax::ModuleId owner{static_cast<base::u32>(&module_info - begin)};
        validate_import_scope(owner, module_info.imports);
        std::unordered_set<IdentId, IdentIdHash> import_aliases;
        import_aliases.reserve(module_info.imports.size());
        for (const syntax::ResolvedImport& import : module_info.imports) {
            import_aliases.insert(import.alias_id);
        }
        std::unordered_set<IdentId, IdentIdHash> reexport_aliases;
        reexport_aliases.reserve(module_info.reexports.size());
        for (const syntax::ResolvedUse& reexport : module_info.reexports) {
            if (!reexport_aliases.insert(reexport.alias_id).second) {
                this->core_.report_duplicate(reexport.alias_range,
                    sema_duplicate_namespace_member_message(this->core_.module_name(owner), reexport.alias));
            }
            if (import_aliases.contains(reexport.alias_id)) {
                this->core_.report_duplicate(reexport.alias_range,
                    sema_duplicate_namespace_member_message(this->core_.module_name(owner), reexport.alias));
            }
            const ModuleLookupKey key = this->core_.module_lookup_key(owner, reexport.alias_id);
            if (type_names.contains(key) || value_names.contains(key)) {
                this->core_.report_duplicate(reexport.alias_range,
                    sema_duplicate_namespace_member_message(this->core_.module_name(owner), reexport.alias));
            }
            if (!syntax::is_valid(reexport.module) || reexport.module.value >= this->core_.ctx_.module.modules.size()) {
                continue;
            }
            const ModuleLookupKey target_key =
                this->core_.find_module_lookup_key(reexport.module, reexport.target_name_id);
            bool target_exists = false;
            bool target_visible_enough = false;
            const auto consider_visibility = [&](const syntax::Visibility visibility) {
                target_exists = true;
                target_visible_enough =
                    target_visible_enough || syntax::visibility_at_least(visibility, reexport.visibility);
            };
            if (is_valid(target_key)) {
                if (const auto found = this->core_.state_.names.named_types_by_name.find(target_key);
                    found != this->core_.state_.names.named_types_by_name.end()) {
                    consider_visibility(found->second.visibility);
                }
                if (const auto found = this->core_.state_.names.type_aliases_by_name.find(target_key);
                    found != this->core_.state_.names.type_aliases_by_name.end() && found->second != nullptr) {
                    consider_visibility(found->second->visibility);
                }
                if (const auto found = this->core_.state_.names.global_values_by_name.find(target_key);
                    found != this->core_.state_.names.global_values_by_name.end() && found->second != nullptr) {
                    consider_visibility(found->second->visibility);
                }
                if (const auto found = this->core_.state_.names.generic_struct_templates_by_name.find(target_key);
                    found != this->core_.state_.names.generic_struct_templates_by_name.end()
                    && found->second != nullptr) {
                    consider_visibility(found->second->visibility);
                }
                if (const auto found = this->core_.state_.names.generic_enum_templates_by_name.find(target_key);
                    found != this->core_.state_.names.generic_enum_templates_by_name.end()
                    && found->second != nullptr) {
                    consider_visibility(found->second->visibility);
                }
                if (const auto found = this->core_.state_.names.generic_type_alias_templates_by_name.find(target_key);
                    found != this->core_.state_.names.generic_type_alias_templates_by_name.end()
                    && found->second != nullptr) {
                    consider_visibility(found->second->visibility);
                }
                if (const auto found = this->core_.state_.names.generic_function_templates_by_name.find(target_key);
                    found != this->core_.state_.names.generic_function_templates_by_name.end()
                    && found->second != nullptr) {
                    consider_visibility(found->second->visibility);
                }
                if (const auto found = this->core_.state_.names.traits_by_name.find(target_key);
                    found != this->core_.state_.names.traits_by_name.end() && found->second != nullptr) {
                    consider_visibility(found->second->visibility);
                }
            }
            if (!target_exists) {
                this->core_.report_lookup(reexport.target_range,
                    sema_unknown_reexport_target_message(
                        this->core_.module_name(reexport.module), reexport.target_name));
            } else if (!target_visible_enough) {
                this->core_.report_visibility(reexport.target_range,
                    sema_private_reexport_target_message(
                        this->core_.module_name(reexport.module), reexport.target_name));
            }
        }
    }
    for (const syntax::ItemImportScope& scope : this->core_.ctx_.module.item_import_scopes) {
        if (scope.item_count == 0) {
            continue;
        }
        validate_import_scope(this->core_.item_module(syntax::ItemId{scope.item_begin}), scope.imports);
    }
}

void SemanticAnalyzerCore::DeclarationAnalyzer::register_type_names()
{
    const auto report_duplicate_type = [&](const ModuleLookupKey key, const syntax::ModuleId owner,
                                           const base::SourceRange& range, const std::string_view name) {
        this->core_.report_duplicate(
            range, sema_duplicate_type_definition_message(this->core_.module_name(owner), name));
        for (base::u32 index = 0; index < this->core_.ctx_.module.items.size(); ++index) {
            const syntax::ItemNode candidate = this->core_.ctx_.module.items[index];
            if (candidate.name_id != key.name || this->core_.item_module(syntax::ItemId{index}).value != key.module
                || candidate.range.begin >= range.begin) {
                continue;
            }
            this->core_.report_note(
                candidate.range, SemanticDiagnosticKind::duplicate, sema_previous_declaration_note_message(name));
            return;
        }
    };

    for (base::u32 item_index = 0; item_index < this->core_.ctx_.module.items.size(); ++item_index) {
        const syntax::ItemNode item = this->core_.ctx_.module.items[item_index];
        if (item.kind == syntax::ItemKind::trait_decl) {
            this->core_.register_trait_name(item, syntax::ItemId{item_index});
            continue;
        }
        if (item.kind == syntax::ItemKind::type_alias
            && (syntax::is_valid(item.impl_type) || syntax::is_valid(item.trait_type)
                || !syntax::is_valid(item.alias_type))) {
            continue;
        }
        const syntax::ItemId item_id{item_index};
        if (this->core_.has_lifetime_origin_params(item) && !this->core_.has_generic_params(item)) {
            this->core_.validate_generic_parameter_list(item);
            this->core_.record_lifetime_origin_params(item, item_id);
        }
        if (this->core_.has_generic_params(item)) {
            this->core_.register_generic_template(item, item_id);
            continue;
        }
        if (this->core_.has_generic_constraints(item)) {
            for (const syntax::GenericConstraintDecl& constraint : item.where_constraints) {
                this->core_.report_lookup(
                    constraint.param_range, sema_unknown_generic_constraint_param_message(constraint.param_name));
            }
        }
        const syntax::ModuleId owner = this->core_.item_module(item_id);
        const base::u32 part_index = this->core_.item_part_index(item_id);
        const ModuleLookupKey key = this->core_.module_lookup_key(owner, item.name_id);
        const std::string qualified = this->core_.qualified_name(owner, item.name);
        const std::string c_name = this->core_.c_symbol_name(owner, item.name);
        TypeHandle handle = INVALID_TYPE_HANDLE;
        if (item.kind == syntax::ItemKind::type_alias) {
            TypeAliasInfo alias;
            alias.name = this->core_.source_name_text(item.name_id, item.name);
            alias.name_id = item.name_id;
            alias.module = owner;
            alias.item = item_id;
            alias.target = item.alias_type;
            alias.range = item.range;
            alias.visibility = item.visibility;
            alias.stable_id = this->core_.stable_definition_id(owner, StableSymbolKind::type, item.name_id, item.name);
            alias.incremental_key = this->core_.stable_incremental_key(
                alias.stable_id, std::string(item.name) + std::string(SEMA_STABLE_TYPE_ALIAS_INCREMENTAL_TAG));
            alias.part_index = part_index;
            auto alias_inserted = this->core_.state_.checked.type_aliases.emplace(key, alias);
            if (!alias_inserted.second) {
                report_duplicate_type(key, owner, item.range, item.name);
            } else {
                this->core_.index_type_alias(alias_inserted.first->second);
            }
            if (this->core_.state_.types.named_types.contains(key)) {
                report_duplicate_type(key, owner, item.range, item.name);
            }
            if (this->core_.state_.generics.struct_templates.contains(key)) {
                report_duplicate_type(key, owner, item.range, item.name);
            }
            if (this->core_.state_.generics.enum_templates.contains(key)
                || this->core_.state_.generics.type_alias_templates.contains(key)) {
                report_duplicate_type(key, owner, item.range, item.name);
            }
            if (this->core_.state_.checked.traits.contains(key)) {
                report_duplicate_type(key, owner, item.range, item.name);
            }
            continue;
        }
        if (item.kind == syntax::ItemKind::struct_decl) {
            if (this->core_.state_.generics.struct_templates.contains(key)
                || this->core_.state_.generics.enum_templates.contains(key)
                || this->core_.state_.generics.type_alias_templates.contains(key)
                || this->core_.state_.checked.traits.contains(key)) {
                report_duplicate_type(key, owner, item.range, item.name);
                continue;
            }
            handle = this->core_.state_.checked.types.named_struct(qualified, c_name, false);
        } else if (item.kind == syntax::ItemKind::enum_decl) {
            if (this->core_.state_.generics.struct_templates.contains(key)
                || this->core_.state_.generics.enum_templates.contains(key)
                || this->core_.state_.generics.type_alias_templates.contains(key)
                || this->core_.state_.checked.traits.contains(key)) {
                report_duplicate_type(key, owner, item.range, item.name);
                continue;
            }
            handle = this->core_.state_.checked.types.named_enum(qualified, c_name);
        } else if (item.kind == syntax::ItemKind::opaque_struct_decl) {
            handle = this->core_.state_.checked.types.opaque_struct(qualified, c_name);
        }

        if (!is_valid(handle)) {
            continue;
        }
        if (item_index < this->core_.state_.checked.item_c_name_ids.size()) {
            this->core_.state_.checked.item_c_name_ids[item_index] = this->core_.state_.checked.intern_c_name(c_name);
        }
        auto inserted = this->core_.state_.types.named_types.emplace(key, handle);
        if (!inserted.second) {
            report_duplicate_type(key, owner, item.range, item.name);
            continue;
        }
        this->core_.index_named_type(owner, item.name_id, handle, item.visibility);
        if (this->core_.state_.checked.type_aliases.contains(key)) {
            report_duplicate_type(key, owner, item.range, item.name);
            continue;
        }
        if (this->core_.state_.checked.traits.contains(key)) {
            report_duplicate_type(key, owner, item.range, item.name);
            continue;
        }

        if (item.kind == syntax::ItemKind::struct_decl || item.kind == syntax::ItemKind::opaque_struct_decl) {
            StructInfo info = this->core_.state_.checked.make_struct_info();
            info.name = this->core_.source_name_text(item.name_id, item.name);
            info.name_id = item.name_id;
            info.c_name = this->core_.state_.checked.intern_text(c_name);
            info.module = owner;
            info.type = handle;
            info.is_opaque = item.kind == syntax::ItemKind::opaque_struct_decl;
            info.visibility = item.visibility;
            info.stable_id = this->core_.stable_definition_id(owner, StableSymbolKind::type, item.name_id, item.name);
            info.incremental_key = this->core_.stable_incremental_key(
                info.stable_id, std::string(item.name) + std::string(SEMA_STABLE_STRUCT_INCREMENTAL_TAG));
            info.part_index = part_index;
            auto struct_inserted = this->core_.state_.checked.structs.emplace(key, std::move(info));
            if (!struct_inserted.second) {
                this->core_.report_duplicate(
                    item.range, sema_duplicate_struct_definition_message(this->core_.module_name(owner), item.name));
            } else {
                this->core_.state_.types.struct_infos_by_type[handle.value] = &struct_inserted.first->second;
            }
        }
    }
}

void SemanticAnalyzerCore::DeclarationAnalyzer::resolve_type_alias_decls()
{
    for (const auto& entry : this->core_.state_.checked.type_aliases) {
        static_cast<void>(this->core_.resolve_type_alias(entry.second, false));
    }
}

void SemanticAnalyzerCore::DeclarationAnalyzer::register_enum_cases_for_item(const syntax::ItemNode& item,
    const syntax::ModuleId owner, const TypeHandle named_enum_type, std::string enum_display_name,
    const std::string& case_prefix, const std::string& c_prefix, const syntax::Visibility visibility,
    const query::GenericInstanceKey& generic_instance_key)
{
    const base::u32 part_index = this->core_.item_part_index(this->core_.state_.flow.current_item);
    const auto make_enum_display_name = [&]() {
        if (!is_valid(named_enum_type)) {
            return enum_display_name;
        }
        const TypeInfo& enum_info = this->core_.state_.checked.types.get(named_enum_type);
        return this->core_.state_.checked.types.display_name(enum_display_name, enum_info.generic_args);
    };
    std::unordered_map<IdentId, base::SourceRange, IdentIdHash> seen_cases;
    seen_cases.reserve(item.enum_cases.size());
    for (const syntax::EnumCaseDecl& enum_case : item.enum_cases) {
        auto inserted_case = seen_cases.emplace(enum_case.name_id, enum_case.range);
        if (!inserted_case.second) {
            this->core_.report_duplicate(
                enum_case.range, sema_duplicate_enum_case_message(make_enum_display_name(), enum_case.name));
            this->core_.report_note(inserted_case.first->second, SemanticDiagnosticKind::duplicate,
                sema_previous_declaration_note_message(enum_case.name));
        }
    }

    const TypeHandle enum_type = syntax::is_valid(item.enum_base_type)
        ? this->core_.resolve_type(item.enum_base_type)
        : this->core_.state_.checked.types.builtin(BuiltinType::u32);
    if (syntax::is_valid(item.enum_base_type) && !this->core_.state_.checked.types.is_integer(enum_type)) {
        this->core_.report_general(item.range, std::string(SEMA_ENUM_BASE_INTEGER));
    }
    if (is_valid(named_enum_type)) {
        this->core_.state_.checked.types.set_enum_underlying(named_enum_type, enum_type);
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
        if (!duplicate_case_name
            && this->core_.type_member_name_exists(named_enum_type, enum_case.name_id, enum_case.name)) {
            this->core_.report_duplicate(
                enum_case.range, sema_duplicate_type_member_message(make_enum_display_name(), enum_case.name));
        }
        const std::string full_name = case_prefix + std::string(enum_case.name);
        const IdentId full_name_id = this->core_.ctx_.module.intern_identifier(full_name);
        const ModuleLookupKey enum_case_key = this->core_.module_lookup_key(owner, full_name_id);
        const bool has_payload = !enum_case.payload_types.empty() || syntax::is_valid(enum_case.payload_type);
        std::vector<TypeHandle> payload_types;
        payload_types.reserve(enum_case.payload_types.empty() ? 1 : enum_case.payload_types.size());
        if (enum_case.payload_types.empty()) {
            if (syntax::is_valid(enum_case.payload_type)) {
                payload_types.push_back(this->core_.resolve_type(enum_case.payload_type));
            }
        } else {
            for (const syntax::TypeId payload_syntax_type : enum_case.payload_types) {
                payload_types.push_back(this->core_.resolve_type(payload_syntax_type));
            }
        }

        TypeHandle payload_type = INVALID_TYPE_HANDLE;
        if (payload_types.size() == 1) {
            payload_type = payload_types.front();
        } else if (payload_types.size() > 1) {
            const std::string payload_type_name =
                this->core_.qualified_name(owner, full_name + SEMA_ENUM_SYNTHETIC_PAYLOAD_SUFFIX);
            const std::string payload_type_c_name = this->core_.c_symbol_name(
                owner, c_prefix + std::string(enum_case.name) + SEMA_ENUM_SYNTHETIC_PAYLOAD_C_SUFFIX);
            bool payload_contains_array = false;
            for (const TypeHandle field_type : payload_types) {
                payload_contains_array =
                    payload_contains_array || this->core_.state_.checked.types.contains_array(field_type);
            }
            payload_type = this->core_.state_.checked.types.named_struct(
                payload_type_name, payload_type_c_name, payload_contains_array);
            StructInfo payload_info = this->core_.state_.checked.make_struct_info();
            payload_info.name = this->core_.state_.checked.intern_text(payload_type_name);
            payload_info.c_name = this->core_.state_.checked.intern_text(payload_type_c_name);
            payload_info.module = owner;
            payload_info.type = payload_type;
            payload_info.visibility = syntax::Visibility::private_;
            payload_info.stable_id = sema::stable_definition_id(
                this->core_.stable_module_id(owner), StableSymbolKind::synthetic, payload_type_name);
            payload_info.incremental_key =
                this->core_.stable_incremental_key(payload_info.stable_id, payload_type_name);
            payload_info.part_index = part_index;
            payload_info.fields.reserve(payload_types.size());
            for (base::usize i = 0; i < payload_types.size(); ++i) {
                const std::string field_name =
                    std::string(SEMA_ENUM_SYNTHETIC_PAYLOAD_FIELD_PREFIX) + std::to_string(i);
                payload_info.fields.push_back(StructFieldInfo{
                    this->core_.state_.checked.intern_text(field_name),
                    this->core_.ctx_.module.intern_identifier(field_name),
                    this->core_.state_.checked.intern_text(field_name),
                    owner,
                    payload_types[i],
                    enum_case.range,
                    syntax::Visibility::public_,
                    sema::stable_member_key(
                        payload_info.stable_id, StableSymbolKind::struct_field, field_name, static_cast<base::u32>(i)),
                });
            }
            const auto payload_inserted = this->core_.state_.checked.structs.emplace(
                this->core_.module_lookup_key(
                    owner, this->core_.ctx_.module.intern_identifier(full_name + SEMA_ENUM_SYNTHETIC_PAYLOAD_SUFFIX)),
                std::move(payload_info));
            if (payload_inserted.second) {
                this->core_.state_.types.struct_infos_by_type[payload_type.value] = &payload_inserted.first->second;
            }
        }

        const std::string value_text =
            enum_case.value_text.empty() ? std::to_string(next_discriminant) : std::string(enum_case.value_text);
        base::u64 discriminant = next_discriminant;
        const bool parsed_discriminant = this->core_.parse_integer_literal_text(value_text, discriminant);
        if (!parsed_discriminant) {
            this->core_.report_general(enum_case.range, std::string(SEMA_ENUM_DISCRIMINANT_OUT_OF_RANGE));
        } else if (!this->core_.integer_literal_fits_type(enum_type, value_text)) {
            this->core_.report_general(enum_case.range, std::string(SEMA_ENUM_DISCRIMINANT_DOES_NOT_FIT));
        } else if (!seen_values.insert(discriminant).second) {
            this->core_.report_duplicate(
                enum_case.range, sema_duplicate_enum_discriminant_message(make_enum_display_name()));
        }
        next_discriminant = discriminant == std::numeric_limits<base::u64>::max() ? discriminant : discriminant + 1;
        if (has_payload) {
            for (const TypeHandle payload_field_type : payload_types) {
                if (!this->core_.is_valid_storage_type(payload_field_type)) {
                    this->core_.report_general(enum_case.range, std::string(SEMA_ENUM_PAYLOAD_STORAGE));
                }
                if (!this->core_.check_m2_value_abi(
                        payload_field_type, ValueAbiContext::enum_payload, enum_case.range)) {
                    contains_array_payload = true;
                }
            }
            const base::u64 case_size = this->core_.abi_size(payload_type);
            const base::u64 case_align = this->core_.abi_align(payload_type);
            if (!is_valid(payload_storage) || case_size > payload_size
                || (case_size == payload_size && case_align > payload_align)) {
                payload_storage = payload_type;
            }
            payload_size = std::max(payload_size, case_size);
            payload_align = std::max(payload_align, case_align);
        }

        EnumCaseInfo case_info = this->core_.state_.checked.make_enum_case_info();
        case_info.name = this->core_.source_name_text(full_name_id, full_name);
        case_info.name_id = full_name_id;
        case_info.c_name = this->core_.state_.checked.intern_text(
            this->core_.c_symbol_name(owner, c_prefix + std::string(enum_case.name)));
        case_info.module = owner;
        case_info.type = named_enum_type;
        case_info.payload_type = payload_type;
        case_info.payload_types = this->core_.state_.checked.copy_type_handle_list(payload_types);
        case_info.value_text = this->core_.state_.checked.intern_text(value_text);
        case_info.range = enum_case.range;
        case_info.enum_name = this->core_.source_name_text(item.name_id, enum_display_name);
        case_info.case_name = this->core_.source_name_text(enum_case.name_id, enum_case.name);
        case_info.case_name_id = enum_case.name_id;
        case_info.visibility = visibility;
        case_info.stable_id =
            sema::stable_definition_id(this->core_.stable_module_id(owner), StableSymbolKind::enum_case, full_name);
        case_info.stable_case_key = this->core_.stable_member_key(
            this->core_.stable_definition_id(owner, StableSymbolKind::type, item.name_id, item.name),
            StableSymbolKind::enum_case, enum_case.name_id, enum_case.name);
        case_info.incremental_key = this->core_.stable_incremental_key(case_info.stable_id, value_text);
        case_info.generic_instance_key = generic_instance_key;
        case_info.part_index = part_index;
        const auto case_inserted = this->core_.state_.checked.enum_cases.emplace(enum_case_key, std::move(case_info));
        if (!case_inserted.second) {
            this->core_.report_duplicate(
                enum_case.range, sema_duplicate_enum_case_message(make_enum_display_name(), enum_case.name));
            this->core_.report_note(case_inserted.first->second.range, SemanticDiagnosticKind::duplicate,
                sema_previous_declaration_note_message(enum_case.name));
            continue;
        }
        this->core_.index_enum_case(case_inserted.first->second);
    }
    if (is_valid(named_enum_type) && is_valid(payload_storage)) {
        this->core_.state_.checked.types.set_enum_payload_layout(named_enum_type,
            payload_storage_type(
                this->core_.state_.checked.types, payload_size, payload_align, this->core_.ctx_.options.target_layout),
            payload_size, payload_align);
    }
    if (is_valid(named_enum_type)) {
        this->core_.state_.checked.types.set_record_contains_array(named_enum_type, contains_array_payload);
    }
}

void SemanticAnalyzerCore::DeclarationAnalyzer::register_value_names()
{
    FunctionRegistry functions(this->core_.state_.checked, this->core_.state_.functions.global_values,
        this->core_.ctx_.diagnostics,
        this->core_.owned_module_.has_value() ? nullptr : &this->core_.ctx_.module.identifiers);
    for (base::u32 item_index = 0; item_index < this->core_.ctx_.module.items.size(); ++item_index) {
        const syntax::ItemNode item = this->core_.ctx_.module.items[item_index];
        if (this->core_.is_trait_requirement_item(syntax::ItemId{item_index})) {
            continue;
        }
        if (this->core_.has_generic_params(item)) {
            continue;
        }
        this->core_.state_.flow.current_module = this->core_.item_module(syntax::ItemId{item_index});
        this->core_.state_.flow.current_item = syntax::ItemId{item_index};
        const ModuleLookupKey item_type_key =
            this->core_.module_lookup_key(this->core_.state_.flow.current_module, item.name_id);
        FunctionLookupKey key = this->core_.function_lookup_key(this->core_.state_.flow.current_module, item.name_id);
        std::string c_name = this->core_.c_symbol_name(this->core_.state_.flow.current_module, item.name);
        if (item.kind == syntax::ItemKind::fn_decl) {
            const bool is_method = syntax::is_valid(item.impl_type);
            const bool is_trait_impl_method = is_method && syntax::is_valid(item.trait_type);
            const std::string trait_impl_method_key =
                is_trait_impl_method ? this->core_.trait_impl_method_key_name(item) : std::string{};
            TypeHandle method_owner_type = INVALID_TYPE_HANDLE;
            if (!is_method && this->core_.state_.generics.function_templates.contains(item_type_key)) {
                this->core_.report_duplicate(item.range,
                    sema_duplicate_function_definition_message(
                        this->core_.module_name(this->core_.state_.flow.current_module), item.name));
                continue;
            }
            if (item.is_variadic && !item.is_extern_c) {
                this->core_.report_general(item.range, std::string(SEMA_VARIADIC_EXTERN_C_ONLY));
            }
            if (is_method) {
                method_owner_type = this->core_.resolve_type(item.impl_type);
                if (is_valid(method_owner_type)) {
                    const TypeKind owner_kind = this->core_.state_.checked.types.get(method_owner_type).kind;
                    if (owner_kind != TypeKind::struct_ && owner_kind != TypeKind::enum_
                        && owner_kind != TypeKind::opaque_struct) {
                        this->core_.report_general(item.range, std::string(SEMA_IMPL_TARGET_NAMED_TYPE));
                    }
                }
                key = is_trait_impl_method
                    ? this->core_.method_function_lookup_key(this->core_.state_.flow.current_module, method_owner_type,
                          this->core_.intern_generated_key(trait_impl_method_key))
                    : this->core_.method_function_lookup_key(
                          this->core_.state_.flow.current_module, method_owner_type, item.name_id);
                c_name = is_trait_impl_method
                    ? this->core_.trait_impl_method_c_symbol_name(method_owner_type, trait_impl_method_key, item.name)
                    : this->core_.method_c_symbol_name(method_owner_type, item.name);
            }
            const bool has_explicit_return = syntax::is_valid(item.return_type);
            TypeHandle return_type = INVALID_TYPE_HANDLE;
            if (has_explicit_return) {
                return_type = this->core_.resolve_type(item.return_type);
            } else if (item.is_extern_c || item.is_export_c) {
                this->core_.report_general(item.range, std::string(SEMA_C_ABI_RETURN_TYPE_EXPLICIT));
                return_type = this->core_.state_.checked.types.builtin(BuiltinType::void_);
            } else if (item.is_prototype) {
                this->core_.report_general(item.range, std::string(SEMA_PROTOTYPE_RETURN_TYPE_EXPLICIT));
                return_type = this->core_.state_.checked.types.builtin(BuiltinType::void_);
            } else if (syntax::visibility_is_public(item.visibility)) {
                this->core_.report_general(item.range, std::string(SEMA_PUBLIC_FUNCTION_RETURN_TYPE_EXPLICIT));
            }
            std::vector<TypeHandle> param_types;
            param_types.reserve(item.params.size());
            std::vector<FunctionParamInfo> params;
            params.reserve(item.params.size());
            bool saw_default_param = false;
            for (const syntax::ParamDecl& param : item.params) {
                TypeHandle param_type = this->core_.resolve_type(param.type);
                if (!this->core_.is_valid_storage_type(param_type)) {
                    this->core_.report_general(param.range, std::string(SEMA_FUNCTION_PARAMETER_STORAGE));
                }
                static_cast<void>(this->core_.check_m2_value_abi(param_type, ValueAbiContext::parameter, param.range));
                const bool has_default = syntax::is_valid(param.default_value);
                if (!has_default && saw_default_param) {
                    this->core_.report_general(param.range, std::string(SEMA_DEFAULT_PARAMETER_AFTER_REQUIRED));
                }
                if (has_default) {
                    saw_default_param = true;
                    if (item.is_extern_c || item.is_export_c) {
                        this->core_.report_general(param.range, std::string(SEMA_DEFAULT_PARAMETER_C_ABI));
                    }
                    if (item.is_variadic) {
                        this->core_.report_general(param.range, std::string(SEMA_DEFAULT_PARAMETER_VARIADIC));
                    }
                    const TypeHandle default_type = this->core_.analyze_expr(param.default_value, param_type);
                    if (!this->core_.can_assign(param_type, default_type, param.default_value)) {
                        this->core_.report_type_mismatch(
                            this->core_.ctx_.module.exprs.range(param.default_value.value),
                            sema_argument_type_message(item.name), param_type, default_type);
                    }
                }
                params.push_back(FunctionParamInfo{
                    this->core_.source_name_text(param.name_id, param.name),
                    param.name_id,
                    param.default_value,
                    param.range,
                });
                param_types.push_back(param_type);
            }
            if (is_method) {
                bool saw_self = false;
                for (base::usize i = 0; i < item.params.size(); ++i) {
                    if (item.params[i].name != "self") {
                        continue;
                    }
                    if (i != 0) {
                        this->core_.report_general(item.params[i].range, std::string(SEMA_METHOD_SELF_FIRST));
                    }
                    saw_self = true;
                }
                if (saw_self && !param_types.empty() && is_valid(method_owner_type)) {
                    TypeHandle self_type = param_types.front();
                    if (this->core_.state_.checked.types.is_pointer(self_type)
                        || this->core_.state_.checked.types.is_reference(self_type)) {
                        self_type = this->core_.state_.checked.types.get(self_type).pointee;
                    }
                    if (!this->core_.state_.checked.types.same(self_type, method_owner_type)) {
                        this->core_.report_general(item.params.front().range, std::string(SEMA_METHOD_SELF_TYPE));
                    }
                }
                if (!is_trait_impl_method
                    && this->core_.type_member_name_exists(method_owner_type, item.name_id, item.name)) {
                    this->core_.report_duplicate(item.range,
                        sema_duplicate_type_member_message(
                            this->core_.state_.checked.types.display_name(method_owner_type), item.name));
                }
            }
            if (has_explicit_return && is_valid(return_type)) {
                this->core_.validate_function_return_type(item, return_type);
            }
            std::string stable_function_name(item.name);
            StableSymbolKind stable_function_kind = StableSymbolKind::function;
            if (is_method && is_valid(method_owner_type)) {
                stable_function_name = is_trait_impl_method
                    ? this->core_.state_.checked.types.display_name(method_owner_type) + "." + trait_impl_method_key
                    : this->core_.state_.checked.types.display_name(method_owner_type) + "." + std::string(item.name);
                stable_function_kind = StableSymbolKind::method;
            }
            const StableDefId stable_id =
                sema::stable_definition_id(this->core_.stable_module_id(this->core_.state_.flow.current_module),
                    stable_function_kind, stable_function_name);
            const IncrementalKey incremental_key = this->core_.stable_incremental_key(stable_id,
                this->core_.function_incremental_fingerprint(
                    stable_function_name, return_type, param_types, is_method, item.is_variadic));
            IdentId trait_name_id = INVALID_IDENT_ID;
            if (is_trait_impl_method && item.trait_type.value < this->core_.ctx_.module.types.size()) {
                trait_name_id = this->core_.ctx_.module.types[item.trait_type.value].name_id;
            }
            FunctionRegistrationRequest request(item);
            request.owner = this->core_.state_.flow.current_module;
            request.key = key;
            request.c_name = c_name;
            request.method_owner_type = method_owner_type;
            request.trait_module =
                is_trait_impl_method ? this->core_.state_.flow.current_module : syntax::INVALID_MODULE_ID;
            request.trait_name_id = trait_name_id;
            request.return_type = return_type;
            request.param_types = std::span<const TypeHandle>{param_types.data(), param_types.size()};
            request.params = std::span<const FunctionParamInfo>{params.data(), params.size()};
            request.item_id = syntax::ItemId{item_index};
            request.part_index = this->core_.item_part_index(syntax::ItemId{item_index});
            request.stable_id = stable_id;
            request.incremental_key = incremental_key;
            request.is_trait_impl_method = is_trait_impl_method;
            functions.register_function(request);
            if (const auto found = this->core_.state_.checked.functions.find(key);
                found != this->core_.state_.checked.functions.end()) {
                this->core_.index_function_lookup(found->second);
                this->core_.index_function_value(found->second);
                this->core_.record_declared_borrow_contract(item, key, found->second);
                if (item.is_prototype || item.is_extern_c || !syntax::is_valid(item.body)) {
                    this->core_.analyze_signature_lifetimes(item, key, found->second);
                }
            }
            if (!item.is_prototype && !item.is_extern_c) {
                this->core_.state_.functions.definition_items[key] = syntax::ItemId{item_index};
            }
            this->core_.state_.functions.body_states[key] = FunctionBodyState::not_started;
        } else if (item.kind == syntax::ItemKind::const_decl) {
            TypeHandle type = this->core_.resolve_type(item.const_type);
            if (item_index < this->core_.state_.checked.item_c_name_ids.size()) {
                this->core_.state_.checked.item_c_name_ids[item_index] =
                    this->core_.state_.checked.intern_c_name(c_name);
            }
            const auto inserted = this->core_.state_.functions.global_values.emplace(key,
                Symbol{
                    SymbolKind::const_,
                    this->core_.source_name_text(item.name_id, item.name),
                    item.name_id,
                    this->core_.state_.checked.intern_text(c_name),
                    this->core_.state_.flow.current_module,
                    type,
                    item.range,
                    false,
                    item.visibility,
                    this->core_.stable_definition_id(
                        this->core_.state_.flow.current_module, StableSymbolKind::value, item.name_id, item.name),
                });
            if (!inserted.second) {
                this->core_.report_duplicate(item.range,
                    sema_duplicate_value_definition_message(
                        this->core_.module_name(this->core_.state_.flow.current_module), item.name));
                this->core_.report_note(inserted.first->second.range, SemanticDiagnosticKind::duplicate,
                    sema_previous_declaration_note_message(item.name));
            } else {
                this->core_.index_global_value(inserted.first->second);
            }
        } else if (item.kind == syntax::ItemKind::enum_decl) {
            const auto type_found = this->core_.state_.types.named_types.find(item_type_key);
            this->core_.register_enum_cases_for_item(item, this->core_.state_.flow.current_module,
                type_found == this->core_.state_.types.named_types.end() ? INVALID_TYPE_HANDLE : type_found->second,
                std::string(item.name), std::string(item.name) + "_", std::string(item.name) + "_", item.visibility,
                query::GenericInstanceKey{});
        }
    }
    this->core_.state_.flow.current_module = syntax::INVALID_MODULE_ID;
    this->core_.state_.flow.current_item = syntax::INVALID_ITEM_ID;
}

void SemanticAnalyzerCore::DeclarationAnalyzer::validate_function_prototypes() const
{
    for (const auto& entry : this->core_.state_.checked.functions) {
        const FunctionSignature& signature = entry.second;
        if (signature.is_extern_c) {
            continue;
        }
        if (signature.has_conflict) {
            continue;
        }
        if (signature.has_prototype && !signature.has_definition) {
            this->core_.report_duplicate(
                signature.range, sema_function_prototype_missing_definition_message(signature.name));
        }
    }
}

std::optional<SemanticAnalyzerCore::DeclarationAnalyzer::ExportSurfaceRestrictedType>
SemanticAnalyzerCore::DeclarationAnalyzer::restricted_type_exposed_by_surface_type(
    const TypeHandle root, const syntax::Visibility surface_visibility) const
{
    if (!is_valid(root)) {
        return std::nullopt;
    }

    std::vector<TypeHandle> pending;
    std::unordered_set<base::u32> visited;
    pending.reserve(SEMA_EXPORT_SURFACE_INITIAL_STACK_CAPACITY);
    visited.reserve(SEMA_EXPORT_SURFACE_INITIAL_STACK_CAPACITY);
    pending.push_back(root);
    const VisibilityPolicy policy;

    while (!pending.empty()) {
        const TypeHandle current = pending.back();
        pending.pop_back();
        if (!is_valid(current) || !visited.insert(current.value).second) {
            continue;
        }

        const TypeInfo& info = this->core_.state_.checked.types.get(current);
        switch (info.kind) {
            case TypeKind::builtin:
            case TypeKind::generic_param:
                break;
            case TypeKind::associated_projection:
                pending.push_back(info.associated_base);
                break;
            case TypeKind::pointer:
            case TypeKind::reference:
                pending.push_back(info.pointee);
                break;
            case TypeKind::array:
                pending.push_back(info.array_element);
                break;
            case TypeKind::slice:
                pending.push_back(info.slice_element);
                break;
            case TypeKind::tuple:
                for (const TypeHandle element : info.tuple_elements) {
                    pending.push_back(element);
                }
                break;
            case TypeKind::function:
                pending.push_back(info.function_return);
                for (const TypeHandle param : info.function_params) {
                    pending.push_back(param);
                }
                break;
            case TypeKind::struct_:
            case TypeKind::opaque_struct:
                if (const StructInfo* const struct_info = this->core_.find_struct(current);
                    struct_info != nullptr && !policy.can_expose_type(surface_visibility, struct_info->visibility)) {
                    return ExportSurfaceRestrictedType{
                        this->core_.state_.checked.types.display_name(current),
                        {},
                        struct_info->visibility,
                    };
                }
                for (const TypeHandle arg : info.generic_args) {
                    pending.push_back(arg);
                }
                break;
            case TypeKind::trait_object:
                if (const auto found = this->core_.state_.checked.traits.find(ModuleLookupKey{
                        info.trait_object_module.value,
                        info.trait_object_name_id,
                    });
                    found != this->core_.state_.checked.traits.end()
                    && !policy.can_expose_type(surface_visibility, found->second.visibility)) {
                    return ExportSurfaceRestrictedType{
                        this->core_.state_.checked.types.display_name(current),
                        found->second.range,
                        found->second.visibility,
                    };
                }
                for (const TypeHandle arg : info.trait_object_args) {
                    pending.push_back(arg);
                }
                for (const TraitObjectAssociatedTypeEquality& equality :
                    info.trait_object_associated_equalities) {
                    pending.push_back(equality.value_type);
                }
                break;
            case TypeKind::enum_: {
                if (const auto found = this->core_.state_.names.enum_cases_by_type.find(current.value);
                    found != this->core_.state_.names.enum_cases_by_type.end() && !found->second.empty()
                    && found->second.front() != nullptr) {
                    const EnumCaseInfo& enum_case = *found->second.front();
                    if (!policy.can_expose_type(surface_visibility, enum_case.visibility)) {
                        return ExportSurfaceRestrictedType{
                            this->core_.state_.checked.types.display_name(current),
                            enum_case.range,
                            enum_case.visibility,
                        };
                    }
                }
                for (const TypeHandle arg : info.generic_args) {
                    pending.push_back(arg);
                }
                break;
            }
        }
    }

    return std::nullopt;
}

bool SemanticAnalyzerCore::DeclarationAnalyzer::method_signature_is_exported_surface(
    const FunctionSignature& signature) const
{
    if (!signature.is_method) {
        return true;
    }
    const std::optional<ExportSurfaceRestrictedType> restricted_receiver =
        this->restricted_type_exposed_by_surface_type(signature.method_owner_type, signature.visibility);
    return !restricted_receiver.has_value();
}

void SemanticAnalyzerCore::DeclarationAnalyzer::validate_exported_signature_surfaces() const
{
    const auto report_if_restricted = [&](const syntax::Visibility surface_visibility, const std::string_view surface,
                                          const std::string_view name, const TypeHandle type,
                                          const base::SourceRange& range) {
        const std::optional<ExportSurfaceRestrictedType> restricted_type =
            this->restricted_type_exposed_by_surface_type(type, surface_visibility);
        if (!restricted_type.has_value()) {
            return;
        }
        this->core_.report_visibility(range,
            sema_export_surface_exposes_restricted_type_message(
                surface_visibility, surface, name, restricted_type->visibility, restricted_type->name));
        if (!restricted_type->range.empty()) {
            this->core_.report_note(restricted_type->range, SemanticDiagnosticKind::visibility,
                sema_previous_declaration_note_message(restricted_type->name));
        }
    };

    for (const auto& entry : this->core_.state_.checked.functions) {
        const FunctionSignature& signature = entry.second;
        if (!visibility_has_export_surface(signature.visibility) || signature.has_conflict
            || !this->method_signature_is_exported_surface(signature)) {
            continue;
        }
        const std::string display_name = function_surface_name(this->core_.state_.checked.types, signature);
        const std::string_view surface =
            signature.is_method ? std::string_view{"method"} : std::string_view{"function"};
        report_if_restricted(signature.visibility, surface, display_name, signature.return_type, signature.range);
        for (const TypeHandle param_type : signature.param_types) {
            report_if_restricted(signature.visibility, surface, display_name, param_type, signature.range);
        }
    }

    for (const auto& entry : this->core_.state_.generics.placeholder_functions) {
        const FunctionSignature& signature = entry.second;
        if (!visibility_has_export_surface(signature.visibility) || signature.has_conflict
            || !this->method_signature_is_exported_surface(signature)) {
            continue;
        }
        const std::string display_name = function_surface_name(this->core_.state_.checked.types, signature);
        const std::string_view surface =
            signature.is_method ? std::string_view{"method"} : std::string_view{"function"};
        report_if_restricted(signature.visibility, surface, display_name, signature.return_type, signature.range);
        for (const TypeHandle param_type : signature.param_types) {
            report_if_restricted(signature.visibility, surface, display_name, param_type, signature.range);
        }
    }

    for (const auto& entry : this->core_.state_.checked.structs) {
        const StructInfo& info = entry.second;
        if (!visibility_has_export_surface(info.visibility)) {
            continue;
        }
        for (const StructFieldInfo& field : info.fields) {
            const syntax::Visibility field_surface = syntax::effective_visibility(info.visibility, field.visibility);
            if (!visibility_has_export_surface(field_surface)) {
                continue;
            }
            const std::string field_name =
                struct_field_surface_name(this->core_.state_.checked.types, info, field);
            report_if_restricted(field_surface, "struct field", field_name, field.type, field.range);
        }
    }

    for (const auto& entry : this->core_.state_.checked.enum_cases) {
        const EnumCaseInfo& info = entry.second;
        if (!visibility_has_export_surface(info.visibility)) {
            continue;
        }
        const std::string case_name = enum_case_surface_name(this->core_.state_.checked.types, info);
        for (const TypeHandle payload_type : info.payload_types) {
            report_if_restricted(info.visibility, "enum case", case_name, payload_type, info.range);
        }
        if (info.payload_types.empty()) {
            report_if_restricted(info.visibility, "enum case", case_name, info.payload_type, info.range);
        }
    }

    for (const auto& entry : this->core_.state_.checked.type_aliases) {
        const TypeAliasInfo& info = entry.second;
        if (!visibility_has_export_surface(info.visibility)) {
            continue;
        }
        const std::optional<TypeHandle> target = cached_checked_syntax_type(this->core_.state_.checked, info.target);
        report_if_restricted(info.visibility, "type alias", info.name,
            target.has_value() ? *target : this->core_.resolve_type_alias(info, false), info.range);
    }

    for (const auto& entry : this->core_.state_.functions.global_values) {
        const Symbol& symbol = entry.second;
        if (symbol.kind != SymbolKind::const_ || !visibility_has_export_surface(symbol.visibility)) {
            continue;
        }
        report_if_restricted(symbol.visibility, "const", symbol.name, symbol.type, symbol.range);
    }
}

void SemanticAnalyzerCore::DeclarationAnalyzer::validate_abi_symbols() const
{
    std::unordered_map<std::string_view, AbiSymbolInfo> symbols;
    symbols.reserve(this->core_.state_.checked.functions.size() + this->core_.state_.functions.global_values.size());

    const auto same_function_type = [&](const AbiFunctionInfo& lhs, const AbiFunctionInfo& rhs) {
        if (!this->core_.state_.checked.types.same(lhs.return_type, rhs.return_type)
            || lhs.is_variadic != rhs.is_variadic || lhs.param_types.size() != rhs.param_types.size()) {
            return false;
        }
        for (base::usize i = 0; i < lhs.param_types.size(); ++i) {
            if (!this->core_.state_.checked.types.same(lhs.param_types[i], rhs.param_types[i])) {
                return false;
            }
        }
        return true;
    };

    const auto report_previous_abi_declaration = [&](const std::string_view symbol, const AbiSymbolInfo& prior) {
        this->core_.report_note(
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
                this->core_.report_type(function.range, sema_extern_c_abi_conflict_message(symbol));
                report_previous_abi_declaration(symbol, prior);
            }
            return;
        }
        this->core_.report_duplicate(function.range, sema_duplicate_abi_symbol_message(symbol));
        report_previous_abi_declaration(symbol, prior);
    };

    for (const auto& entry : this->core_.state_.checked.functions) {
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

    for (const auto& entry : this->core_.state_.functions.global_values) {
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
        this->core_.report_duplicate(symbol.range, sema_duplicate_abi_symbol_message(symbol.c_name));
        report_previous_abi_declaration(symbol_name, found->second);
    }
}

void SemanticAnalyzerCore::DeclarationAnalyzer::analyze_entry_points() const
{
    constexpr syntax::ModuleId root_module{0};
    const FunctionSignature* aurex_entry = nullptr;
    const FunctionSignature* c_entry = nullptr;

    for (const auto& entry : this->core_.state_.checked.functions) {
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
        this->core_.report_general(aurex_entry->range, std::string(SEMA_ORDINARY_MAIN_EXPORTED_C_MAIN));
    }
    if (aurex_entry->c_name == "main") {
        this->core_.report_general(aurex_entry->range, std::string(SEMA_ORDINARY_MAIN_ABI_NAME));
    }
    const TypeHandle i32_type = this->core_.state_.checked.types.builtin(BuiltinType::i32);
    const TypeHandle void_type = this->core_.state_.checked.types.builtin(BuiltinType::void_);
    if (aurex_entry->param_types.empty()) {
        // fn main() -> i32
    } else if (aurex_entry->param_types.size() == 2) {
        if (!this->core_.state_.checked.types.same(aurex_entry->param_types[0], i32_type)
            || !is_main_argv_type(this->core_.state_.checked.types, aurex_entry->param_types[1])) {
            this->core_.report_general(aurex_entry->range, std::string(SEMA_MAIN_PARAMETERS_EXACT));
        }
    } else {
        this->core_.report_general(aurex_entry->range, std::string(SEMA_MAIN_PARAMETERS));
    }
    if (!this->core_.state_.checked.types.same(aurex_entry->return_type, i32_type)
        && !this->core_.state_.checked.types.same(aurex_entry->return_type, void_type)) {
        this->core_.report_general(aurex_entry->range, std::string(SEMA_MAIN_RETURN));
    }
}

void SemanticAnalyzerCore::DeclarationAnalyzer::analyze_struct_properties()
{
    for (base::u32 index = 0; index < this->core_.ctx_.module.items.size(); ++index) {
        if (this->core_.ctx_.module.items.kind(index) != syntax::ItemKind::struct_decl) {
            continue;
        }
        const syntax::ItemNode item = this->core_.ctx_.module.items[index];
        if (this->core_.has_generic_params(item)) {
            continue;
        }
        this->core_.state_.flow.current_module = this->core_.item_module(syntax::ItemId{index});
        this->core_.state_.flow.current_item = syntax::ItemId{index};
        const ModuleLookupKey key = this->core_.module_lookup_key(this->core_.state_.flow.current_module, item.name_id);
        bool contains_array = false;
        std::unordered_map<IdentId, base::SourceRange, IdentIdHash> seen_fields;
        seen_fields.reserve(item.fields.size());
        StructInfo* struct_info = nullptr;
        if (const auto struct_found = this->core_.state_.checked.structs.find(key);
            struct_found != this->core_.state_.checked.structs.end()) {
            struct_info = &struct_found->second;
            struct_info->fields.reserve(item.fields.size());
        }
        for (const syntax::FieldDecl& field : item.fields) {
            const auto inserted_field = seen_fields.emplace(field.name_id, field.range);
            if (!inserted_field.second) {
                this->core_.report_duplicate(field.range, sema_duplicate_struct_field_message(field.name));
                this->core_.report_note(inserted_field.first->second, SemanticDiagnosticKind::duplicate,
                    sema_previous_declaration_note_message(field.name));
                continue;
            }
            const TypeHandle field_type = this->core_.resolve_type(field.type);
            if (!this->core_.is_valid_storage_type(field_type)) {
                this->core_.report_general(field.range, std::string(SEMA_FIELD_STORAGE));
            }
            if (struct_info != nullptr) {
                struct_info->fields.push_back(StructFieldInfo{
                    this->core_.source_name_text(field.name_id, field.name),
                    field.name_id,
                    {},
                    this->core_.state_.flow.current_module,
                    field_type,
                    field.range,
                    field.visibility,
                    this->core_.stable_member_key(
                        struct_info->stable_id, StableSymbolKind::struct_field, field.name_id, field.name),
                });
            }
            if (this->core_.state_.checked.types.contains_array(field_type)) {
                contains_array = true;
            }
        }
        const auto found = this->core_.state_.types.named_types.find(key);
        if (found != this->core_.state_.types.named_types.end()) {
            this->core_.state_.checked.types.set_record_contains_array(found->second, contains_array);
        }
    }
    this->core_.state_.flow.current_module = syntax::INVALID_MODULE_ID;
    this->core_.state_.flow.current_item = syntax::INVALID_ITEM_ID;
}

void SemanticAnalyzerCore::DeclarationAnalyzer::analyze_derive_attributes()
{
    for (base::u32 index = 0; index < this->core_.ctx_.module.items.size(); ++index) {
        const syntax::ItemNode item = this->core_.ctx_.module.items[index];
        if (item.derives.empty()) {
            continue;
        }
        const syntax::ItemId item_id{index};
        this->core_.state_.flow.current_module = this->core_.item_module(item_id);
        this->core_.state_.flow.current_item = item_id;
        TypeHandle type = INVALID_TYPE_HANDLE;
        if (item.kind == syntax::ItemKind::struct_decl || item.kind == syntax::ItemKind::enum_decl
            || item.kind == syntax::ItemKind::opaque_struct_decl) {
            const ModuleLookupKey key =
                this->core_.module_lookup_key(this->core_.state_.flow.current_module, item.name_id);
            if (const auto found = this->core_.state_.types.named_types.find(key);
                found != this->core_.state_.types.named_types.end()) {
                type = found->second;
            }
        }
        const bool generic_type_item = (item.kind == syntax::ItemKind::struct_decl
                                           || item.kind == syntax::ItemKind::enum_decl
                                           || item.kind == syntax::ItemKind::type_alias)
            && this->core_.has_generic_params(item);
        this->analyze_derive_attributes_for_item(item, type, true, !generic_type_item);
    }
    this->core_.state_.flow.current_module = syntax::INVALID_MODULE_ID;
    this->core_.state_.flow.current_item = syntax::INVALID_ITEM_ID;
}

void SemanticAnalyzerCore::DeclarationAnalyzer::analyze_derive_attributes_for_item(const syntax::ItemNode& item,
    const TypeHandle type, const bool report_invalid_attributes, const bool report_component_failures)
{
    if (item.derives.empty()) {
        return;
    }
    const bool supported_target = item.kind == syntax::ItemKind::struct_decl || item.kind == syntax::ItemKind::enum_decl;
    if (!supported_target) {
        if (report_invalid_attributes) {
            for (const syntax::DeriveDecl& derive : item.derives) {
                this->core_.report_capability(derive.range, std::string(SEMA_DERIVE_TARGET));
            }
        }
        return;
    }

    std::unordered_map<CapabilityKind, base::SourceRange, CapabilityKindHash> seen;
    seen.reserve(item.derives.size());
    for (const syntax::DeriveDecl& derive : item.derives) {
        const std::optional<CapabilityKind> capability = parse_derive_capability(derive.name);
        if (!capability.has_value()) {
            if (report_invalid_attributes) {
                this->core_.report_capability(derive.range, sema_unsupported_derive_message(derive.name));
            }
            continue;
        }
        const auto inserted = seen.emplace(*capability, derive.range);
        if (!inserted.second) {
            if (report_invalid_attributes) {
                this->core_.report_capability(derive.range, sema_duplicate_derive_message(derive.name));
                this->core_.report_note(inserted.first->second, SemanticDiagnosticKind::duplicate,
                    sema_previous_declaration_note_message(derive.name));
            }
            continue;
        }
        if (!is_valid(type)) {
            continue;
        }

        bool components_ok = true;
        const std::string_view capability_name_text = capability_name(*capability);
        if (item.kind == syntax::ItemKind::struct_decl) {
            const auto struct_info = this->core_.state_.types.struct_infos_by_type.find(type.value);
            if (struct_info != this->core_.state_.types.struct_infos_by_type.end()) {
                for (const StructFieldInfo& field : struct_info->second->fields) {
                    if (!this->core_.type_satisfies_capability(field.type, *capability)) {
                        components_ok = false;
                        if (report_component_failures) {
                            this->core_.report_capability(
                                field.range, sema_derive_field_capability_message(capability_name_text, field.name));
                        }
                    }
                }
            }
        } else if (item.kind == syntax::ItemKind::enum_decl) {
            for (const auto& entry : this->core_.state_.checked.enum_cases) {
                const EnumCaseInfo& enum_case = entry.second;
                if (!this->core_.state_.checked.types.same(enum_case.type, type)) {
                    continue;
                }
                for (const TypeHandle payload : enum_case.payload_types) {
                    if (!this->core_.type_satisfies_capability(payload, *capability)) {
                        components_ok = false;
                        if (report_component_failures) {
                            this->core_.report_capability(enum_case.range,
                                sema_derive_enum_payload_capability_message(
                                    capability_name_text, enum_case.case_name));
                        }
                    }
                }
            }
        }

        if (*capability == CapabilityKind::copy
            && !resource_is_copy(ResourceSemanticsClassifier(this->core_.state_.checked,
                                    [this](const TypeHandle param) {
                                        return this->core_.generic_param_has_capability(param, CapabilityKind::copy);
                                    })
                                    .classify(type))) {
            components_ok = false;
            if (report_component_failures) {
                this->core_.report_capability(derive.range, sema_derive_type_capability_message(capability_name_text));
            }
        }
        if (!components_ok) {
            continue;
        }

        DerivedCapabilityList& derived = this->core_.state_.checked.derived_capabilities_by_type[type.value];
        if (derived.empty()) {
            derived = this->core_.state_.checked.make_derived_capability_list();
        }
        const bool already_recorded = std::ranges::any_of(derived, [capability](const DerivedCapabilityInfo& info) {
            return info.capability == *capability;
        });
        if (!already_recorded) {
            derived.push_back(DerivedCapabilityInfo{*capability, derive.range});
        }
    }
}

void SemanticAnalyzerCore::DeclarationAnalyzer::analyze_const_decls()
{
    SemaMap<ModuleLookupKey, ModuleLookupList, ModuleLookupKeyHash> dependencies_by_const =
        make_sema_map<ModuleLookupKey, ModuleLookupList, ModuleLookupKeyHash>(
            *this->core_.state_.arena, ModuleLookupKeyHash{});
    SemaMap<ModuleLookupKey, base::SourceRange, ModuleLookupKeyHash> const_ranges =
        make_sema_map<ModuleLookupKey, base::SourceRange, ModuleLookupKeyHash>(
            *this->core_.state_.arena, ModuleLookupKeyHash{});
    SemaMap<ModuleLookupKey, InternedText, ModuleLookupKeyHash> const_names =
        make_sema_map<ModuleLookupKey, InternedText, ModuleLookupKeyHash>(
            *this->core_.state_.arena, ModuleLookupKeyHash{});
    dependencies_by_const.reserve(this->core_.ctx_.module.items.size());
    const_ranges.reserve(this->core_.ctx_.module.items.size());
    const_names.reserve(this->core_.ctx_.module.items.size());

    for (base::u32 index = 0; index < this->core_.ctx_.module.items.size(); ++index) {
        if (this->core_.ctx_.module.items.kind(index) != syntax::ItemKind::const_decl) {
            continue;
        }
        const syntax::ItemNode item = this->core_.ctx_.module.items[index];
        this->core_.state_.flow.current_module = this->core_.item_module(syntax::ItemId{index});
        this->core_.state_.flow.current_item = syntax::ItemId{index};
        const IdentId const_name_id =
            is_valid(item.name_id) ? item.name_id : this->core_.ctx_.module.identifiers.find(item.name);
        const ModuleLookupKey const_key = const_dependency_key(this->core_.state_.flow.current_module, const_name_id);
        if (!is_valid(const_key)) {
            continue;
        }
        const_ranges[const_key] = item.range;
        const_names[const_key] = this->core_.state_.checked.intern_text(
            this->core_.qualified_name(this->core_.state_.flow.current_module, item.name));
        const TypeHandle declared = this->core_.resolve_type(item.const_type);
        const bool previous_const_initializer = this->core_.state_.flow.in_const_initializer;
        this->core_.state_.flow.in_const_initializer = true;
        const TypeHandle actual = this->core_.analyze_expr(item.const_value, declared);
        this->core_.state_.flow.in_const_initializer = previous_const_initializer;
        ModuleLookupSet dependencies =
            make_sema_set<ModuleLookupKey, ModuleLookupKeyHash>(*this->core_.state_.arena, ModuleLookupKeyHash{});
        if (!this->core_.is_const_evaluable_expr(item.const_value, dependencies)) {
            const base::SourceRange range =
                syntax::is_valid(item.const_value) && item.const_value.value < this->core_.ctx_.module.exprs.size()
                ? this->core_.ctx_.module.exprs.range(item.const_value.value)
                : item.range;
            this->core_.report_general(range, std::string(SEMA_CONST_NOT_COMPILE_TIME));
        }
        ModuleLookupList dependency_list = make_sema_vector<ModuleLookupKey>(*this->core_.state_.arena);
        dependency_list.reserve(dependencies.size());
        dependency_list.insert(dependency_list.end(), dependencies.begin(), dependencies.end());
        if (const auto found = dependencies_by_const.find(const_key); found != dependencies_by_const.end()) {
            found->second = std::move(dependency_list);
        } else {
            dependencies_by_const.emplace(const_key, std::move(dependency_list));
        }
        if (!this->core_.is_valid_storage_type(declared)) {
            this->core_.report_general(item.range, std::string(SEMA_CONST_TYPE_STORAGE));
        }
        if (!this->core_.can_assign(declared, actual, item.const_value)) {
            this->core_.report_type_mismatch(item.range, std::string(SEMA_CONST_TYPE_MISMATCH), declared, actual);
        }
    }

    constexpr base::u8 SEMA_CONST_DEP_STATE_VISITING = static_cast<base::u8>(ConstDependencyState::VISITING);
    constexpr base::u8 SEMA_CONST_DEP_STATE_VISITED = static_cast<base::u8>(ConstDependencyState::VISITED);

    SemaMap<ModuleLookupKey, base::u8, ModuleLookupKeyHash> states =
        make_sema_map<ModuleLookupKey, base::u8, ModuleLookupKeyHash>(*this->core_.state_.arena, ModuleLookupKeyHash{});
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
                    ? this->core_.ctx_.module.identifiers.text(frame.key.name)
                    : name->second.view();
                this->core_.report_general(range == const_ranges.end() ? base::SourceRange{} : range->second,
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
    this->core_.state_.flow.current_module = syntax::INVALID_MODULE_ID;
    this->core_.state_.flow.current_item = syntax::INVALID_ITEM_ID;
}

bool SemanticAnalyzerCore::DeclarationAnalyzer::is_const_evaluable_expr(
    const syntax::ExprId expr_id, ModuleLookupSet& dependencies)
{
    if (!syntax::is_valid(expr_id) || expr_id.value >= this->core_.ctx_.module.exprs.size()) {
        return false;
    }
    std::vector<ConstEvalFrame> stack;
    std::vector<bool> values;
    stack.push_back(ConstEvalFrame{expr_id, ConstEvalStage::ENTER, 0});
    while (!stack.empty()) {
        const ConstEvalFrame frame = stack.back();
        stack.pop_back();

        if (!syntax::is_valid(frame.expr_id) || frame.expr_id.value >= this->core_.ctx_.module.exprs.size()) {
            values.push_back(false);
            continue;
        }
        const ExprView expr = this->core_.expr_view(frame.expr_id);
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
                                this->core_.resolve_import_alias(expr.scope_name, expr.scope_range, false);
                            symbol = syntax::is_valid(module)
                                ? this->core_.find_symbol_in_module(module, expr.text_id, expr.text, expr.range, false)
                                : nullptr;
                        } else {
                            if (const Symbol* local = this->core_.state_.names.symbols.find(expr.text_id);
                                local != nullptr) {
                                symbol = local;
                            } else {
                                const ModuleLookupKey lookup_key = this->core_.find_module_lookup_key(
                                    this->core_.state_.flow.current_module, expr.text_id);
                                if (is_valid(lookup_key)) {
                                    if (const auto found =
                                            this->core_.state_.names.global_values_by_name.find(lookup_key);
                                        found != this->core_.state_.names.global_values_by_name.end()) {
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
                            : this->core_.ctx_.module.identifiers.find(symbol->name);
                        const ModuleLookupKey dependency_key = const_dependency_key(symbol->module, dependency_name_id);
                        if (is_valid(dependency_key)) {
                            dependencies.insert(dependency_key);
                        }
                        values.push_back(true);
                        break;
                    }
                    case syntax::ExprKind::field: {
                        bool const_evaluable = false;
                        const std::string_view expr_c_name = this->core_.cached_expr_c_name(frame.expr_id);
                        if (!expr_c_name.empty()) {
                            for (const auto& entry : this->core_.state_.checked.enum_cases) {
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

void SemanticAnalyzerCore::validate_module_namespace_conflicts() const
{
    DeclarationAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).validate_module_namespace_conflicts();
}

void SemanticAnalyzerCore::register_type_names()
{
    DeclarationAnalyzer(*this).register_type_names();
}

void SemanticAnalyzerCore::resolve_type_alias_decls()
{
    DeclarationAnalyzer(*this).resolve_type_alias_decls();
}

void SemanticAnalyzerCore::register_enum_cases_for_item(const syntax::ItemNode& item, const syntax::ModuleId owner,
    const TypeHandle named_enum_type, std::string enum_display_name, const std::string& case_prefix,
    const std::string& c_prefix, const syntax::Visibility visibility,
    const query::GenericInstanceKey& generic_instance_key)
{
    DeclarationAnalyzer(*this).register_enum_cases_for_item(
        item, owner, named_enum_type, enum_display_name, case_prefix, c_prefix, visibility, generic_instance_key);
}

void SemanticAnalyzerCore::register_value_names()
{
    DeclarationAnalyzer(*this).register_value_names();
}

void SemanticAnalyzerCore::validate_function_prototypes() const
{
    DeclarationAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).validate_function_prototypes();
}

void SemanticAnalyzerCore::validate_exported_signature_surfaces() const
{
    DeclarationAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).validate_exported_signature_surfaces();
}

void SemanticAnalyzerCore::validate_abi_symbols() const
{
    DeclarationAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).validate_abi_symbols();
}

void SemanticAnalyzerCore::analyze_entry_points() const
{
    DeclarationAnalyzer(const_cast<SemanticAnalyzerCore&>(*this)).analyze_entry_points();
}

void SemanticAnalyzerCore::analyze_struct_properties()
{
    DeclarationAnalyzer(*this).analyze_struct_properties();
}

void SemanticAnalyzerCore::analyze_derive_attributes()
{
    DeclarationAnalyzer(*this).analyze_derive_attributes();
}

void SemanticAnalyzerCore::analyze_derive_attributes_for_item(const syntax::ItemNode& item, const TypeHandle type,
    const bool report_invalid_attributes, const bool report_component_failures)
{
    DeclarationAnalyzer(*this).analyze_derive_attributes_for_item(
        item, type, report_invalid_attributes, report_component_failures);
}

void SemanticAnalyzerCore::analyze_const_decls()
{
    DeclarationAnalyzer(*this).analyze_const_decls();
}

bool SemanticAnalyzerCore::is_const_evaluable_expr(const syntax::ExprId expr_id, ModuleLookupSet& dependencies)
{
    return DeclarationAnalyzer(*this).is_const_evaluable_expr(expr_id, dependencies);
}

} // namespace aurex::sema
