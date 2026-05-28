#include <aurex/query/query_context.hpp>
#include <aurex/query/query_executor.hpp>

#include <optional>

namespace aurex::query {
namespace {

struct QueryRequestKeyVisitor {
    [[nodiscard]] std::optional<QueryKey> operator()(const FileContentProviderInput& input) const noexcept
    {
        return file_content_query_key(input.key);
    }

    [[nodiscard]] std::optional<QueryKey> operator()(const LexFileProviderInput& input) const noexcept
    {
        return lex_file_query_key(input.key);
    }

    [[nodiscard]] std::optional<QueryKey> operator()(const ParseFileProviderInput& input) const noexcept
    {
        return parse_file_query_key(input.key);
    }

    [[nodiscard]] std::optional<QueryKey> operator()(const ModuleGraphProviderInput& input) const noexcept
    {
        return module_graph_query_key(input.key);
    }

    [[nodiscard]] std::optional<QueryKey> operator()(const ModulePartProviderInput& input) const noexcept
    {
        return module_part_query_key(input.key);
    }

    [[nodiscard]] std::optional<QueryKey> operator()(const ItemListProviderInput& input) const noexcept
    {
        return item_list_query_key(input.key);
    }

    [[nodiscard]] std::optional<QueryKey> operator()(const ModuleExportsProviderInput& input) const noexcept
    {
        return module_exports_query_key(input.key);
    }

    [[nodiscard]] std::optional<QueryKey> operator()(const ModulePackageExportsProviderInput& input) const noexcept
    {
        return module_package_exports_query_key(input.key);
    }

    [[nodiscard]] std::optional<QueryKey> operator()(const ItemSignatureProviderInput& input) const noexcept
    {
        return item_signature_query_key(input.key);
    }

    [[nodiscard]] std::optional<QueryKey> operator()(const GenericTemplateSignatureProviderInput& input) const noexcept
    {
        return generic_template_signature_query_key(input.key);
    }

    [[nodiscard]] std::optional<QueryKey> operator()(const GenericInstanceSignatureQueryRequest& request) const noexcept
    {
        return generic_instance_signature_query_key(request.key);
    }

    [[nodiscard]] std::optional<QueryKey> operator()(const FunctionBodySyntaxProviderInput& input) const noexcept
    {
        return function_body_syntax_query_key(input.key);
    }

    [[nodiscard]] std::optional<QueryKey> operator()(const TypeCheckBodyProviderInput& input) const noexcept
    {
        return type_check_body_query_key(input.key);
    }

    [[nodiscard]] std::optional<QueryKey> operator()(const GenericInstanceBodyQueryRequest& request) const noexcept
    {
        return generic_instance_body_query_key(request.key);
    }

    [[nodiscard]] std::optional<QueryKey> operator()(const LowerFunctionIRProviderInput& input) const noexcept
    {
        return lower_function_ir_query_key(input.key);
    }

    [[nodiscard]] std::optional<QueryKey> operator()(const LowerGenericInstanceIRQueryRequest& request) const noexcept
    {
        return lower_generic_instance_ir_query_key(request.key);
    }

    [[nodiscard]] std::optional<QueryKey> operator()(const DiagnosticsProviderInput& input) const noexcept
    {
        return diagnostics_query_key(input.producer);
    }
};

struct QueryEvaluateVisitor {
    QueryContext& context;

    [[nodiscard]] QueryEvaluationResult operator()(const FileContentProviderInput& input) const
    {
        return this->context.evaluate_file_content(input);
    }

    [[nodiscard]] QueryEvaluationResult operator()(const LexFileProviderInput& input) const
    {
        return this->context.evaluate_lex_file(input);
    }

    [[nodiscard]] QueryEvaluationResult operator()(const ParseFileProviderInput& input) const
    {
        return this->context.evaluate_parse_file(input);
    }

    [[nodiscard]] QueryEvaluationResult operator()(const ModuleGraphProviderInput& input) const
    {
        return this->context.evaluate_module_graph(input);
    }

