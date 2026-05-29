#pragma once

#include <aurex/query/query_graph.hpp>
#include <aurex/query/query_provider_set.hpp>

#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace aurex::query {

class QueryContext;

struct GenericInstanceSignatureQueryRequest {
    GenericInstanceKey key;
    GenericInstanceSignatureAuthority authority;
};

struct GenericInstanceBodyQueryRequest {
    GenericInstanceKey key;
    GenericInstanceBodyAuthority authority;
};

struct LowerGenericInstanceIRQueryRequest {
    GenericInstanceKey key;
    QueryResultFingerprint ir;
};

using QueryRequestInput = std::variant<ProjectGraphProviderInput, FileContentProviderInput, LexFileProviderInput,
    ParseFileProviderInput, ModuleGraphProviderInput, ModulePartProviderInput, ItemListProviderInput,
    ModuleExportsProviderInput, ModulePackageExportsProviderInput, ItemSignatureProviderInput,
    GenericTemplateSignatureProviderInput, GenericInstanceSignatureQueryRequest, FunctionBodySyntaxProviderInput,
    TypeCheckBodyProviderInput, GenericInstanceBodyQueryRequest, LowerFunctionIRProviderInput,
    LowerGenericInstanceIRQueryRequest, DiagnosticsProviderInput>;

struct QueryRequest {
    QueryRequestInput input;
};

struct QueryExecutionSummary {
    base::usize total = 0;
    base::usize computed = 0;
    base::usize cached = 0;
    base::usize failed = 0;
    base::usize cycles = 0;
};

struct QueryBatchExecutionResult {
    QueryExecutionSummary summary;
    std::vector<QueryEvaluationResult> results;
};

[[nodiscard]] std::optional<QueryKey> query_request_key(const QueryRequest& request) noexcept;

class QueryExecutor final {
public:
    explicit QueryExecutor(QueryContext& context) noexcept;

    [[nodiscard]] QueryEvaluationResult evaluate(const QueryRequest& request);
    [[nodiscard]] QueryBatchExecutionResult evaluate_all(std::span<const QueryRequest> requests);

private:
    QueryContext* context_ = nullptr;
};

} // namespace aurex::query
