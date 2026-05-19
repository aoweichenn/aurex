#pragma once

#include <aurex/query/diagnostics_query.hpp>
#include <aurex/query/function_body_syntax_query.hpp>
#include <aurex/query/generic_instance_body_query.hpp>
#include <aurex/query/generic_instance_signature_query.hpp>
#include <aurex/query/item_signature_query.hpp>
#include <aurex/query/module_exports_query.hpp>
#include <aurex/query/type_check_body_query.hpp>

#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

namespace aurex::query {

enum class QueryNodeStatus : base::u8 {
    in_progress,
    done,
    failed,
};

enum class QueryEvaluationStatus : base::u8 {
    computed,
    cached,
    failed,
    cycle,
};

struct QueryNode {
    QueryKey key;
    QueryNodeStatus status = QueryNodeStatus::failed;
    QueryRecord record;
    QueryResultFingerprint result;
    std::vector<QueryKey> dependencies;
};

struct QueryEvaluationResult {
    QueryEvaluationStatus status = QueryEvaluationStatus::failed;
    const QueryNode* node = nullptr;
};

struct QueryDependencyEdge {
    QueryKey dependent;
    QueryKey dependency;

    [[nodiscard]] friend constexpr bool operator==(QueryDependencyEdge lhs, QueryDependencyEdge rhs) noexcept = default;
};

using ItemSignatureProvider =
    std::function<std::optional<ItemSignatureProviderOutput>(const ItemSignatureProviderInput&)>;
using GenericInstanceSignatureProvider =
    std::function<std::optional<GenericInstanceSignatureProviderOutput>(const GenericInstanceSignatureProviderInput&)>;
using GenericInstanceBodyProvider =
    std::function<std::optional<GenericInstanceBodyProviderOutput>(const GenericInstanceBodyProviderInput&)>;
using ModuleExportsProvider =
    std::function<std::optional<ModuleExportsProviderOutput>(const ModuleExportsProviderInput&)>;
using FunctionBodySyntaxProvider =
    std::function<std::optional<FunctionBodySyntaxProviderOutput>(const FunctionBodySyntaxProviderInput&)>;
using TypeCheckBodyProvider =
    std::function<std::optional<TypeCheckBodyProviderOutput>(const TypeCheckBodyProviderInput&)>;
using DiagnosticsProvider = std::function<std::optional<DiagnosticsProviderOutput>(const DiagnosticsProviderInput&)>;

class QueryContext final {
public:
    QueryContext();
    explicit QueryContext(ItemSignatureProvider item_signature_provider);
    QueryContext(ItemSignatureProvider item_signature_provider,
        GenericInstanceSignatureProvider generic_instance_signature_provider);
    QueryContext(ModuleExportsProvider module_exports_provider, ItemSignatureProvider item_signature_provider,
        GenericInstanceSignatureProvider generic_instance_signature_provider);
    QueryContext(ModuleExportsProvider module_exports_provider, ItemSignatureProvider item_signature_provider,
        GenericInstanceSignatureProvider generic_instance_signature_provider,
        FunctionBodySyntaxProvider function_body_syntax_provider, TypeCheckBodyProvider type_check_body_provider,
        GenericInstanceBodyProvider generic_instance_body_provider, DiagnosticsProvider diagnostics_provider);

    void set_module_exports_provider(ModuleExportsProvider provider);
    void set_item_signature_provider(ItemSignatureProvider provider);
    void set_generic_instance_signature_provider(GenericInstanceSignatureProvider provider);
    void set_function_body_syntax_provider(FunctionBodySyntaxProvider provider);
    void set_type_check_body_provider(TypeCheckBodyProvider provider);
    void set_generic_instance_body_provider(GenericInstanceBodyProvider provider);
    void set_diagnostics_provider(DiagnosticsProvider provider);
    [[nodiscard]] QueryEvaluationResult evaluate_module_exports(const ModuleExportsProviderInput& input);
    [[nodiscard]] QueryEvaluationResult evaluate_item_signature(const ItemSignatureProviderInput& input);
    [[nodiscard]] QueryEvaluationResult evaluate_generic_instance_signature(
        const GenericInstanceSignatureProviderInput& input);
    [[nodiscard]] QueryEvaluationResult evaluate_function_body_syntax(const FunctionBodySyntaxProviderInput& input);
    [[nodiscard]] QueryEvaluationResult evaluate_type_check_body(const TypeCheckBodyProviderInput& input);
    [[nodiscard]] QueryEvaluationResult evaluate_generic_instance_body(const GenericInstanceBodyProviderInput& input);
    [[nodiscard]] QueryEvaluationResult evaluate_diagnostics(const DiagnosticsProviderInput& input);
    [[nodiscard]] bool seed_completed_record(QueryRecord record, std::vector<QueryKey> dependencies = {});
    [[nodiscard]] bool invalidate(QueryKey key);
    [[nodiscard]] const QueryNode* find(QueryKey key) const;
    [[nodiscard]] std::vector<QueryKey> dependencies_for(QueryKey key) const;
    [[nodiscard]] std::vector<QueryKey> dependents_of(QueryKey key) const;
    [[nodiscard]] std::vector<QueryDependencyEdge> dependency_edges() const;
    [[nodiscard]] bool has_dependency(QueryKey dependent, QueryKey dependency) const;
    [[nodiscard]] base::usize dependency_edge_count() const noexcept;
    [[nodiscard]] std::vector<QueryRecord> completed_records() const;

private:
    struct QueryEvaluationStart {
        QueryNode* node = nullptr;
        std::optional<QueryEvaluationResult> result;
    };

    [[nodiscard]] QueryEvaluationStart start_query(QueryKey key);
    void complete_query(
        QueryNode& node, QueryRecord record, QueryResultFingerprint result, std::vector<QueryKey> dependencies);
    void remove_dependency_edges(QueryNode& node);
    void add_dependency_edges(const QueryNode& node);
    [[nodiscard]] QueryNode& node_for(QueryKey key);
    [[nodiscard]] QueryEvaluationResult fail_query(QueryNode& node);

    std::unordered_map<QueryKey, QueryNode, QueryKeyHash> nodes_;
    std::unordered_map<QueryKey, std::vector<QueryKey>, QueryKeyHash> dependents_by_dependency_;
    base::usize dependency_edge_count_ = 0;
    ModuleExportsProvider module_exports_provider_;
    ItemSignatureProvider item_signature_provider_;
    GenericInstanceSignatureProvider generic_instance_signature_provider_;
    FunctionBodySyntaxProvider function_body_syntax_provider_;
    TypeCheckBodyProvider type_check_body_provider_;
    GenericInstanceBodyProvider generic_instance_body_provider_;
    DiagnosticsProvider diagnostics_provider_;
};

} // namespace aurex::query
