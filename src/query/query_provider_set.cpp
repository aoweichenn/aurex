#include <aurex/query/query_provider_set.hpp>

#include <utility>

namespace aurex::query {

QueryProviderSet::QueryProviderSet()
    : QueryProviderSet(ModuleGraphProvider{provide_module_graph_query},
          ModuleExportsProvider{provide_module_exports_query}, ItemListProvider{provide_item_list_query},
          ItemSignatureProvider{provide_item_signature_query},
          GenericTemplateSignatureProvider{provide_generic_template_signature_query},
          GenericInstanceSignatureProvider{provide_generic_instance_signature_query},
          FileContentProvider{provide_file_content_query}, LexFileProvider{provide_lex_file_query},
          ParseFileProvider{provide_parse_file_query}, FunctionBodySyntaxProvider{provide_function_body_syntax_query},
          TypeCheckBodyProvider{provide_type_check_body_query},
          GenericInstanceBodyProvider{provide_generic_instance_body_query},
          LowerFunctionIRProvider{provide_lower_function_ir_query},
          LowerGenericInstanceIRProvider{provide_lower_generic_instance_ir_query},
          DiagnosticsProvider{provide_diagnostics_query})
{
}

QueryProviderSet::QueryProviderSet(ItemSignatureProvider item_signature_provider)
    : QueryProviderSet(ModuleGraphProvider{provide_module_graph_query},
          ModuleExportsProvider{provide_module_exports_query}, ItemListProvider{provide_item_list_query},
          std::move(item_signature_provider),
          GenericTemplateSignatureProvider{provide_generic_template_signature_query},
          GenericInstanceSignatureProvider{provide_generic_instance_signature_query},
          FileContentProvider{provide_file_content_query}, LexFileProvider{provide_lex_file_query},
          ParseFileProvider{provide_parse_file_query}, FunctionBodySyntaxProvider{provide_function_body_syntax_query},
          TypeCheckBodyProvider{provide_type_check_body_query},
          GenericInstanceBodyProvider{provide_generic_instance_body_query},
          LowerFunctionIRProvider{provide_lower_function_ir_query},
          LowerGenericInstanceIRProvider{provide_lower_generic_instance_ir_query},
          DiagnosticsProvider{provide_diagnostics_query})
{
}

QueryProviderSet::QueryProviderSet(
    ItemSignatureProvider item_signature_provider, GenericInstanceSignatureProvider generic_instance_signature_provider)
    : QueryProviderSet(ModuleGraphProvider{provide_module_graph_query},
          ModuleExportsProvider{provide_module_exports_query}, ItemListProvider{provide_item_list_query},
          std::move(item_signature_provider),
          GenericTemplateSignatureProvider{provide_generic_template_signature_query},
          std::move(generic_instance_signature_provider), FileContentProvider{provide_file_content_query},
          LexFileProvider{provide_lex_file_query}, ParseFileProvider{provide_parse_file_query},
          FunctionBodySyntaxProvider{provide_function_body_syntax_query},
          TypeCheckBodyProvider{provide_type_check_body_query},
          GenericInstanceBodyProvider{provide_generic_instance_body_query},
          LowerFunctionIRProvider{provide_lower_function_ir_query},
          LowerGenericInstanceIRProvider{provide_lower_generic_instance_ir_query},
          DiagnosticsProvider{provide_diagnostics_query})
{
}

QueryProviderSet::QueryProviderSet(ModuleExportsProvider module_exports_provider,
    ItemSignatureProvider item_signature_provider, GenericInstanceSignatureProvider generic_instance_signature_provider)
    : QueryProviderSet(ModuleGraphProvider{provide_module_graph_query}, std::move(module_exports_provider),
          ItemListProvider{provide_item_list_query}, std::move(item_signature_provider),
          GenericTemplateSignatureProvider{provide_generic_template_signature_query},
          std::move(generic_instance_signature_provider), FileContentProvider{provide_file_content_query},
          LexFileProvider{provide_lex_file_query}, ParseFileProvider{provide_parse_file_query},
          FunctionBodySyntaxProvider{provide_function_body_syntax_query},
          TypeCheckBodyProvider{provide_type_check_body_query},
          GenericInstanceBodyProvider{provide_generic_instance_body_query},
          LowerFunctionIRProvider{provide_lower_function_ir_query},
          LowerGenericInstanceIRProvider{provide_lower_generic_instance_ir_query},
          DiagnosticsProvider{provide_diagnostics_query})
{
}

QueryProviderSet::QueryProviderSet(ModuleGraphProvider module_graph_provider,
    ModuleExportsProvider module_exports_provider, ItemListProvider item_list_provider,
    ItemSignatureProvider item_signature_provider, GenericTemplateSignatureProvider generic_template_signature_provider,
    GenericInstanceSignatureProvider generic_instance_signature_provider, FileContentProvider file_content_provider,
    LexFileProvider lex_file_provider, ParseFileProvider parse_file_provider,
    FunctionBodySyntaxProvider function_body_syntax_provider, TypeCheckBodyProvider type_check_body_provider,
    GenericInstanceBodyProvider generic_instance_body_provider, LowerFunctionIRProvider lower_function_ir_provider,
    LowerGenericInstanceIRProvider lower_generic_instance_ir_provider, DiagnosticsProvider diagnostics_provider)
{
    this->set_file_content_provider(std::move(file_content_provider));
    this->set_lex_file_provider(std::move(lex_file_provider));
    this->set_parse_file_provider(std::move(parse_file_provider));
    this->set_module_graph_provider(std::move(module_graph_provider));
    this->set_module_exports_provider(std::move(module_exports_provider));
    this->set_item_list_provider(std::move(item_list_provider));
    this->set_item_signature_provider(std::move(item_signature_provider));
    this->set_generic_template_signature_provider(std::move(generic_template_signature_provider));
    this->set_generic_instance_signature_provider(std::move(generic_instance_signature_provider));
    this->set_function_body_syntax_provider(std::move(function_body_syntax_provider));
    this->set_type_check_body_provider(std::move(type_check_body_provider));
    this->set_generic_instance_body_provider(std::move(generic_instance_body_provider));
    this->set_lower_function_ir_provider(std::move(lower_function_ir_provider));
    this->set_lower_generic_instance_ir_provider(std::move(lower_generic_instance_ir_provider));
    this->set_diagnostics_provider(std::move(diagnostics_provider));
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