    [[nodiscard]] QueryEvaluationResult operator()(const ModulePartProviderInput& input) const
    {
        return this->context.evaluate_module_part(input);
    }

    [[nodiscard]] QueryEvaluationResult operator()(const ItemListProviderInput& input) const
    {
        return this->context.evaluate_item_list(input);
    }

    [[nodiscard]] QueryEvaluationResult operator()(const ModuleExportsProviderInput& input) const
    {
        return this->context.evaluate_module_exports(input);
    }

    [[nodiscard]] QueryEvaluationResult operator()(const ModulePackageExportsProviderInput& input) const
    {
        return this->context.evaluate_module_package_exports(input);
    }

    [[nodiscard]] QueryEvaluationResult operator()(const ItemSignatureProviderInput& input) const
    {
        return this->context.evaluate_item_signature(input);
    }

    [[nodiscard]] QueryEvaluationResult operator()(const GenericTemplateSignatureProviderInput& input) const
    {
        return this->context.evaluate_generic_template_signature(input);
    }

    [[nodiscard]] QueryEvaluationResult operator()(const GenericInstanceSignatureQueryRequest& request) const
    {
        return this->context.evaluate_generic_instance_signature(GenericInstanceSignatureProviderInput{
            &request.key,
            request.authority,
        });
    }

    [[nodiscard]] QueryEvaluationResult operator()(const FunctionBodySyntaxProviderInput& input) const
    {
        return this->context.evaluate_function_body_syntax(input);
    }

    [[nodiscard]] QueryEvaluationResult operator()(const TypeCheckBodyProviderInput& input) const
    {
        return this->context.evaluate_type_check_body(input);
    }

    [[nodiscard]] QueryEvaluationResult operator()(const GenericInstanceBodyQueryRequest& request) const
    {
        return this->context.evaluate_generic_instance_body(GenericInstanceBodyProviderInput{
            &request.key,
            request.authority,
        });
    }

    [[nodiscard]] QueryEvaluationResult operator()(const LowerFunctionIRProviderInput& input) const
    {
        return this->context.evaluate_lower_function_ir(input);
    }

    [[nodiscard]] QueryEvaluationResult operator()(const LowerGenericInstanceIRQueryRequest& request) const
    {
        return this->context.evaluate_lower_generic_instance_ir(LowerGenericInstanceIRProviderInput{
            &request.key,
            request.ir,
        });
    }

    [[nodiscard]] QueryEvaluationResult operator()(const DiagnosticsProviderInput& input) const
    {
        return this->context.evaluate_diagnostics(input);
    }
};

void add_status_to_summary(QueryExecutionSummary& summary, const QueryEvaluationStatus status) noexcept
{
    ++summary.total;
    switch (status) {
        case QueryEvaluationStatus::computed:
            ++summary.computed;
            break;
        case QueryEvaluationStatus::cached:
            ++summary.cached;
            break;
        case QueryEvaluationStatus::failed:
            ++summary.failed;
            break;
        case QueryEvaluationStatus::cycle:
            ++summary.cycles;
            break;
    }
}

} // namespace

std::optional<QueryKey> query_request_key(const QueryRequest& request) noexcept
{
    return std::visit(QueryRequestKeyVisitor{}, request.input);
}

QueryExecutor::QueryExecutor(QueryContext& context) noexcept : context_(&context)
{
}

QueryEvaluationResult QueryExecutor::evaluate(const QueryRequest& request)
{
    return std::visit(QueryEvaluateVisitor{*this->context_}, request.input);
}

QueryBatchExecutionResult QueryExecutor::evaluate_all(std::span<const QueryRequest> requests)
{
    QueryBatchExecutionResult batch;
    batch.results.reserve(requests.size());
    for (const QueryRequest& request : requests) {
        QueryEvaluationResult result = this->evaluate(request);
        add_status_to_summary(batch.summary, result.status);
        batch.results.push_back(result);
    }
    return batch;
}

} // namespace aurex::query
