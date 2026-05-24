#pragma once

#include <aurex/query/query_graph.hpp>
#include <aurex/query/query_interner.hpp>
#include <aurex/query/query_provider_set.hpp>

#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace aurex::query {

class QueryContext final {
public:
    QueryContext();
    explicit QueryContext(QueryProviderSet providers);
    explicit QueryContext(QueryProviderOverrides provider_overrides);
    explicit QueryContext(ItemSignatureProvider item_signature_provider);
    QueryContext(ItemSignatureProvider item_signature_provider,
        GenericInstanceSignatureProvider generic_instance_signature_provider);
    QueryContext(ModuleExportsProvider module_exports_provider, ItemSignatureProvider item_signature_provider,
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
    [[nodiscard]] QueryEvaluationResult evaluate_file_content(const FileContentProviderInput& input);
    [[nodiscard]] QueryEvaluationResult evaluate_lex_file(const LexFileProviderInput& input);
    [[nodiscard]] QueryEvaluationResult evaluate_parse_file(const ParseFileProviderInput& input);
    [[nodiscard]] QueryEvaluationResult evaluate_module_graph(const ModuleGraphProviderInput& input);
    [[nodiscard]] QueryEvaluationResult evaluate_module_exports(const ModuleExportsProviderInput& input);
    [[nodiscard]] QueryEvaluationResult evaluate_item_list(const ItemListProviderInput& input);
    [[nodiscard]] QueryEvaluationResult evaluate_item_signature(const ItemSignatureProviderInput& input);
    [[nodiscard]] QueryEvaluationResult evaluate_generic_template_signature(
        const GenericTemplateSignatureProviderInput& input);
    [[nodiscard]] QueryEvaluationResult evaluate_generic_instance_signature(
        const GenericInstanceSignatureProviderInput& input);
    [[nodiscard]] QueryEvaluationResult evaluate_function_body_syntax(const FunctionBodySyntaxProviderInput& input);
    [[nodiscard]] QueryEvaluationResult evaluate_type_check_body(const TypeCheckBodyProviderInput& input);
    [[nodiscard]] QueryEvaluationResult evaluate_generic_instance_body(const GenericInstanceBodyProviderInput& input);
    [[nodiscard]] QueryEvaluationResult evaluate_lower_function_ir(const LowerFunctionIRProviderInput& input);
    [[nodiscard]] QueryEvaluationResult evaluate_lower_generic_instance_ir(
        const LowerGenericInstanceIRProviderInput& input);
    [[nodiscard]] QueryEvaluationResult evaluate_diagnostics(const DiagnosticsProviderInput& input);
    [[nodiscard]] bool seed_completed_record(QueryRecord record, std::vector<QueryKey> dependencies = {});
    [[nodiscard]] bool invalidate(QueryKey key);
    [[nodiscard]] const QueryNode* find(QueryKey key) const;
    [[nodiscard]] const QueryNode* find(QueryNodeId id) const;
    [[nodiscard]] std::optional<QueryNodeId> node_id_for(QueryKey key) const;
    [[nodiscard]] std::vector<QueryKey> dependencies_for(QueryKey key) const;
    [[nodiscard]] std::vector<QueryKey> dependents_of(QueryKey key) const;
    [[nodiscard]] std::vector<QueryDependencyEdge> dependency_edges() const;
    [[nodiscard]] bool has_dependency(QueryKey dependent, QueryKey dependency) const;
    [[nodiscard]] base::usize dependency_edge_count() const noexcept;
    [[nodiscard]] base::usize interned_query_count() const noexcept;
    [[nodiscard]] base::usize bound_stable_identity_count() const noexcept;
    [[nodiscard]] QueryRevision current_revision() const noexcept;
    [[nodiscard]] QueryRevision advance_revision() noexcept;
    [[nodiscard]] std::vector<QueryRecord> completed_records() const;

private:
    struct QueryEvaluationStart {
        QueryNode* node = nullptr;
        std::optional<QueryEvaluationResult> result;
    };

    template <typename ProviderOutput, typename ProviderCall>
    [[nodiscard]] QueryEvaluationResult evaluate_query(
        std::optional<QueryKey> expected_key, ProviderCall&& provider_call);

    [[nodiscard]] QueryEvaluationStart start_query(QueryKey key);
    [[nodiscard]] QueryEvaluationResult complete_query(
        QueryNode& node, QueryRecord record, QueryResultFingerprint result, std::vector<QueryKey> dependencies);
    void remove_dependency_edges(QueryNode& node);
    void add_dependency_edges(const QueryNode& node);
    [[nodiscard]] QueryNode& node_for(QueryKey key);
    [[nodiscard]] bool intern_dependency_keys(const std::vector<QueryKey>& dependencies);
    [[nodiscard]] QueryEvaluationResult fail_query(QueryNode& node);

    QueryInterner interner_;
    std::unordered_map<QueryKey, QueryNode, QueryKeyHash> nodes_;
    std::unordered_map<QueryKey, std::vector<QueryKey>, QueryKeyHash> dependents_by_dependency_;
    base::usize dependency_edge_count_ = 0;
    QueryRevision current_revision_ = QUERY_REVISION_INITIAL;
    QueryProviderSet providers_;
};

template <typename ProviderOutput, typename ProviderCall>
QueryEvaluationResult QueryContext::evaluate_query(
    const std::optional<QueryKey> expected_key, ProviderCall&& provider_call)
{
    if (!expected_key) {
        return {};
    }

    QueryEvaluationStart start = this->start_query(*expected_key);
    if (start.result) {
        return *start.result;
    }
    QueryNode& node = *start.node;

    std::optional<ProviderOutput> output = std::forward<ProviderCall>(provider_call)();
    if (!output || !is_valid(*output) || output->record.key != *expected_key) {
        return this->fail_query(node);
    }

    return this->complete_query(node, std::move(output->record), output->result, std::move(output->dependencies));
}

} // namespace aurex::query
