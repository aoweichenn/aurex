#pragma once

#include <optional>
#include <string>

#include <frontend/sema/internal/core/private/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore::DeclarationAnalyzer final {
public:
    explicit DeclarationAnalyzer(SemanticAnalyzerCore& core) noexcept;

    void validate_module_namespace_conflicts() const;
    void register_type_names();
    void resolve_type_alias_decls();
    void register_enum_cases_for_item(const syntax::ItemNode& item, const syntax::ModuleId owner,
        const TypeHandle named_enum_type, std::string enum_display_name, const std::string& case_prefix,
        const std::string& c_prefix, const syntax::Visibility visibility,
        const query::GenericInstanceKey& generic_instance_key);
    void register_value_names();
    void validate_function_prototypes() const;
    void validate_exported_signature_surfaces() const;
    void validate_abi_symbols() const;
    void analyze_entry_points() const;
    void analyze_struct_properties();
    void analyze_const_decls();
    bool is_const_evaluable_expr(const syntax::ExprId expr_id, ModuleLookupSet& dependencies);

    struct ExportSurfaceRestrictedType {
        std::string name;
        base::SourceRange range{};
        syntax::Visibility visibility = syntax::Visibility::private_;
    };

    [[nodiscard]] std::optional<ExportSurfaceRestrictedType> restricted_type_exposed_by_surface_type(
        TypeHandle root, syntax::Visibility surface_visibility) const;

private:
    [[nodiscard]] bool method_signature_is_exported_surface(const FunctionSignature& signature) const;

    SemanticAnalyzerCore& core_;
};

} // namespace aurex::sema
