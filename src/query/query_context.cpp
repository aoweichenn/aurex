#include <aurex/query/query_context.hpp>

#include <algorithm>
#include <tuple>
#include <utility>

namespace aurex::query {
namespace {

[[nodiscard]] bool query_key_less(const QueryKey lhs, const QueryKey rhs) noexcept
{
    return std::tie(
               lhs.kind, lhs.schema, lhs.global_id, lhs.payload.primary, lhs.payload.secondary, lhs.payload.byte_count)
        < std::tie(
            rhs.kind, rhs.schema, rhs.global_id, rhs.payload.primary, rhs.payload.secondary, rhs.payload.byte_count);
}

[[nodiscard]] bool query_record_key_less(const QueryRecord& lhs, const QueryRecord& rhs) noexcept
{
    return std::tie(lhs.key.kind, lhs.key.global_id, lhs.result.global_id, lhs.stable_key_bytes)
        < std::tie(rhs.key.kind, rhs.key.global_id, rhs.result.global_id, rhs.stable_key_bytes);
}

[[nodiscard]] bool query_dependency_edge_less(const QueryDependencyEdge& lhs, const QueryDependencyEdge& rhs) noexcept
{
    if (lhs.dependent != rhs.dependent) {
        return query_key_less(lhs.dependent, rhs.dependent);
    }
    return query_key_less(lhs.dependency, rhs.dependency);
}

[[nodiscard]] bool has_invalid_dependency(const std::vector<QueryKey>& dependencies) noexcept
{
    for (const QueryKey dependency : dependencies) {
        if (!is_valid(dependency)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool has_unexpected_dependency_kind(
    const QueryKey dependent, const std::vector<QueryKey>& dependencies) noexcept
{
    for (const QueryKey dependency : dependencies) {
        if (!query_dependency_edge_kind_is_expected(QueryDependencyEdge{
                dependent,
                dependency,
            })) {
            return true;
        }
    }
    return false;
}

void normalize_dependencies(std::vector<QueryKey>& dependencies)
{
    std::sort(dependencies.begin(), dependencies.end(), query_key_less);
    dependencies.erase(std::unique(dependencies.begin(), dependencies.end()), dependencies.end());
}

} // namespace

QueryContext::QueryContext() : QueryContext(QueryProviderSet{})
{
}

QueryContext::QueryContext(QueryProviderSet providers) : providers_(std::move(providers))
{
}

QueryContext::QueryContext(ItemSignatureProvider item_signature_provider)
    : QueryContext(QueryProviderSet{std::move(item_signature_provider)})
{
}

QueryContext::QueryContext(
    ItemSignatureProvider item_signature_provider, GenericInstanceSignatureProvider generic_instance_signature_provider)
    : QueryContext(QueryProviderSet{std::move(item_signature_provider), std::move(generic_instance_signature_provider)})
{
}

QueryContext::QueryContext(ModuleExportsProvider module_exports_provider, ItemSignatureProvider item_signature_provider,
    GenericInstanceSignatureProvider generic_instance_signature_provider)
    : QueryContext(QueryProviderSet{
          std::move(module_exports_provider),
          std::move(item_signature_provider),
          std::move(generic_instance_signature_provider),
      })
{
}

QueryContext::QueryContext(ModuleGraphProvider module_graph_provider, ModuleExportsProvider module_exports_provider,
    ItemListProvider item_list_provider, ItemSignatureProvider item_signature_provider,
    GenericTemplateSignatureProvider generic_template_signature_provider,
    GenericInstanceSignatureProvider generic_instance_signature_provider, FileContentProvider file_content_provider,
    LexFileProvider lex_file_provider, ParseFileProvider parse_file_provider,
    FunctionBodySyntaxProvider function_body_syntax_provider, TypeCheckBodyProvider type_check_body_provider,
    GenericInstanceBodyProvider generic_instance_body_provider, LowerFunctionIRProvider lower_function_ir_provider,
    LowerGenericInstanceIRProvider lower_generic_instance_ir_provider, DiagnosticsProvider diagnostics_provider)
    : QueryContext(QueryProviderSet{
          std::move(module_graph_provider),
          std::move(module_exports_provider),
          std::move(item_list_provider),
          std::move(item_signature_provider),
          std::move(generic_template_signature_provider),
          std::move(generic_instance_signature_provider),
          std::move(file_content_provider),
          std::move(lex_file_provider),
          std::move(parse_file_provider),
          std::move(function_body_syntax_provider),
          std::move(type_check_body_provider),
          std::move(generic_instance_body_provider),
          std::move(lower_function_ir_provider),
          std::move(lower_generic_instance_ir_provider),
          std::move(diagnostics_provider),
      })
{
}

void QueryContext::set_file_content_provider(FileContentProvider provider)
{
    this->providers_.set_file_content_provider(std::move(provider));
}

void QueryContext::set_lex_file_provider(LexFileProvider provider)
{
    this->providers_.set_lex_file_provider(std::move(provider));
}

void QueryContext::set_parse_file_provider(ParseFileProvider provider)
{
    this->providers_.set_parse_file_provider(std::move(provider));
}

void QueryContext::set_module_graph_provider(ModuleGraphProvider provider)
{
    this->providers_.set_module_graph_provider(std::move(provider));
}

void QueryContext::set_module_exports_provider(ModuleExportsProvider provider)
{
    this->providers_.set_module_exports_provider(std::move(provider));
}

void QueryContext::set_item_list_provider(ItemListProvider provider)
{
    this->providers_.set_item_list_provider(std::move(provider));
}

void QueryContext::set_item_signature_provider(ItemSignatureProvider provider)
{
    this->providers_.set_item_signature_provider(std::move(provider));
}

void QueryContext::set_generic_template_signature_provider(GenericTemplateSignatureProvider provider)
{
    this->providers_.set_generic_template_signature_provider(std::move(provider));
}

void QueryContext::set_generic_instance_signature_provider(GenericInstanceSignatureProvider provider)
{
    this->providers_.set_generic_instance_signature_provider(std::move(provider));
}

void QueryContext::set_function_body_syntax_provider(FunctionBodySyntaxProvider provider)
{
    this->providers_.set_function_body_syntax_provider(std::move(provider));
}

void QueryContext::set_type_check_body_provider(TypeCheckBodyProvider provider)
{
    this->providers_.set_type_check_body_provider(std::move(provider));
}

void QueryContext::set_generic_instance_body_provider(GenericInstanceBodyProvider provider)
{
    this->providers_.set_generic_instance_body_provider(std::move(provider));
}

void QueryContext::set_lower_function_ir_provider(LowerFunctionIRProvider provider)
{
    this->providers_.set_lower_function_ir_provider(std::move(provider));
}

void QueryContext::set_lower_generic_instance_ir_provider(LowerGenericInstanceIRProvider provider)
{
    this->providers_.set_lower_generic_instance_ir_provider(std::move(provider));
}

void QueryContext::set_diagnostics_provider(DiagnosticsProvider provider)
{
    this->providers_.set_diagnostics_provider(std::move(provider));
}

QueryEvaluationResult QueryContext::evaluate_file_content(const FileContentProviderInput& input)
{
    const std::optional<QueryKey> expected_key = file_content_query_key(input.key);
    if (!expected_key) {
        return {};
    }

    QueryEvaluationStart start = this->start_query(*expected_key);
    if (start.result) {
        return *start.result;
    }
    QueryNode& node = *start.node;

    std::optional<FileContentProviderOutput> output = this->providers_.provide_file_content(input);
    if (!output || !is_valid(*output) || output->record.key != *expected_key) {
        return this->fail_query(node);
    }

    return this->complete_query(node, std::move(output->record), output->result, std::move(output->dependencies));
}

QueryEvaluationResult QueryContext::evaluate_lex_file(const LexFileProviderInput& input)
{
    const std::optional<QueryKey> expected_key = lex_file_query_key(input.key);
    if (!expected_key) {
        return {};
    }

    QueryEvaluationStart start = this->start_query(*expected_key);
    if (start.result) {
        return *start.result;
    }
    QueryNode& node = *start.node;

    std::optional<LexFileProviderOutput> output = this->providers_.provide_lex_file(input);
    if (!output || !is_valid(*output) || output->record.key != *expected_key) {
        return this->fail_query(node);
    }

    return this->complete_query(node, std::move(output->record), output->result, std::move(output->dependencies));
}

QueryEvaluationResult QueryContext::evaluate_parse_file(const ParseFileProviderInput& input)
{
    const std::optional<QueryKey> expected_key = parse_file_query_key(input.key);
    if (!expected_key) {
        return {};
    }

    QueryEvaluationStart start = this->start_query(*expected_key);
    if (start.result) {
        return *start.result;
    }
    QueryNode& node = *start.node;

    std::optional<ParseFileProviderOutput> output = this->providers_.provide_parse_file(input);
    if (!output || !is_valid(*output) || output->record.key != *expected_key) {
        return this->fail_query(node);
    }

    return this->complete_query(node, std::move(output->record), output->result, std::move(output->dependencies));
}

QueryEvaluationResult QueryContext::evaluate_module_graph(const ModuleGraphProviderInput& input)
{
    const std::optional<QueryKey> expected_key = module_graph_query_key(input.key);
    if (!expected_key) {
        return {};
    }

    QueryEvaluationStart start = this->start_query(*expected_key);
    if (start.result) {
        return *start.result;
    }
    QueryNode& node = *start.node;

    std::optional<ModuleGraphProviderOutput> output = this->providers_.provide_module_graph(input);
    if (!output || !is_valid(*output) || output->record.key != *expected_key) {
        return this->fail_query(node);
    }

    return this->complete_query(node, std::move(output->record), output->result, std::move(output->dependencies));
}

QueryEvaluationResult QueryContext::evaluate_module_exports(const ModuleExportsProviderInput& input)
{
    const std::optional<QueryKey> expected_key = module_exports_query_key(input.key);
    if (!expected_key) {
        return {};
    }

    QueryEvaluationStart start = this->start_query(*expected_key);
    if (start.result) {
        return *start.result;
    }
    QueryNode& node = *start.node;

    std::optional<ModuleExportsProviderOutput> output = this->providers_.provide_module_exports(input);
    if (!output || !is_valid(*output) || output->record.key != *expected_key) {
        return this->fail_query(node);
    }

    return this->complete_query(node, std::move(output->record), output->result, std::move(output->dependencies));
}

QueryEvaluationResult QueryContext::evaluate_item_list(const ItemListProviderInput& input)
{
    const std::optional<QueryKey> expected_key = item_list_query_key(input.key);
    if (!expected_key) {
        return {};
    }

    QueryEvaluationStart start = this->start_query(*expected_key);
    if (start.result) {
        return *start.result;
    }
    QueryNode& node = *start.node;

    std::optional<ItemListProviderOutput> output = this->providers_.provide_item_list(input);
    if (!output || !is_valid(*output) || output->record.key != *expected_key) {
        return this->fail_query(node);
    }

    return this->complete_query(node, std::move(output->record), output->result, std::move(output->dependencies));
}

QueryEvaluationResult QueryContext::evaluate_item_signature(const ItemSignatureProviderInput& input)
{
    const std::optional<QueryKey> expected_key = item_signature_query_key(input.key);
    if (!expected_key) {
        return {};
    }

    QueryEvaluationStart start = this->start_query(*expected_key);
    if (start.result) {
        return *start.result;
    }
    QueryNode& node = *start.node;

    std::optional<ItemSignatureProviderOutput> output = this->providers_.provide_item_signature(input);
    if (!output || !is_valid(*output) || output->record.key != *expected_key) {
        return this->fail_query(node);
    }

    return this->complete_query(node, std::move(output->record), output->result, std::move(output->dependencies));
}

QueryEvaluationResult QueryContext::evaluate_generic_template_signature(
    const GenericTemplateSignatureProviderInput& input)
{
    const std::optional<QueryKey> expected_key = generic_template_signature_query_key(input.key);
    if (!expected_key) {
        return {};
    }

    QueryEvaluationStart start = this->start_query(*expected_key);
    if (start.result) {
        return *start.result;
    }
    QueryNode& node = *start.node;

    std::optional<GenericTemplateSignatureProviderOutput> output =
        this->providers_.provide_generic_template_signature(input);
    if (!output || !is_valid(*output) || output->record.key != *expected_key) {
        return this->fail_query(node);
    }

    return this->complete_query(node, std::move(output->record), output->result, std::move(output->dependencies));
}

QueryEvaluationResult QueryContext::evaluate_generic_instance_signature(
    const GenericInstanceSignatureProviderInput& input)
{
    const std::optional<QueryKey> expected_key =
        input.key == nullptr ? std::nullopt : generic_instance_signature_query_key(*input.key);
    if (!expected_key) {
        return {};
    }

    QueryEvaluationStart start = this->start_query(*expected_key);
    if (start.result) {
        return *start.result;
    }
    QueryNode& node = *start.node;

    std::optional<GenericInstanceSignatureProviderOutput> output =
        this->providers_.provide_generic_instance_signature(input);
    if (!output || !is_valid(*output) || output->record.key != *expected_key) {
        return this->fail_query(node);
    }

    return this->complete_query(node, std::move(output->record), output->result, std::move(output->dependencies));
}

QueryEvaluationResult QueryContext::evaluate_function_body_syntax(const FunctionBodySyntaxProviderInput& input)
{
    const std::optional<QueryKey> expected_key = function_body_syntax_query_key(input.key);
    if (!expected_key) {
        return {};
    }

    QueryEvaluationStart start = this->start_query(*expected_key);
    if (start.result) {
        return *start.result;
    }
    QueryNode& node = *start.node;

    std::optional<FunctionBodySyntaxProviderOutput> output = this->providers_.provide_function_body_syntax(input);
    if (!output || !is_valid(*output) || output->record.key != *expected_key) {
        return this->fail_query(node);
    }

    return this->complete_query(node, std::move(output->record), output->result, std::move(output->dependencies));
}

QueryEvaluationResult QueryContext::evaluate_type_check_body(const TypeCheckBodyProviderInput& input)
{
    const std::optional<QueryKey> expected_key = type_check_body_query_key(input.key);
    if (!expected_key) {
        return {};
    }

    QueryEvaluationStart start = this->start_query(*expected_key);
    if (start.result) {
        return *start.result;
    }
    QueryNode& node = *start.node;

    std::optional<TypeCheckBodyProviderOutput> output = this->providers_.provide_type_check_body(input);
    if (!output || !is_valid(*output) || output->record.key != *expected_key) {
        return this->fail_query(node);
    }

    return this->complete_query(node, std::move(output->record), output->result, std::move(output->dependencies));
}

QueryEvaluationResult QueryContext::evaluate_generic_instance_body(const GenericInstanceBodyProviderInput& input)
{
    const std::optional<QueryKey> expected_key =
        input.key == nullptr ? std::nullopt : generic_instance_body_query_key(*input.key);
    if (!expected_key) {
        return {};
    }

    QueryEvaluationStart start = this->start_query(*expected_key);
    if (start.result) {
        return *start.result;
    }
    QueryNode& node = *start.node;

    std::optional<GenericInstanceBodyProviderOutput> output = this->providers_.provide_generic_instance_body(input);
    if (!output || !is_valid(*output) || output->record.key != *expected_key) {
        return this->fail_query(node);
    }

    return this->complete_query(node, std::move(output->record), output->result, std::move(output->dependencies));
}

QueryEvaluationResult QueryContext::evaluate_lower_function_ir(const LowerFunctionIRProviderInput& input)
{
    const std::optional<QueryKey> expected_key = lower_function_ir_query_key(input.key);
    if (!expected_key) {
        return {};
    }

    QueryEvaluationStart start = this->start_query(*expected_key);
    if (start.result) {
        return *start.result;
    }
    QueryNode& node = *start.node;

    std::optional<LowerFunctionIRProviderOutput> output = this->providers_.provide_lower_function_ir(input);
    if (!output || !is_valid(*output) || output->record.key != *expected_key) {
        return this->fail_query(node);
    }

    return this->complete_query(node, std::move(output->record), output->result, std::move(output->dependencies));
}

QueryEvaluationResult QueryContext::evaluate_lower_generic_instance_ir(const LowerGenericInstanceIRProviderInput& input)
{
    const std::optional<QueryKey> expected_key =
        input.key == nullptr ? std::nullopt : lower_generic_instance_ir_query_key(*input.key);
    if (!expected_key) {
        return {};
    }

    QueryEvaluationStart start = this->start_query(*expected_key);
    if (start.result) {
        return *start.result;
    }
    QueryNode& node = *start.node;

    std::optional<LowerGenericInstanceIRProviderOutput> output =
        this->providers_.provide_lower_generic_instance_ir(input);
    if (!output || !is_valid(*output) || output->record.key != *expected_key) {
        return this->fail_query(node);
    }

    return this->complete_query(node, std::move(output->record), output->result, std::move(output->dependencies));
}

QueryEvaluationResult QueryContext::evaluate_diagnostics(const DiagnosticsProviderInput& input)
{
    const std::optional<QueryKey> expected_key = diagnostics_query_key(input.producer);
    if (!expected_key) {
        return {};
    }

    QueryEvaluationStart start = this->start_query(*expected_key);
    if (start.result) {
        return *start.result;
    }
    QueryNode& node = *start.node;

    std::optional<DiagnosticsProviderOutput> output = this->providers_.provide_diagnostics(input);
    if (!output || !is_valid(*output) || output->record.key != *expected_key) {
        return this->fail_query(node);
    }

    return this->complete_query(node, std::move(output->record), output->result, std::move(output->dependencies));
}

bool QueryContext::seed_completed_record(QueryRecord record, std::vector<QueryKey> dependencies)
{
    if (!query_record_stable_identity_is_valid(record) || has_invalid_dependency(dependencies)
        || has_unexpected_dependency_kind(record.key, dependencies)) {
        return false;
    }

    QueryNode& node = this->node_for(record.key);
    if (node.status == QueryNodeStatus::done || node.status == QueryNodeStatus::in_progress) {
        return false;
    }

    const QueryResultFingerprint result = record.result;
    return this->complete_query(node, std::move(record), result, std::move(dependencies)).status
        == QueryEvaluationStatus::computed;
}

bool QueryContext::invalidate(const QueryKey key)
{
    const auto found = this->nodes_.find(key);
    if (found == this->nodes_.end() || found->second.status != QueryNodeStatus::done) {
        return false;
    }

    QueryNode& node = found->second;
    this->remove_dependency_edges(node);
    node.status = QueryNodeStatus::failed;
    node.record = {};
    node.result = {};
    node.dependencies.clear();
    return true;
}

const QueryNode* QueryContext::find(const QueryKey key) const
{
    const auto found = this->nodes_.find(key);
    return found == this->nodes_.end() ? nullptr : &found->second;
}

const QueryNode* QueryContext::find(const QueryNodeId id) const
{
    const QueryInternedIdentity* const identity = this->interner_.find(id);
    return identity == nullptr ? nullptr : this->find(identity->key);
}

std::optional<QueryNodeId> QueryContext::node_id_for(const QueryKey key) const
{
    return this->interner_.find(key);
}

std::vector<QueryKey> QueryContext::dependencies_for(const QueryKey key) const
{
    const QueryNode* const node = this->find(key);
    if (node == nullptr || node->status != QueryNodeStatus::done) {
        return {};
    }
    return node->dependencies;
}

std::vector<QueryKey> QueryContext::dependents_of(const QueryKey key) const
{
    const auto found = this->dependents_by_dependency_.find(key);
    if (found == this->dependents_by_dependency_.end()) {
        return {};
    }
    return found->second;
}

std::vector<QueryDependencyEdge> QueryContext::dependency_edges() const
{
    std::vector<QueryDependencyEdge> edges;
    edges.reserve(this->dependency_edge_count_);
    for (const auto& entry : this->nodes_) {
        const QueryNode& node = entry.second;
        if (node.status != QueryNodeStatus::done) {
            continue;
        }
        for (const QueryKey dependency : node.dependencies) {
            edges.push_back(QueryDependencyEdge{
                node.key,
                dependency,
            });
        }
    }
    std::sort(edges.begin(), edges.end(), query_dependency_edge_less);
    return edges;
}

bool QueryContext::has_dependency(const QueryKey dependent, const QueryKey dependency) const
{
    const auto found = this->dependents_by_dependency_.find(dependency);
    if (found == this->dependents_by_dependency_.end()) {
        return false;
    }
    const std::vector<QueryKey>& dependents = found->second;
    return std::binary_search(dependents.begin(), dependents.end(), dependent, query_key_less);
}

base::usize QueryContext::dependency_edge_count() const noexcept
{
    return this->dependency_edge_count_;
}

base::usize QueryContext::interned_query_count() const noexcept
{
    return this->interner_.size();
}

base::usize QueryContext::bound_stable_identity_count() const noexcept
{
    return this->interner_.stable_identity_count();
}

std::vector<QueryRecord> QueryContext::completed_records() const
{
    std::vector<QueryRecord> records;
    records.reserve(this->nodes_.size());
    for (const auto& entry : this->nodes_) {
        const QueryNode& node = entry.second;
        if (node.status == QueryNodeStatus::done) {
            records.push_back(node.record);
        }
    }
    std::sort(records.begin(), records.end(), query_record_key_less);
    return records;
}

QueryContext::QueryEvaluationStart QueryContext::start_query(const QueryKey key)
{
    QueryNode& node = this->node_for(key);
    if (node.status == QueryNodeStatus::done) {
        return QueryEvaluationStart{
            &node,
            QueryEvaluationResult{
                QueryEvaluationStatus::cached,
                &node,
            },
        };
    }
    if (node.status == QueryNodeStatus::in_progress) {
        return QueryEvaluationStart{
            &node,
            QueryEvaluationResult{
                QueryEvaluationStatus::cycle,
                &node,
            },
        };
    }

    node.status = QueryNodeStatus::in_progress;
    this->remove_dependency_edges(node);
    node.record = {};
    node.result = {};
    node.dependencies.clear();
    return QueryEvaluationStart{
        &node,
        std::nullopt,
    };
}

QueryEvaluationResult QueryContext::complete_query(
    QueryNode& node, QueryRecord record, const QueryResultFingerprint result, std::vector<QueryKey> dependencies)
{
    if (!query_record_stable_identity_is_valid(record) || record.key != node.key || record.result != result
        || has_invalid_dependency(dependencies) || has_unexpected_dependency_kind(record.key, dependencies)) {
        return this->fail_query(node);
    }
    if (!this->interner_.bind_record(node.id, record) || !this->intern_dependency_keys(dependencies)) {
        return this->fail_query(node);
    }

    this->remove_dependency_edges(node);
    normalize_dependencies(dependencies);
    node.record = std::move(record);
    node.result = result;
    node.dependencies = std::move(dependencies);
    node.status = QueryNodeStatus::done;
    this->add_dependency_edges(node);
    return QueryEvaluationResult{
        QueryEvaluationStatus::computed,
        &node,
    };
}

void QueryContext::remove_dependency_edges(QueryNode& node)
{
    for (const QueryKey dependency : node.dependencies) {
        const auto found = this->dependents_by_dependency_.find(dependency);
        if (found == this->dependents_by_dependency_.end()) {
            continue;
        }
        std::vector<QueryKey>& dependents = found->second;
        const base::usize removed = std::erase(dependents, node.key);
        this->dependency_edge_count_ -= removed;
        if (dependents.empty()) {
            this->dependents_by_dependency_.erase(found);
        }
    }
}

void QueryContext::add_dependency_edges(const QueryNode& node)
{
    for (const QueryKey dependency : node.dependencies) {
        std::vector<QueryKey>& dependents = this->dependents_by_dependency_[dependency];
        if (std::binary_search(dependents.begin(), dependents.end(), node.key, query_key_less)) {
            continue;
        }
        dependents.push_back(node.key);
        std::sort(dependents.begin(), dependents.end(), query_key_less);
        ++this->dependency_edge_count_;
    }
}

QueryNode& QueryContext::node_for(const QueryKey key)
{
    const std::optional<QueryNodeId> id = this->interner_.intern_key(key);
    const auto inserted = this->nodes_.try_emplace(key,
        QueryNode{
            id.value_or(QueryNodeId{}),
            key,
            QueryNodeStatus::failed,
            {},
            {},
            {},
        });
    return inserted.first->second;
}

bool QueryContext::intern_dependency_keys(const std::vector<QueryKey>& dependencies)
{
    for (const QueryKey dependency : dependencies) {
        if (!this->interner_.intern_key(dependency).has_value()) {
            return false;
        }
    }
    return true;
}

QueryEvaluationResult QueryContext::fail_query(QueryNode& node)
{
    node.status = QueryNodeStatus::failed;
    this->remove_dependency_edges(node);
    node.record = {};
    node.result = {};
    node.dependencies.clear();
    return QueryEvaluationResult{
        QueryEvaluationStatus::failed,
        &node,
    };
}

} // namespace aurex::query
