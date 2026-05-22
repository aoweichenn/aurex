#pragma once

#include <aurex/query/diagnostics_query.hpp>
#include <aurex/query/function_body_syntax_query.hpp>
#include <aurex/query/generic_instance_body_query.hpp>
#include <aurex/query/generic_instance_signature_query.hpp>
#include <aurex/query/generic_template_signature_query.hpp>
#include <aurex/query/item_list_query.hpp>
#include <aurex/query/item_signature_query.hpp>
#include <aurex/query/lower_function_ir_query.hpp>
#include <aurex/query/module_exports_query.hpp>
#include <aurex/query/module_graph_query.hpp>
#include <aurex/query/source_file_query.hpp>
#include <aurex/query/type_check_body_query.hpp>

#include <functional>
#include <optional>

namespace aurex::query {

using ItemSignatureProvider =
    std::function<std::optional<ItemSignatureProviderOutput>(const ItemSignatureProviderInput&)>;
using GenericInstanceSignatureProvider =
    std::function<std::optional<GenericInstanceSignatureProviderOutput>(const GenericInstanceSignatureProviderInput&)>;
using GenericInstanceBodyProvider =
    std::function<std::optional<GenericInstanceBodyProviderOutput>(const GenericInstanceBodyProviderInput&)>;
using LowerFunctionIRProvider =
    std::function<std::optional<LowerFunctionIRProviderOutput>(const LowerFunctionIRProviderInput&)>;
using LowerGenericInstanceIRProvider =
    std::function<std::optional<LowerGenericInstanceIRProviderOutput>(const LowerGenericInstanceIRProviderInput&)>;
using ModuleGraphProvider = std::function<std::optional<ModuleGraphProviderOutput>(const ModuleGraphProviderInput&)>;
using ModuleExportsProvider =
    std::function<std::optional<ModuleExportsProviderOutput>(const ModuleExportsProviderInput&)>;
using ItemListProvider = std::function<std::optional<ItemListProviderOutput>(const ItemListProviderInput&)>;
using GenericTemplateSignatureProvider =
    std::function<std::optional<GenericTemplateSignatureProviderOutput>(const GenericTemplateSignatureProviderInput&)>;
using FunctionBodySyntaxProvider =
    std::function<std::optional<FunctionBodySyntaxProviderOutput>(const FunctionBodySyntaxProviderInput&)>;
using TypeCheckBodyProvider =
    std::function<std::optional<TypeCheckBodyProviderOutput>(const TypeCheckBodyProviderInput&)>;
using DiagnosticsProvider = std::function<std::optional<DiagnosticsProviderOutput>(const DiagnosticsProviderInput&)>;
using FileContentProvider = std::function<std::optional<FileContentProviderOutput>(const FileContentProviderInput&)>;
using LexFileProvider = std::function<std::optional<LexFileProviderOutput>(const LexFileProviderInput&)>;
using ParseFileProvider = std::function<std::optional<ParseFileProviderOutput>(const ParseFileProviderInput&)>;

struct QueryProviderOverrides final {
    FileContentProvider file_content;
    LexFileProvider lex_file;
    ParseFileProvider parse_file;
    ModuleGraphProvider module_graph;
    ModuleExportsProvider module_exports;
    ItemListProvider item_list;
    ItemSignatureProvider item_signature;
    GenericTemplateSignatureProvider generic_template_signature;
    GenericInstanceSignatureProvider generic_instance_signature;
    FunctionBodySyntaxProvider function_body_syntax;
    TypeCheckBodyProvider type_check_body;
    GenericInstanceBodyProvider generic_instance_body;
    LowerFunctionIRProvider lower_function_ir;
    LowerGenericInstanceIRProvider lower_generic_instance_ir;
    DiagnosticsProvider diagnostics;
};

class QueryProviderSet final {
public:
    QueryProviderSet();
    explicit QueryProviderSet(QueryProviderOverrides overrides);
    explicit QueryProviderSet(ItemSignatureProvider item_signature_provider);
    QueryProviderSet(ItemSignatureProvider item_signature_provider,
        GenericInstanceSignatureProvider generic_instance_signature_provider);
    QueryProviderSet(ModuleExportsProvider module_exports_provider, ItemSignatureProvider item_signature_provider,
        GenericInstanceSignatureProvider generic_instance_signature_provider);

