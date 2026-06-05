#include <aurex/frontend/sema/sema_messages.hpp>

#include <optional>
#include <string>

#include <frontend/sema/internal/raii/private/sema_raii_analyzer.hpp>

namespace aurex::sema {
namespace {

constexpr std::string_view SEMA_RAII_DROP_TRAIT_NAME = "Drop";
constexpr std::string_view SEMA_RAII_DROP_METHOD_NAME = "drop";
constexpr std::string_view SEMA_RAII_SELF_PARAM_NAME = "self";
constexpr base::usize SEMA_RAII_DROP_METHOD_PARAM_COUNT = 1;

[[nodiscard]] std::optional<syntax::TypeNode> syntax_type_node(
    const syntax::AstModule& module, const syntax::TypeId type) noexcept
{
    if (!syntax::is_valid(type) || type.value >= module.types.size()) {
        return std::nullopt;
    }
    return module.types[type.value];
}

[[nodiscard]] bool final_type_name_is_drop(const syntax::TypeNode& type) noexcept
{
    return type.kind == syntax::TypeKind::named && type.name == SEMA_RAII_DROP_TRAIT_NAME;
}

[[nodiscard]] bool valid_drop_trait_surface(const syntax::TypeNode& type) noexcept
{
    return final_type_name_is_drop(type) && type.scope_parts.empty() && type.type_args.empty();
}

[[nodiscard]] bool valid_drop_impl_target_kind(const TypeKind kind) noexcept
{
    return kind == TypeKind::struct_ || kind == TypeKind::enum_ || kind == TypeKind::opaque_struct;
}

[[nodiscard]] bool valid_drop_impl_target_syntax(
    const syntax::AstModule& module, const syntax::TypeId impl_type) noexcept
{
    const std::optional<syntax::TypeNode> type = syntax_type_node(module, impl_type);
    return type.has_value() && type->kind == syntax::TypeKind::named;
}

[[nodiscard]] bool method_has_unsupported_drop_shape(const syntax::ItemNode& method) noexcept
{
    return !method.generic_params.empty() || !method.where_constraints.empty() || method.is_unsafe
        || method.is_extern_c || method.is_export_c || method.is_variadic;
}

[[nodiscard]] bool method_return_is_explicit_void(
    const TypeTable& types, const syntax::ItemNode& method, const FunctionSignature* const signature) noexcept
{
    return syntax::is_valid(method.return_type) && signature != nullptr && types.is_void(signature->return_type);
}

[[nodiscard]] bool method_has_exactly_one_param(const syntax::ItemNode& method) noexcept
{
    return method.params.size() == SEMA_RAII_DROP_METHOD_PARAM_COUNT;
}

} // namespace

SemanticAnalyzerCore::RaiiAnalyzer::RaiiAnalyzer(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

class SemanticAnalyzerCore::RaiiAnalyzer::AnalysisScope final {
public:
    AnalysisScope(SemanticAnalyzerCore& core, const syntax::ModuleId module, const syntax::ItemId item) noexcept
        : core_(core), previous_module_(core.state_.flow.current_module), previous_item_(core.state_.flow.current_item)
    {
        this->core_.state_.flow.current_module = module;
        this->core_.state_.flow.current_item = item;
    }

    ~AnalysisScope() noexcept
    {
        this->core_.state_.flow.current_module = this->previous_module_;
        this->core_.state_.flow.current_item = this->previous_item_;
    }

    AnalysisScope(const AnalysisScope&) = delete;
    AnalysisScope& operator=(const AnalysisScope&) = delete;
    AnalysisScope(AnalysisScope&&) = delete;
    AnalysisScope& operator=(AnalysisScope&&) = delete;

private:
    SemanticAnalyzerCore& core_;
    syntax::ModuleId previous_module_;
    syntax::ItemId previous_item_;
};

bool SemanticAnalyzerCore::RaiiAnalyzer::is_destructor_impl_block(const syntax::ItemNode& item) const
{
    return this->core_.is_destructor_impl_block(item);
}

void SemanticAnalyzerCore::RaiiAnalyzer::validate_destructor_impls()
{
    for (base::u32 item_index = 0; item_index < this->core_.ctx_.module.items.size(); ++item_index) {
        const syntax::ItemNode item = this->core_.ctx_.module.items[item_index];
        if (this->is_destructor_impl_block(item)) {
            this->validate_destructor_impl_block(item, syntax::ItemId{item_index});
        }
    }
}

void SemanticAnalyzerCore::RaiiAnalyzer::validate_destructor_impl_block(
    const syntax::ItemNode& impl_block, const syntax::ItemId impl_id)
{
    const AnalysisScope scope(this->core_, this->core_.item_module(impl_id), impl_id);

    TypeHandle self_type = INVALID_TYPE_HANDLE;
    if (!this->validate_impl_header(impl_block, impl_id, self_type)) {
        return;
    }

    const std::optional<syntax::ItemId> method_id = this->single_drop_method(impl_block);
    if (!method_id.has_value()) {
        return;
    }
    const syntax::ItemNode method = this->core_.ctx_.module.items[method_id->value];
    FunctionSignature* const signature = this->destructor_signature(method, *method_id);
    if (!this->validate_method_surface(method, self_type, signature)) {
        return;
    }
    if (signature == nullptr) {
        this->core_.report_type(method.range, std::string(SEMA_DROP_METHOD_SIGNATURE));
        return;
    }

    const FunctionLookupKey function_key = this->core_.function_key(method, *method_id);
    signature->is_destructor = true;
    this->record_destructor(impl_block, impl_id, method, *method_id, self_type, function_key);
}

bool SemanticAnalyzerCore::RaiiAnalyzer::validate_impl_header(
    const syntax::ItemNode& impl_block, const syntax::ItemId impl_id, TypeHandle& self_type) const
{
    const std::optional<syntax::TypeNode> trait_type = syntax_type_node(this->core_.ctx_.module, impl_block.trait_type);
    if (!trait_type.has_value() || !valid_drop_trait_surface(*trait_type)) {
        this->core_.report_type(impl_block.range, std::string(SEMA_DROP_IMPL_TRAIT_SURFACE));
        return false;
    }

    if (!impl_block.generic_params.empty() || !impl_block.where_constraints.empty()) {
        this->core_.report_unsupported(impl_block.range, std::string(SEMA_DROP_IMPL_GENERIC_UNSUPPORTED));
        return false;
    }

    if (!valid_drop_impl_target_syntax(this->core_.ctx_.module, impl_block.impl_type)) {
        this->core_.report_type(impl_block.range, std::string(SEMA_DROP_IMPL_TARGET));
        return false;
    }

    self_type = this->core_.resolve_type(impl_block.impl_type);
    if (!is_valid(self_type) || self_type.value >= this->core_.state_.checked.types.size()) {
        return false;
    }
    if (!valid_drop_impl_target_kind(this->core_.state_.checked.types.get(self_type).kind)) {
        this->core_.report_type(impl_block.range, std::string(SEMA_DROP_IMPL_TARGET));
        return false;
    }
    if (this->core_.state_.checked.destructors.contains(self_type.value)) {
        this->core_.report_duplicate(impl_block.range, std::string(SEMA_DROP_IMPL_DUPLICATE));
        return false;
    }
    return syntax::is_valid(impl_id);
}

std::optional<syntax::ItemId> SemanticAnalyzerCore::RaiiAnalyzer::single_drop_method(
    const syntax::ItemNode& impl_block) const
{
    base::usize method_count = 0;
    syntax::ItemId method_id = syntax::INVALID_ITEM_ID;
    for (const syntax::ItemId item_id : impl_block.impl_items) {
        if (!syntax::is_valid(item_id) || item_id.value >= this->core_.ctx_.module.items.size()) {
            continue;
        }
        const syntax::ItemNode item = this->core_.ctx_.module.items[item_id.value];
        if (item.kind == syntax::ItemKind::type_alias) {
            this->core_.report_unsupported(item.range, std::string(SEMA_DROP_ASSOCIATED_TYPE_UNSUPPORTED));
            continue;
        }
        if (item.kind == syntax::ItemKind::fn_decl) {
            ++method_count;
            method_id = item_id;
        }
    }

    if (method_count == 0) {
        this->core_.report_type(impl_block.range, std::string(SEMA_DROP_IMPL_METHOD_REQUIRED));
        return std::nullopt;
    }
    if (method_count != 1 || impl_block.impl_items.size() != 1) {
        this->core_.report_type(impl_block.range, std::string(SEMA_DROP_IMPL_SINGLE_METHOD));
        return std::nullopt;
    }
    return method_id;
}

FunctionSignature* SemanticAnalyzerCore::RaiiAnalyzer::destructor_signature(
    const syntax::ItemNode& method, const syntax::ItemId method_id)
{
    const FunctionLookupKey function_key = this->core_.function_key(method, method_id);
    const auto found = this->core_.state_.checked.functions.find(function_key);
    return found == this->core_.state_.checked.functions.end() ? nullptr : &found->second;
}

bool SemanticAnalyzerCore::RaiiAnalyzer::validate_method_surface(
    const syntax::ItemNode& method, const TypeHandle self_type, const FunctionSignature* const signature) const
{
    bool valid = true;
    if (method.name != SEMA_RAII_DROP_METHOD_NAME) {
        this->core_.report_type(method.range, std::string(SEMA_DROP_METHOD_NAME));
        valid = false;
    }
    if (method_has_unsupported_drop_shape(method)) {
        this->core_.report_type(method.range, std::string(SEMA_DROP_METHOD_SIGNATURE));
        valid = false;
    }
    if (method.is_prototype || !syntax::is_valid(method.body)) {
        this->core_.report_type(method.range, std::string(SEMA_DROP_METHOD_BODY_REQUIRED));
        valid = false;
    }
    if (method.borrow_contract.present) {
        this->core_.report_unsupported(method.borrow_contract.range,
            std::string(SEMA_DROP_METHOD_BORROW_CONTRACT_UNSUPPORTED));
        valid = false;
    }
    if (!method_return_is_explicit_void(this->core_.state_.checked.types, method, signature)) {
        this->core_.report_type(method.range, std::string(SEMA_DROP_METHOD_RETURN_VOID));
        valid = false;
    }
    if (!method_has_exactly_one_param(method) || signature == nullptr
        || signature->param_types.size() != SEMA_RAII_DROP_METHOD_PARAM_COUNT) {
        this->core_.report_type(method.range, std::string(SEMA_DROP_METHOD_SIGNATURE));
        return false;
    }

    const syntax::ParamDecl& param = method.params.front();
    const TypeHandle param_type = signature->param_types.front();
    if (param.name != SEMA_RAII_SELF_PARAM_NAME) {
        this->core_.report_type(param.range, std::string(SEMA_DROP_METHOD_SIGNATURE));
        valid = false;
    }
    if (!param.is_deinit) {
        this->core_.report_type(param.range, std::string(SEMA_DROP_METHOD_SELF_DEINIT));
        valid = false;
    }
    if (this->core_.state_.checked.types.is_pointer(param_type)
        || this->core_.state_.checked.types.is_reference(param_type)
        || !this->core_.state_.checked.types.same(param_type, self_type)) {
        this->core_.report_type(param.range, std::string(SEMA_DROP_METHOD_SELF_TYPE));
        valid = false;
    }
    return valid;
}

void SemanticAnalyzerCore::RaiiAnalyzer::record_destructor(const syntax::ItemNode& impl_block,
    const syntax::ItemId impl_id, const syntax::ItemNode&, const syntax::ItemId method_id, const TypeHandle self_type,
    const FunctionLookupKey function_key)
{
    DestructorInfo info;
    info.module = this->core_.item_module(impl_id);
    info.impl_item = impl_id;
    info.method_item = method_id;
    info.self_type = self_type;
    info.function_key = function_key;
    info.range = impl_block.range;
    info.part_index = this->core_.item_part_index(impl_id);
    info.fingerprint = destructor_info_fingerprint(info);
    this->core_.state_.checked.destructors.emplace(self_type.value, info);
}

void SemanticAnalyzerCore::validate_destructor_impls()
{
    RaiiAnalyzer(*this).validate_destructor_impls();
}

bool SemanticAnalyzerCore::is_destructor_impl_block(const syntax::ItemNode& item) const
{
    if (item.kind != syntax::ItemKind::impl_block) {
        return false;
    }
    const std::optional<syntax::TypeNode> trait_type = syntax_type_node(this->ctx_.module, item.trait_type);
    return trait_type.has_value() && final_type_name_is_drop(*trait_type);
}

} // namespace aurex::sema
