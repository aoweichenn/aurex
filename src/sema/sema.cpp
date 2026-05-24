#include <utility>

#include <sema/internal/sema_core.hpp>
#include <sema/internal/sema_pipeline.hpp>

namespace aurex::sema {

SemanticAnalyzerCore::SemanticAnalyzerCore(
    syntax::AstModule& module, base::DiagnosticSink& diagnostics, const SemanticOptions options) noexcept
    : ctx_{module, diagnostics, options}
{
}

SemanticAnalyzerCore::SemanticAnalyzerCore(
    syntax::AstModule&& module, base::DiagnosticSink& diagnostics, const SemanticOptions options) noexcept
    : owned_module_(std::move(module)), ctx_{*this->owned_module_, diagnostics, options}
{
}

SemanticAnalyzerCore::GenericTemplateList& SemanticAnalyzerCore::generic_method_template_bucket(
    const ModuleLookupKey& key)
{
    if (const auto found = this->state_.names.generic_method_templates_by_name.find(key);
        found != this->state_.names.generic_method_templates_by_name.end()) {
        return found->second;
    }
    auto bucket = make_sema_vector<const GenericTemplateInfo*>(*this->state_.arena);
    const auto inserted = this->state_.names.generic_method_templates_by_name.emplace(key, std::move(bucket));
    return inserted.first->second;
}

SemanticAnalyzerCore::EnumCaseList& SemanticAnalyzerCore::enum_case_type_bucket(const TypeHandle enum_type)
{
    if (const auto found = this->state_.names.enum_cases_by_type.find(enum_type.value);
        found != this->state_.names.enum_cases_by_type.end()) {
        return found->second;
    }
    auto bucket = make_sema_vector<const EnumCaseInfo*>(*this->state_.arena);
    const auto inserted = this->state_.names.enum_cases_by_type.emplace(enum_type.value, std::move(bucket));
    return inserted.first->second;
}

SemanticAnalyzerCore::ModuleIdList SemanticAnalyzerCore::make_module_id_list() const
{
    return make_sema_vector<syntax::ModuleId>(*this->state_.arena);
}

SemanticAnalyzerCore::GenericTemplateInfo SemanticAnalyzerCore::make_generic_template_info() const
{
    GenericTemplateInfo info;
    info.params = make_sema_vector<IdentId>(*this->state_.arena);
    info.param_identities = make_sema_vector<GenericParamIdentity>(*this->state_.arena);
    info.constraints = make_sema_map<IdentId, CapabilitySet, IdentIdHash>(*this->state_.arena, IdentIdHash{});
    info.expr_node_ids = make_sema_vector<base::u32>(*this->state_.arena);
    info.pattern_node_ids = make_sema_vector<base::u32>(*this->state_.arena);
    info.type_node_ids = make_sema_vector<base::u32>(*this->state_.arena);
    info.stmt_node_ids = make_sema_vector<base::u32>(*this->state_.arena);
    return info;
}

SemanticAnalyzerCore::GenericContext SemanticAnalyzerCore::make_generic_context() const
{
    GenericContext context;
    context.params = make_sema_map<IdentId, TypeHandle, IdentIdHash>(*this->state_.arena, IdentIdHash{});
    context.param_identities =
        make_sema_map<IdentId, GenericParamIdentity, IdentIdHash>(*this->state_.arena, IdentIdHash{});
    context.constraints = make_sema_map<IdentId, CapabilitySet, IdentIdHash>(*this->state_.arena, IdentIdHash{});
    context.constraints_by_identity = make_sema_map<GenericParamIdentity, CapabilitySet, GenericParamIdentityHash>(
        *this->state_.arena, GenericParamIdentityHash{});
    return context;
}

SemanticAnalyzerCore::CapabilitySet SemanticAnalyzerCore::make_capability_set() const
{
    return make_sema_set<CapabilityKind, CapabilityKindHash>(*this->state_.arena, CapabilityKindHash{});
}

SemanticAnalyzerCore::CapabilitySet SemanticAnalyzerCore::copy_capability_set(const CapabilitySet& source) const
{
    CapabilitySet copy = this->make_capability_set();
    copy.reserve(source.size());
    copy.insert(source.begin(), source.end());
    return copy;
}

void SemanticAnalyzerCore::copy_capability_map(CapabilityMap& target, const CapabilityMap& source) const
{
    target.clear();
    target.reserve(source.size());
    for (const auto& entry : source) {
        target.emplace(entry.first, this->copy_capability_set(entry.second));
    }
}

SemanticAnalyzerCore::CapabilitySet& SemanticAnalyzerCore::capability_bucket(
    CapabilityMap& map, const IdentId key) const
{
    if (const auto found = map.find(key); found != map.end()) {
        return found->second;
    }
    const auto inserted = map.emplace(key, this->make_capability_set());
    return inserted.first->second;
}

SemanticAnalyzerCore::CapabilitySet& SemanticAnalyzerCore::capability_bucket(
    CapabilityIdentityMap& map, const GenericParamIdentity key) const
{
    if (const auto found = map.find(key); found != map.end()) {
        return found->second;
    }
    const auto inserted = map.emplace(key, this->make_capability_set());
    return inserted.first->second;
}

base::Result<CheckedModule> SemanticAnalyzerCore::analyze()
{
    return SemanticAnalysisPipeline(*this).run();
}

} // namespace aurex::sema
