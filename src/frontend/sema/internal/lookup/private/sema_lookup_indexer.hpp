#pragma once

#include <frontend/sema/internal/core/private/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore::LookupIndexer final {
public:
    explicit LookupIndexer(SemanticAnalyzerCore& core) noexcept;

    void index_named_type(syntax::ModuleId module, IdentId name_id, TypeHandle type, syntax::Visibility visibility);
    void index_type_alias(const TypeAliasInfo& info);
    void index_generic_struct_template(const GenericTemplateInfo& info);
    void index_generic_enum_template(const GenericTemplateInfo& info);
    void index_generic_type_alias_template(const GenericTemplateInfo& info);
    void index_generic_function_template(const GenericTemplateInfo& info);
    void index_generic_method_template(const GenericTemplateInfo& info);
    void index_function_lookup(const FunctionSignature& signature);
    void index_method_lookup(
        syntax::ModuleId module, TypeHandle owner_type, IdentId name_id, const FunctionSignature& signature);
    void index_function_value(const FunctionSignature& signature);
    void index_global_value(const Symbol& symbol);
    [[nodiscard]] bool named_type_lookup_complete() const noexcept;
    [[nodiscard]] bool type_alias_lookup_complete() const noexcept;
    [[nodiscard]] bool generic_struct_lookup_complete() const noexcept;
    [[nodiscard]] bool generic_enum_lookup_complete() const noexcept;
    [[nodiscard]] bool generic_type_alias_lookup_complete() const noexcept;
    [[nodiscard]] bool generic_function_lookup_complete() const noexcept;
    [[nodiscard]] bool generic_method_lookup_complete() const noexcept;
    [[nodiscard]] bool function_lookup_complete() const noexcept;
    [[nodiscard]] bool global_value_lookup_complete() const noexcept;
    [[nodiscard]] bool enum_case_module_lookup_complete() const noexcept;
    [[nodiscard]] bool top_level_value_name_exists(
        syntax::ModuleId module, IdentId name_id, std::string_view name) const;
    [[nodiscard]] bool module_type_or_value_name_exists(
        syntax::ModuleId module, IdentId name_id, std::string_view name) const;
    [[nodiscard]] bool current_generic_param_exists(IdentId name_id, std::string_view name) const;
    [[nodiscard]] bool visible_type_name_exists(IdentId name_id, std::string_view name) const;
    [[nodiscard]] bool can_define_local_name(
        IdentId name_id, std::string_view name, const base::SourceRange& range) const;
    [[nodiscard]] bool type_member_name_exists(TypeHandle owner_type, IdentId name_id, std::string_view name) const;
    void index_enum_case(const EnumCaseInfo& info);

private:
    SemanticAnalyzerCore& core_;
};

} // namespace aurex::sema