    void set_file_content_provider(FileContentProvider provider);
    void set_lex_file_provider(LexFileProvider provider);
    void set_parse_file_provider(ParseFileProvider provider);
    void set_module_graph_provider(ModuleGraphProvider provider);
    void set_module_exports_provider(ModuleExportsProvider provider);
    void set_item_list_provider(ItemListProvider provider);
    void set_item_signature_provider(ItemSignatureProvider provider);
    void set_generic_template_signature_provider(GenericTemplateSignatureProvider provider);
    void set_generic_instance_signature_provider(GenericInstanceSignatureProvider provider);
    void set_function_body_syntax_provider(FunctionBodySyntaxProvider provider);
    void set_type_check_body_provider(TypeCheckBodyProvider provider);
    void set_generic_instance_body_provider(GenericInstanceBodyProvider provider);
    void set_lower_function_ir_provider(LowerFunctionIRProvider provider);
    void set_lower_generic_instance_ir_provider(LowerGenericInstanceIRProvider provider);
    void set_diagnostics_provider(DiagnosticsProvider provider);

    [[nodiscard]] std::optional<FileContentProviderOutput> provide_file_content(
        const FileContentProviderInput& input) const;
    [[nodiscard]] std::optional<LexFileProviderOutput> provide_lex_file(const LexFileProviderInput& input) const;
    [[nodiscard]] std::optional<ParseFileProviderOutput> provide_parse_file(const ParseFileProviderInput& input) const;
    [[nodiscard]] std::optional<ModuleGraphProviderOutput> provide_module_graph(
        const ModuleGraphProviderInput& input) const;
    [[nodiscard]] std::optional<ModuleExportsProviderOutput> provide_module_exports(
        const ModuleExportsProviderInput& input) const;
    [[nodiscard]] std::optional<ItemListProviderOutput> provide_item_list(const ItemListProviderInput& input) const;
    [[nodiscard]] std::optional<ItemSignatureProviderOutput> provide_item_signature(
        const ItemSignatureProviderInput& input) const;
    [[nodiscard]] std::optional<GenericTemplateSignatureProviderOutput> provide_generic_template_signature(
        const GenericTemplateSignatureProviderInput& input) const;
    [[nodiscard]] std::optional<GenericInstanceSignatureProviderOutput> provide_generic_instance_signature(
        const GenericInstanceSignatureProviderInput& input) const;
    [[nodiscard]] std::optional<FunctionBodySyntaxProviderOutput> provide_function_body_syntax(
        const FunctionBodySyntaxProviderInput& input) const;
    [[nodiscard]] std::optional<TypeCheckBodyProviderOutput> provide_type_check_body(
        const TypeCheckBodyProviderInput& input) const;
    [[nodiscard]] std::optional<GenericInstanceBodyProviderOutput> provide_generic_instance_body(
        const GenericInstanceBodyProviderInput& input) const;
    [[nodiscard]] std::optional<LowerFunctionIRProviderOutput> provide_lower_function_ir(
        const LowerFunctionIRProviderInput& input) const;
    [[nodiscard]] std::optional<LowerGenericInstanceIRProviderOutput> provide_lower_generic_instance_ir(
        const LowerGenericInstanceIRProviderInput& input) const;
    [[nodiscard]] std::optional<DiagnosticsProviderOutput> provide_diagnostics(
        const DiagnosticsProviderInput& input) const;

private:
    FileContentProvider file_content_provider_;
    LexFileProvider lex_file_provider_;
    ParseFileProvider parse_file_provider_;
    ModuleGraphProvider module_graph_provider_;
    ModuleExportsProvider module_exports_provider_;
    ItemListProvider item_list_provider_;
    ItemSignatureProvider item_signature_provider_;
    GenericTemplateSignatureProvider generic_template_signature_provider_;
    GenericInstanceSignatureProvider generic_instance_signature_provider_;
    FunctionBodySyntaxProvider function_body_syntax_provider_;
    TypeCheckBodyProvider type_check_body_provider_;
    GenericInstanceBodyProvider generic_instance_body_provider_;
    LowerFunctionIRProvider lower_function_ir_provider_;
    LowerGenericInstanceIRProvider lower_generic_instance_ir_provider_;
    DiagnosticsProvider diagnostics_provider_;
};

} // namespace aurex::query
