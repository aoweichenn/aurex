#include <aurex/query/query_provider_set.hpp>

#include <utility>

namespace aurex::query {
namespace {

[[nodiscard]] QueryProviderOverrides item_signature_override(ItemSignatureProvider provider)
{
    QueryProviderOverrides overrides;
    overrides.item_signature = std::move(provider);
    return overrides;
}

[[nodiscard]] QueryProviderOverrides item_and_instance_signature_override(
    ItemSignatureProvider item_signature_provider, GenericInstanceSignatureProvider generic_instance_signature_provider)
{
    QueryProviderOverrides overrides;
    overrides.item_signature = std::move(item_signature_provider);
    overrides.generic_instance_signature = std::move(generic_instance_signature_provider);
    return overrides;
}

[[nodiscard]] QueryProviderOverrides module_and_signature_override(ModuleExportsProvider module_exports_provider,
    ItemSignatureProvider item_signature_provider, GenericInstanceSignatureProvider generic_instance_signature_provider)
{
    QueryProviderOverrides overrides;
    overrides.module_exports = std::move(module_exports_provider);
    overrides.item_signature = std::move(item_signature_provider);
    overrides.generic_instance_signature = std::move(generic_instance_signature_provider);
    return overrides;
}

} // namespace

QueryProviderSet::QueryProviderSet() : QueryProviderSet(QueryProviderOverrides{})
{
}

QueryProviderSet::QueryProviderSet(QueryProviderOverrides overrides)
{
    this->set_file_content_provider(std::move(overrides.file_content));
    this->set_lex_file_provider(std::move(overrides.lex_file));
    this->set_parse_file_provider(std::move(overrides.parse_file));
    this->set_module_graph_provider(std::move(overrides.module_graph));
    this->set_module_exports_provider(std::move(overrides.module_exports));
    this->set_module_package_exports_provider(std::move(overrides.module_package_exports));
    this->set_item_list_provider(std::move(overrides.item_list));
    this->set_item_signature_provider(std::move(overrides.item_signature));
    this->set_generic_template_signature_provider(std::move(overrides.generic_template_signature));
    this->set_generic_instance_signature_provider(std::move(overrides.generic_instance_signature));
    this->set_function_body_syntax_provider(std::move(overrides.function_body_syntax));
    this->set_type_check_body_provider(std::move(overrides.type_check_body));
    this->set_generic_instance_body_provider(std::move(overrides.generic_instance_body));
    this->set_lower_function_ir_provider(std::move(overrides.lower_function_ir));
    this->set_lower_generic_instance_ir_provider(std::move(overrides.lower_generic_instance_ir));
    this->set_diagnostics_provider(std::move(overrides.diagnostics));
}

QueryProviderSet::QueryProviderSet(ItemSignatureProvider item_signature_provider)
    : QueryProviderSet(item_signature_override(std::move(item_signature_provider)))
{
}

QueryProviderSet::QueryProviderSet(
    ItemSignatureProvider item_signature_provider, GenericInstanceSignatureProvider generic_instance_signature_provider)
    : QueryProviderSet(item_and_instance_signature_override(
          std::move(item_signature_provider), std::move(generic_instance_signature_provider)))
{
}

QueryProviderSet::QueryProviderSet(ModuleExportsProvider module_exports_provider,
    ItemSignatureProvider item_signature_provider, GenericInstanceSignatureProvider generic_instance_signature_provider)
    : QueryProviderSet(module_and_signature_override(std::move(module_exports_provider),
          std::move(item_signature_provider), std::move(generic_instance_signature_provider)))
{
}

void QueryProviderSet::set_file_content_provider(FileContentProvider provider)
{
    this->file_content_provider_ = provider ? std::move(provider) : FileContentProvider{provide_file_content_query};
}

void QueryProviderSet::set_lex_file_provider(LexFileProvider provider)
{
    this->lex_file_provider_ = provider ? std::move(provider) : LexFileProvider{provide_lex_file_query};
}

void QueryProviderSet::set_parse_file_provider(ParseFileProvider provider)
{
    this->parse_file_provider_ = provider ? std::move(provider) : ParseFileProvider{provide_parse_file_query};
}

void QueryProviderSet::set_module_graph_provider(ModuleGraphProvider provider)
{
    this->module_graph_provider_ = provider ? std::move(provider) : ModuleGraphProvider{provide_module_graph_query};
}

void QueryProviderSet::set_module_exports_provider(ModuleExportsProvider provider)
{
    this->module_exports_provider_ =
        provider ? std::move(provider) : ModuleExportsProvider{provide_module_exports_query};
}

void QueryProviderSet::set_module_package_exports_provider(ModulePackageExportsProvider provider)
{
    this->module_package_exports_provider_ =
        provider ? std::move(provider) : ModulePackageExportsProvider{provide_module_package_exports_query};
}

void QueryProviderSet::set_item_list_provider(ItemListProvider provider)
{
    this->item_list_provider_ = provider ? std::move(provider) : ItemListProvider{provide_item_list_query};
}

void QueryProviderSet::set_item_signature_provider(ItemSignatureProvider provider)
{
    this->item_signature_provider_ =
        provider ? std::move(provider) : ItemSignatureProvider{provide_item_signature_query};
}

void QueryProviderSet::set_generic_template_signature_provider(GenericTemplateSignatureProvider provider)
{
    this->generic_template_signature_provider_ = provider ? std::move(provider)
                                                          : GenericTemplateSignatureProvider{
                                                                provide_generic_template_signature_query,
                                                            };
}

void QueryProviderSet::set_generic_instance_signature_provider(GenericInstanceSignatureProvider provider)
{
    this->generic_instance_signature_provider_ =
        provider ? std::move(provider) : GenericInstanceSignatureProvider{provide_generic_instance_signature_query};
}

void QueryProviderSet::set_function_body_syntax_provider(FunctionBodySyntaxProvider provider)
{
    this->function_body_syntax_provider_ =
        provider ? std::move(provider) : FunctionBodySyntaxProvider{provide_function_body_syntax_query};
}

void QueryProviderSet::set_type_check_body_provider(TypeCheckBodyProvider provider)
{
    this->type_check_body_provider_ =
        provider ? std::move(provider) : TypeCheckBodyProvider{provide_type_check_body_query};
}

void QueryProviderSet::set_generic_instance_body_provider(GenericInstanceBodyProvider provider)
{
    this->generic_instance_body_provider_ =
        provider ? std::move(provider) : GenericInstanceBodyProvider{provide_generic_instance_body_query};
}

void QueryProviderSet::set_lower_function_ir_provider(LowerFunctionIRProvider provider)
{
    this->lower_function_ir_provider_ =
        provider ? std::move(provider) : LowerFunctionIRProvider{provide_lower_function_ir_query};
}

void QueryProviderSet::set_lower_generic_instance_ir_provider(LowerGenericInstanceIRProvider provider)
{
    this->lower_generic_instance_ir_provider_ = provider ? std::move(provider)
                                                         : LowerGenericInstanceIRProvider{
                                                               provide_lower_generic_instance_ir_query,
                                                           };
}

void QueryProviderSet::set_diagnostics_provider(DiagnosticsProvider provider)
{
    this->diagnostics_provider_ = provider ? std::move(provider) : DiagnosticsProvider{provide_diagnostics_query};
}

std::optional<FileContentProviderOutput> QueryProviderSet::provide_file_content(
    const FileContentProviderInput& input) const
{
    return this->file_content_provider_(input);
}

std::optional<LexFileProviderOutput> QueryProviderSet::provide_lex_file(const LexFileProviderInput& input) const
{
    return this->lex_file_provider_(input);
}

std::optional<ParseFileProviderOutput> QueryProviderSet::provide_parse_file(const ParseFileProviderInput& input) const
{
    return this->parse_file_provider_(input);
}

std::optional<ModuleGraphProviderOutput> QueryProviderSet::provide_module_graph(
    const ModuleGraphProviderInput& input) const
{
    return this->module_graph_provider_(input);
}

std::optional<ModuleExportsProviderOutput> QueryProviderSet::provide_module_exports(
    const ModuleExportsProviderInput& input) const
{
    return this->module_exports_provider_(input);
}

std::optional<ModulePackageExportsProviderOutput> QueryProviderSet::provide_module_package_exports(
    const ModulePackageExportsProviderInput& input) const
{
    return this->module_package_exports_provider_(input);
}

std::optional<ItemListProviderOutput> QueryProviderSet::provide_item_list(const ItemListProviderInput& input) const
{
    return this->item_list_provider_(input);
}

std::optional<ItemSignatureProviderOutput> QueryProviderSet::provide_item_signature(
    const ItemSignatureProviderInput& input) const
{
    return this->item_signature_provider_(input);
}

std::optional<GenericTemplateSignatureProviderOutput> QueryProviderSet::provide_generic_template_signature(
    const GenericTemplateSignatureProviderInput& input) const
{
    return this->generic_template_signature_provider_(input);
}

std::optional<GenericInstanceSignatureProviderOutput> QueryProviderSet::provide_generic_instance_signature(
    const GenericInstanceSignatureProviderInput& input) const
{
    return this->generic_instance_signature_provider_(input);
}

std::optional<FunctionBodySyntaxProviderOutput> QueryProviderSet::provide_function_body_syntax(
    const FunctionBodySyntaxProviderInput& input) const
{
    return this->function_body_syntax_provider_(input);
}

std::optional<TypeCheckBodyProviderOutput> QueryProviderSet::provide_type_check_body(
    const TypeCheckBodyProviderInput& input) const
{
    return this->type_check_body_provider_(input);
}

std::optional<GenericInstanceBodyProviderOutput> QueryProviderSet::provide_generic_instance_body(
    const GenericInstanceBodyProviderInput& input) const
{
    return this->generic_instance_body_provider_(input);
}

std::optional<LowerFunctionIRProviderOutput> QueryProviderSet::provide_lower_function_ir(
    const LowerFunctionIRProviderInput& input) const
{
    return this->lower_function_ir_provider_(input);
}

std::optional<LowerGenericInstanceIRProviderOutput> QueryProviderSet::provide_lower_generic_instance_ir(
    const LowerGenericInstanceIRProviderInput& input) const
{
    return this->lower_generic_instance_ir_provider_(input);
}

std::optional<DiagnosticsProviderOutput> QueryProviderSet::provide_diagnostics(
    const DiagnosticsProviderInput& input) const
{
    return this->diagnostics_provider_(input);
}

} // namespace aurex::query
