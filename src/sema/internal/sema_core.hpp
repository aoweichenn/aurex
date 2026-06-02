#pragma once

#include <aurex/base/diagnostic.hpp>
#include <aurex/base/result.hpp>
#include <aurex/sema/access_control.hpp>
#include <aurex/sema/checked_module.hpp>
#include <aurex/sema/diagnostic_kind.hpp>
#include <aurex/sema/function.hpp>
#include <aurex/sema/identifier.hpp>
#include <aurex/sema/sema.hpp>
#include <aurex/sema/symbol.hpp>
#include <aurex/sema/type.hpp>
#include <aurex/syntax/ast.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace aurex::sema {

enum class ValueAbiContext {
    parameter,
    function_type_parameter,
    function_type_return,
    return_value,
    assignment,
    enum_payload,
    enum_payload_argument,
    argument,
};

struct SemaContext final {
    syntax::AstModule& module;
    base::DiagnosticSink& diagnostics;
    SemanticOptions options;
};

class ModuleVisibilityResolver;
class SemanticAnalysisPipeline;
class SemanticAbiChecker;
class SemanticDiagnosticReporter;
class SemanticBodyCheckService;
class SemanticGenericService;
class SemanticLookupService;
class SemanticServiceBundle;
class SemanticTypeService;
class SemanticSideTableReader;
class SemanticSideTableStore;
class SemanticTypeResolver;
class SemanticTypeValidator;
class BodyMoveAnalysis;

class SemanticAnalyzerCore final {
    friend class BodyMoveAnalysis;

public:
    SemanticAnalyzerCore(
        syntax::AstModule& module, base::DiagnosticSink& diagnostics, SemanticOptions options = {}) noexcept;
    SemanticAnalyzerCore(
        const syntax::AstModule& module, base::DiagnosticSink& diagnostics, SemanticOptions options = {}) = delete;
    SemanticAnalyzerCore(
        syntax::AstModule&& module, base::DiagnosticSink& diagnostics, SemanticOptions options = {}) noexcept;

    [[nodiscard]] base::Result<CheckedModule> analyze();

#if defined(AUREX_SEMA_WHITEBOX_TESTS)
public:
#else
private:
#endif
    enum class FunctionBodyState {
        not_started,
        analyzing,
        analyzed,
    };

    struct ReturnTypeInference {
        TypeHandle inferred_type = INVALID_TYPE_HANDLE;
        std::vector<syntax::StmtId> returns;
        std::vector<syntax::StmtId> pending_null_returns;
    };

    static constexpr base::u64 SEMA_TYPE_ABI_INVALID_SIZE = 0;
    static constexpr base::u64 SEMA_TYPE_ABI_MIN_ALIGNMENT = 1;
    static constexpr int SEMA_NO_LOOP_DEPTH = 0;

    using CapabilitySet = SemaSet<CapabilityKind, CapabilityKindHash>;
    using CapabilityMap = SemaMap<IdentId, CapabilitySet, IdentIdHash>;
    using CapabilityIdentityMap = SemaMap<GenericParamIdentity, CapabilitySet, GenericParamIdentityHash>;

    class BuiltinExpressionAnalyzer;
    class BorrowEscapeAnalyzer;
    class BodyFlowAnalyzer;
    class BodyMoveAnalyzer;
    class ControlExpressionAnalyzer;
    class DeclarationAnalyzer;
    class ExpressionAnalyzer;
    class GenericAnalyzer;
    class LookupIndexer;
    class LookupResolver;
    class OperatorExpressionAnalyzer;
    class PatternMatchAnalyzer;
    class ProjectionAggregateExpressionAnalyzer;
    class StatementAnalyzer;
    class TraitAnalyzer;

    struct GenericTemplateInfo {
        syntax::ItemId item = syntax::INVALID_ITEM_ID;
        syntax::ModuleId module = syntax::INVALID_MODULE_ID;
        base::u32 part_index = 0;
        InternedText name;
        IdentId name_id = INVALID_IDENT_ID;
        ModuleLookupKey key;
        FunctionLookupKey function_key;
        StableDefId stable_id;
        IncrementalKey incremental_key;
        SemaVector<IdentId> params;
        SemaVector<GenericParamIdentity> param_identities;
        CapabilityMap constraints;
        SemaIndexTable predicate_indices;
        SemaIndexTable obligation_indices;
        query::ParamEnvKey param_env_key;
        base::u32 param_env_index = SEMA_TRAIT_PREDICATE_INVALID_INDEX;
        TypeHandle impl_type_pattern = INVALID_TYPE_HANDLE;
        syntax::Visibility visibility = syntax::Visibility::private_;
        GenericNodeSpan expr_span;
        GenericNodeSpan pattern_span;
        GenericNodeSpan type_span;
        GenericNodeSpan stmt_span;
        SemaIndexTable expr_node_ids;
        SemaIndexTable pattern_node_ids;
        SemaIndexTable type_node_ids;
        SemaIndexTable stmt_node_ids;
        mutable base::usize checked_side_table_layout_index = SEMA_GENERIC_SIDE_TABLE_INVALID_LAYOUT_INDEX;

        [[nodiscard]] bool has_sparse_node_ids() const noexcept;
    };

    struct GenericContext {
        SemaMap<IdentId, TypeHandle, IdentIdHash> params;
        SemaMap<IdentId, GenericParamIdentity, IdentIdHash> param_identities;
        CapabilityMap constraints;
        CapabilityIdentityMap constraints_by_identity;
        SemaIndexTable predicate_indices;
        SemaIndexTable obligation_indices;
        query::ParamEnvKey param_env_key;
    };

    struct TraitMethodCallResolution {
        const TraitSignature* trait = nullptr;
        const TraitMethodRequirement* requirement = nullptr;
        const TraitImplInfo* impl = nullptr;
        const TraitPredicate* predicate = nullptr;
        const FunctionSignature* signature = nullptr;
        std::vector<TypeHandle> param_types;
        TypeHandle return_type = INVALID_TYPE_HANDLE;
        TraitMethodDispatchKind dispatch = TraitMethodDispatchKind::param_env;
        bool found = false;
        bool reported_failure = false;
    };

    struct GenericSideTableScope {
        GenericSideTables* side_tables = nullptr;
        bool cache_syntax_types = true;
    };

    struct FlowState {
        syntax::ModuleId current_module = syntax::INVALID_MODULE_ID;
        syntax::ItemId current_item = syntax::INVALID_ITEM_ID;
        TypeHandle current_function_return_type = INVALID_TYPE_HANDLE;
        ReturnTypeInference* current_return_inference = nullptr;
        GenericContext* current_generic_context = nullptr;
        GenericSideTableScope current_side_tables{};
        int loop_depth = SEMA_NO_LOOP_DEPTH;
        int unsafe_context_depth = 0;
        bool in_const_initializer = false;
    };

    struct GenericState {
        explicit GenericState(base::BumpAllocator& arena)
            : struct_templates(make_sema_map<ModuleLookupKey, GenericTemplateInfo, ModuleLookupKeyHash>(
                  arena, ModuleLookupKeyHash{})),
              enum_templates(make_sema_map<ModuleLookupKey, GenericTemplateInfo, ModuleLookupKeyHash>(
                  arena, ModuleLookupKeyHash{})),
              type_alias_templates(make_sema_map<ModuleLookupKey, GenericTemplateInfo, ModuleLookupKeyHash>(
                  arena, ModuleLookupKeyHash{})),
              function_templates(make_sema_map<ModuleLookupKey, GenericTemplateInfo, ModuleLookupKeyHash>(
                  arena, ModuleLookupKeyHash{})),
              method_templates(make_sema_map<FunctionLookupKey, GenericTemplateInfo, FunctionLookupKeyHash>(
                  arena, FunctionLookupKeyHash{})),
              param_query_keys(make_sema_map<GenericParamIdentity, query::GenericParamKey, GenericParamIdentityHash>(
                  arena, GenericParamIdentityHash{})),
              struct_instances(make_sema_map<IdentId, TypeHandle, IdentIdHash>(arena, IdentIdHash{})),
              enum_instances(make_sema_map<IdentId, TypeHandle, IdentIdHash>(arena, IdentIdHash{})),
              resolved_type_aliases(make_sema_map<IdentId, TypeHandle, IdentIdHash>(arena, IdentIdHash{})),
              function_instances(
                  make_sema_map<FunctionLookupKey, base::usize, FunctionLookupKeyHash>(arena, FunctionLookupKeyHash{})),
              placeholder_functions(make_sema_map<FunctionLookupKey, FunctionSignature, FunctionLookupKeyHash>(
                  arena, FunctionLookupKeyHash{}))
        {
        }

        SemaMap<ModuleLookupKey, GenericTemplateInfo, ModuleLookupKeyHash> struct_templates;
        SemaMap<ModuleLookupKey, GenericTemplateInfo, ModuleLookupKeyHash> enum_templates;
        SemaMap<ModuleLookupKey, GenericTemplateInfo, ModuleLookupKeyHash> type_alias_templates;
        SemaMap<ModuleLookupKey, GenericTemplateInfo, ModuleLookupKeyHash> function_templates;
        SemaMap<FunctionLookupKey, GenericTemplateInfo, FunctionLookupKeyHash> method_templates;
        SemaMap<GenericParamIdentity, query::GenericParamKey, GenericParamIdentityHash> param_query_keys;
        SemaMap<IdentId, TypeHandle, IdentIdHash> struct_instances;
        SemaMap<IdentId, TypeHandle, IdentIdHash> enum_instances;
        SemaMap<IdentId, TypeHandle, IdentIdHash> resolved_type_aliases;
        SemaMap<FunctionLookupKey, base::usize, FunctionLookupKeyHash> function_instances;
        SemaMap<FunctionLookupKey, FunctionSignature, FunctionLookupKeyHash> placeholder_functions;
    };

    struct FunctionState {
        explicit FunctionState(base::BumpAllocator& arena)
            : global_values(
                  make_sema_map<FunctionLookupKey, Symbol, FunctionLookupKeyHash>(arena, FunctionLookupKeyHash{})),
              definition_items(make_sema_map<FunctionLookupKey, syntax::ItemId, FunctionLookupKeyHash>(
                  arena, FunctionLookupKeyHash{})),
              body_states(make_sema_map<FunctionLookupKey, FunctionBodyState, FunctionLookupKeyHash>(
                  arena, FunctionLookupKeyHash{}))
        {
        }

        SemaMap<FunctionLookupKey, Symbol, FunctionLookupKeyHash> global_values;
        SemaMap<FunctionLookupKey, syntax::ItemId, FunctionLookupKeyHash> definition_items;
        SemaMap<FunctionLookupKey, FunctionBodyState, FunctionLookupKeyHash> body_states;
    };

    struct TraitState {
        explicit TraitState(base::BumpAllocator& arena)
            : requirement_items(make_sema_set<base::u32>(arena)),
              default_method_instances(
                  make_sema_map<FunctionLookupKey, base::usize, FunctionLookupKeyHash>(arena, FunctionLookupKeyHash{}))
        {
        }

        SemaSet<base::u32> requirement_items;
        SemaMap<FunctionLookupKey, base::usize, FunctionLookupKeyHash> default_method_instances;
    };

    struct GenericInstanceIdentity {
        query::GenericInstanceKey key;
        std::string fingerprint_text;
    };

    struct FunctionBodyContextScope;
    struct GenericAnalysisScope;
    class GenericInstanceCanonicalResolver;

    struct PlaceInfo {
        TypeHandle type = INVALID_TYPE_HANDLE;
        bool is_place = false;
        bool is_writable = false;
        bool crosses_raw_pointer = false;
    };

    struct TryShape {
        enum class Kind {
            none,
            result,
            option,
            malformed_result,
            malformed_option,
        };

        Kind kind = Kind::none;
        const EnumCaseInfo* success_case = nullptr;
        const EnumCaseInfo* failure_case = nullptr;
    };

    struct ModuleSelector {
        syntax::ModuleId module = syntax::INVALID_MODULE_ID;
        bool failed_as_module_selector = false;
    };

    struct SelectiveReexportTarget {
        syntax::ModuleId module = syntax::INVALID_MODULE_ID;
        IdentId name_id = INVALID_IDENT_ID;
        std::string_view name;
        syntax::ModuleId exporter = syntax::INVALID_MODULE_ID;
        syntax::Visibility visibility = syntax::Visibility::public_;
    };

    using SelectiveReexportTargetList = std::vector<SelectiveReexportTarget>;

    struct IndexedTypeInfo {
        TypeHandle type = INVALID_TYPE_HANDLE;
        syntax::Visibility visibility = syntax::Visibility::public_;
    };

    struct TypeState {
        explicit TypeState(base::BumpAllocator& arena)
            : named_types(
                  make_sema_map<ModuleLookupKey, TypeHandle, ModuleLookupKeyHash>(arena, ModuleLookupKeyHash{})),
              resolved_type_aliases(
                  make_sema_map<ModuleLookupKey, TypeHandle, ModuleLookupKeyHash>(arena, ModuleLookupKeyHash{})),
              resolving_type_aliases(make_sema_vector<ModuleLookupKey>(arena)),
              struct_infos_by_type(make_sema_map<base::u32, const StructInfo*>(arena))
        {
        }

        SemaMap<ModuleLookupKey, TypeHandle, ModuleLookupKeyHash> named_types;
        SemaMap<ModuleLookupKey, TypeHandle, ModuleLookupKeyHash> resolved_type_aliases;
        SemaVector<ModuleLookupKey> resolving_type_aliases;
        SemaMap<base::u32, const StructInfo*> struct_infos_by_type;
    };

    struct ModuleSelectorPath {
        std::vector<std::string_view> parts;
        std::vector<IdentId> part_ids;
        base::SourceRange range{};
    };

    struct NamedTypeSelector {
        syntax::ModuleId module = syntax::INVALID_MODULE_ID;
        std::string_view name;
        IdentId name_id = INVALID_IDENT_ID;
        base::SourceRange range{};
        std::vector<syntax::TypeId> type_args;
        bool qualified = false;
    };

    struct ExprView {
        syntax::ExprKind kind = syntax::ExprKind::invalid;
        base::SourceRange range{};
        std::string_view scope_name;
        IdentId scope_name_id = INVALID_IDENT_ID;
        base::SourceRange scope_range{};
        std::string_view text;
        IdentId text_id = INVALID_IDENT_ID;
        syntax::UnaryOp unary_op = syntax::UnaryOp::logical_not;
        syntax::ExprId unary_operand = syntax::INVALID_EXPR_ID;
        syntax::ExprId try_operand = syntax::INVALID_EXPR_ID;
        syntax::BinaryOp binary_op = syntax::BinaryOp::add;
        syntax::ExprId binary_lhs = syntax::INVALID_EXPR_ID;
        syntax::ExprId binary_rhs = syntax::INVALID_EXPR_ID;
        syntax::ExprId callee = syntax::INVALID_EXPR_ID;
        std::span<const syntax::ExprId> args{};
        syntax::ExprId condition = syntax::INVALID_EXPR_ID;
        syntax::PatternId condition_pattern = syntax::INVALID_PATTERN_ID;
        syntax::ExprId then_expr = syntax::INVALID_EXPR_ID;
        syntax::ExprId else_expr = syntax::INVALID_EXPR_ID;
        syntax::StmtId block = syntax::INVALID_STMT_ID;
        syntax::ExprId block_result = syntax::INVALID_EXPR_ID;
        syntax::ExprId match_value = syntax::INVALID_EXPR_ID;
        std::span<const syntax::MatchArm> match_arms{};
        std::span<const syntax::ExprId> array_elements{};
        std::span<const syntax::ExprId> tuple_elements{};
        syntax::ExprId array_repeat_value = syntax::INVALID_EXPR_ID;
        syntax::ExprId array_repeat_count = syntax::INVALID_EXPR_ID;
        syntax::ExprId object = syntax::INVALID_EXPR_ID;
        std::string_view field_name;
        IdentId field_name_id = INVALID_IDENT_ID;
        syntax::ExprId index = syntax::INVALID_EXPR_ID;
        syntax::ExprId slice_start = syntax::INVALID_EXPR_ID;
        syntax::ExprId slice_end = syntax::INVALID_EXPR_ID;
        std::string_view struct_name;
        IdentId struct_name_id = INVALID_IDENT_ID;
        std::span<const syntax::TypeId> type_args{};
        std::span<const syntax::FieldInit> field_inits{};
        syntax::TypeId cast_type = syntax::INVALID_TYPE_ID;
        syntax::ExprId cast_expr = syntax::INVALID_EXPR_ID;
    };

    struct BinaryExprAnalysis {
        TypeHandle lhs = INVALID_TYPE_HANDLE;
        TypeHandle rhs = INVALID_TYPE_HANDLE;
        TypeHandle operand_expected = INVALID_TYPE_HANDLE;
        TypeHandle result_intrinsic = INVALID_TYPE_HANDLE;
        bool null_pointer_comparison = false;
    };

    struct PatternBinding {
        InternedText name;
        IdentId name_id = INVALID_IDENT_ID;
        TypeHandle type = INVALID_TYPE_HANDLE;
        base::SourceRange range{};
    };

    struct MatchUsefulnessChecker;

    enum class StatementAnalysisRootKind {
        statement,
        scoped_block,
        block_statements,
    };

    enum class StatementAnalysisActionKind {
        statement,
        scoped_block,
        block_statements,
        pattern_scoped_block,
        local_pattern,
        pop_scope,
        enter_loop,
        exit_loop,
        for_condition,
    };

    struct StatementAnalysisAction {
        StatementAnalysisActionKind kind = StatementAnalysisActionKind::statement;
        syntax::StmtId stmt = syntax::INVALID_STMT_ID;
        syntax::StmtId block = syntax::INVALID_STMT_ID;
        syntax::PatternId pattern = syntax::INVALID_PATTERN_ID;
        TypeHandle pattern_type = INVALID_TYPE_HANDLE;
        bool is_mutable = false;
        bool allow_refutable = false;
    };

    using GenericTemplateList = SemaVector<const GenericTemplateInfo*>;
    using EnumCaseList = SemaVector<const EnumCaseInfo*>;
    using ModuleIdList = SemaVector<syntax::ModuleId>;
    using ModuleLookupList = SemaVector<ModuleLookupKey>;
    using ModuleLookupSet = SemaSet<ModuleLookupKey, ModuleLookupKeyHash>;

    struct NameState {
        explicit NameState(base::BumpAllocator& arena)
            : named_types_by_name(
                  make_sema_map<ModuleLookupKey, IndexedTypeInfo, ModuleLookupKeyHash>(arena, ModuleLookupKeyHash{})),
              type_aliases_by_name(make_sema_map<ModuleLookupKey, const TypeAliasInfo*, ModuleLookupKeyHash>(
                  arena, ModuleLookupKeyHash{})),
              generic_struct_templates_by_name(
                  make_sema_map<ModuleLookupKey, const GenericTemplateInfo*, ModuleLookupKeyHash>(
                      arena, ModuleLookupKeyHash{})),
              generic_enum_templates_by_name(
                  make_sema_map<ModuleLookupKey, const GenericTemplateInfo*, ModuleLookupKeyHash>(
                      arena, ModuleLookupKeyHash{})),
              generic_type_alias_templates_by_name(
                  make_sema_map<ModuleLookupKey, const GenericTemplateInfo*, ModuleLookupKeyHash>(
                      arena, ModuleLookupKeyHash{})),
              generic_function_templates_by_name(
                  make_sema_map<ModuleLookupKey, const GenericTemplateInfo*, ModuleLookupKeyHash>(
                      arena, ModuleLookupKeyHash{})),
              generic_method_templates_by_name(make_sema_map<ModuleLookupKey, GenericTemplateList, ModuleLookupKeyHash>(
                  arena, ModuleLookupKeyHash{})),
              functions_by_name(make_sema_map<ModuleLookupKey, const FunctionSignature*, ModuleLookupKeyHash>(
                  arena, ModuleLookupKeyHash{})),
              methods_by_name(make_sema_map<MethodLookupKey, const FunctionSignature*, MethodLookupKeyHash>(
                  arena, MethodLookupKeyHash{})),
              traits_by_name(make_sema_map<ModuleLookupKey, const TraitSignature*, ModuleLookupKeyHash>(
                  arena, ModuleLookupKeyHash{})),
              global_values_by_name(
                  make_sema_map<ModuleLookupKey, const Symbol*, ModuleLookupKeyHash>(arena, ModuleLookupKeyHash{})),
              method_global_values_by_name(
                  make_sema_map<MethodLookupKey, const Symbol*, MethodLookupKeyHash>(arena, MethodLookupKeyHash{})),
              enum_cases_by_module_name(make_sema_map<ModuleLookupKey, const EnumCaseInfo*, ModuleLookupKeyHash>(
                  arena, ModuleLookupKeyHash{})),
              enum_cases_by_type_and_case(make_sema_map<EnumCaseLookupKey, const EnumCaseInfo*, EnumCaseLookupKeyHash>(
                  arena, EnumCaseLookupKeyHash{})),
              enum_cases_by_type(make_sema_map<base::u32, EnumCaseList>(arena))
        {
        }

        SymbolTable symbols;
        SemaMap<ModuleLookupKey, IndexedTypeInfo, ModuleLookupKeyHash> named_types_by_name;
        SemaMap<ModuleLookupKey, const TypeAliasInfo*, ModuleLookupKeyHash> type_aliases_by_name;
        SemaMap<ModuleLookupKey, const GenericTemplateInfo*, ModuleLookupKeyHash> generic_struct_templates_by_name;
        SemaMap<ModuleLookupKey, const GenericTemplateInfo*, ModuleLookupKeyHash> generic_enum_templates_by_name;
        SemaMap<ModuleLookupKey, const GenericTemplateInfo*, ModuleLookupKeyHash> generic_type_alias_templates_by_name;
        SemaMap<ModuleLookupKey, const GenericTemplateInfo*, ModuleLookupKeyHash> generic_function_templates_by_name;
        SemaMap<ModuleLookupKey, GenericTemplateList, ModuleLookupKeyHash> generic_method_templates_by_name;
        base::usize generic_method_lookup_indexed_count = 0;
        SemaMap<ModuleLookupKey, const FunctionSignature*, ModuleLookupKeyHash> functions_by_name;
        SemaMap<MethodLookupKey, const FunctionSignature*, MethodLookupKeyHash> methods_by_name;
        SemaMap<ModuleLookupKey, const TraitSignature*, ModuleLookupKeyHash> traits_by_name;
        base::usize internal_function_lookup_exclusions = 0;
        base::usize internal_global_value_lookup_exclusions = 0;
        SemaMap<ModuleLookupKey, const Symbol*, ModuleLookupKeyHash> global_values_by_name;
        SemaMap<MethodLookupKey, const Symbol*, MethodLookupKeyHash> method_global_values_by_name;
        SemaMap<ModuleLookupKey, const EnumCaseInfo*, ModuleLookupKeyHash> enum_cases_by_module_name;
        SemaMap<EnumCaseLookupKey, const EnumCaseInfo*, EnumCaseLookupKeyHash> enum_cases_by_type_and_case;
        SemaMap<base::u32, EnumCaseList> enum_cases_by_type;
    };

    struct ModuleState {
        explicit ModuleState(base::BumpAllocator& arena)
            : visible_modules_cache(make_sema_map<base::u32, ModuleIdList>(arena)),
              item_visible_modules_cache(make_sema_map<base::u32, ModuleIdList>(arena)),
              export_modules_cache(make_sema_map<base::u32, ModuleIdList>(arena))
        {
        }

        SemaMap<base::u32, ModuleIdList> visible_modules_cache;
        SemaMap<base::u32, ModuleIdList> item_visible_modules_cache;
        SemaMap<base::u32, ModuleIdList> export_modules_cache;
    };

    struct TypeAbiLayout {
        base::u64 size = SEMA_TYPE_ABI_INVALID_SIZE;
        base::u64 align = SEMA_TYPE_ABI_MIN_ALIGNMENT;
    };

    struct SemaState {
        SemaState()
            : arena(std::make_unique<base::BumpAllocator>()), names(*this->arena), types(*this->arena),
              generics(*this->arena), functions(*this->arena), traits(*this->arena), modules(*this->arena)
        {
        }

        std::unique_ptr<base::BumpAllocator> arena;
        CheckedModule checked;
        NameState names;
        TypeState types;
        GenericState generics;
        FunctionState functions;
        TraitState traits;
        mutable ModuleState modules;
        FlowState flow;
    };

    void register_type_names();
    void register_trait_name(const syntax::ItemNode& item, syntax::ItemId item_id);
    void register_trait_signatures();
    void validate_trait_impls();
    void analyze_trait_default_method_bodies();
    [[nodiscard]] bool is_trait_requirement_item(syntax::ItemId item) const;
    void register_generic_template(const syntax::ItemNode& item, syntax::ItemId item_id);
    void validate_generic_parameter_list(const syntax::ItemNode& item);
    void validate_generic_constraints(const syntax::ItemNode& item, GenericTemplateInfo& info);
    void record_generic_template_signature(const GenericTemplateInfo& info, query::DefNamespace name_space);
    [[nodiscard]] GenericTemplateInfo make_generic_template_info() const;
    [[nodiscard]] std::string generic_template_incremental_fingerprint(
        const syntax::ItemNode& item, const GenericTemplateInfo& info) const;
    void populate_generic_template_node_spans(GenericTemplateInfo& info, const syntax::ItemNode& item) const;
    [[nodiscard]] GenericSideTables make_generic_instance_side_tables(const GenericTemplateInfo& info);
    [[nodiscard]] base::usize generic_side_table_layout_index(const GenericTemplateInfo& info);
    void index_generic_param_query_keys(const GenericTemplateInfo& info, query::DefNamespace name_space);
    [[nodiscard]] GenericContext make_generic_context() const;
    [[nodiscard]] CapabilitySet make_capability_set() const;
    [[nodiscard]] CapabilitySet copy_capability_set(const CapabilitySet& source) const;
    void copy_capability_map(CapabilityMap& target, const CapabilityMap& source) const;
    [[nodiscard]] CapabilitySet& capability_bucket(CapabilityMap& map, IdentId key) const;
    [[nodiscard]] CapabilitySet& capability_bucket(CapabilityIdentityMap& map, GenericParamIdentity key) const;
    [[nodiscard]] const TraitSignature* find_trait_in_visible_modules(
        IdentId name_id, std::string_view name, const base::SourceRange& range, bool report_unknown = true);
    [[nodiscard]] TraitMethodCallResolution resolve_trait_method_call(TypeHandle owner_type, IdentId name_id,
        std::string_view name, const base::SourceRange& range, bool require_self, bool report_failure = true);
    [[nodiscard]] TypeHandle resolve_associated_type_projection(TypeHandle base_type, IdentId associated_name_id,
        std::string_view associated_name, const base::SourceRange& range, bool report_failure = true);
    [[nodiscard]] query::StableFingerprint128 generic_trait_predicate_fingerprint(const GenericTemplateInfo& info,
        base::usize param_index, TraitPredicateKind kind, CapabilityKind capability, const TraitSignature* trait) const;
    [[nodiscard]] bool generic_param_has_capability(std::string_view param, CapabilityKind capability) const;
    [[nodiscard]] bool generic_param_has_capability(TypeHandle param, CapabilityKind capability) const;
    [[nodiscard]] bool type_satisfies_capability(TypeHandle type, CapabilityKind capability) const;
    [[nodiscard]] bool type_satisfies_equality_capability(TypeHandle type) const;
    [[nodiscard]] bool type_satisfies_ordering_capability(TypeHandle type) const;
    [[nodiscard]] bool type_supports_equality_operator(TypeHandle type) const;
    [[nodiscard]] bool type_supports_ordering_operator(TypeHandle type) const;
    [[nodiscard]] bool type_supports_hash_capability(TypeHandle type) const;
    [[nodiscard]] bool validate_generic_arguments(
        const GenericTemplateInfo& info, const std::vector<TypeHandle>& args, const base::SourceRange& use_range);
    [[nodiscard]] bool has_generic_params(const syntax::ItemNode& item) const noexcept;
    [[nodiscard]] bool has_generic_constraints(const syntax::ItemNode& item) const noexcept;
    void register_value_names();
    void register_enum_cases_for_item(const syntax::ItemNode& item, syntax::ModuleId owner, TypeHandle named_enum_type,
        std::string enum_display_name, const std::string& case_prefix, const std::string& c_prefix,
        syntax::Visibility visibility, const query::GenericInstanceKey& generic_instance_key);
    void validate_function_prototypes() const;
    void validate_exported_signature_surfaces() const;
    void validate_abi_symbols() const;
    void validate_type_layouts();
    void validate_module_namespace_conflicts() const;
    void analyze_entry_points() const;
    void resolve_type_alias_decls();
    void analyze_struct_properties();
    void analyze_const_decls();
    void analyze_function_body(const syntax::ItemNode& function, syntax::ItemId function_id);
    void analyze_function_body_with_signature(const syntax::ItemNode& function, const FunctionLookupKey& key,
        const FunctionSignature& signature, FunctionBodyState& state);
    void analyze_borrow_escapes(const syntax::ItemNode& function);
    void analyze_body_moves(const syntax::ItemNode& function, const FunctionSignature& signature);
    void collect_body_flow_graph(const syntax::ItemNode& function, const FunctionLookupKey& key);
    void analyze_generic_function_definition(const GenericTemplateInfo& info);
    void analyze_generic_function_body(const syntax::ItemNode& function, const GenericTemplateInfo& info,
        const FunctionSignature& signature, FunctionBodyState& state);
    void analyze_block(syntax::StmtId block, TypeHandle expected_return, ReturnTypeInference* return_inference);
    void analyze_block_statements(
        syntax::StmtId block, TypeHandle expected_return, ReturnTypeInference* return_inference);
    void analyze_stmt(syntax::StmtId stmt, TypeHandle expected_return, ReturnTypeInference* return_inference);
    void analyze_statement_tree(syntax::StmtId root, TypeHandle expected_return, ReturnTypeInference* return_inference,
        StatementAnalysisRootKind root_kind);
    void analyze_statement_action(const StatementAnalysisAction& action, std::vector<StatementAnalysisAction>& stack,
        TypeHandle expected_return, ReturnTypeInference* return_inference);
    void analyze_statement_node(syntax::StmtId stmt, std::vector<StatementAnalysisAction>& stack,
        TypeHandle expected_return, ReturnTypeInference* return_inference);
    void analyze_statement_block(syntax::StmtId block, std::vector<StatementAnalysisAction>& stack) const;
    void analyze_pattern_scoped_block(syntax::PatternId pattern, TypeHandle pattern_type, syntax::StmtId block,
        std::vector<StatementAnalysisAction>& stack);
    void analyze_for_condition(syntax::StmtId stmt);
    [[nodiscard]] TypeHandle analyze_for_range_bounds(syntax::StmtId stmt_id, const syntax::StmtNode& stmt);
    void define_for_range_local(const syntax::StmtNode& stmt, TypeHandle type);
    [[nodiscard]] TypeHandle analyze_assignment_target(syntax::ExprId expr);
    [[nodiscard]] bool block_guarantees_return(syntax::StmtId block) const;
    [[nodiscard]] bool stmt_guarantees_return(syntax::StmtId stmt) const;
    [[nodiscard]] bool block_may_fallthrough(syntax::StmtId block) const;
    [[nodiscard]] bool stmt_may_fallthrough(syntax::StmtId stmt) const;
    void record_inferred_return(syntax::StmtId stmt, TypeHandle actual, ReturnTypeInference& inference);
    void finalize_inferred_return(
        const syntax::ItemNode& function, const FunctionLookupKey& key, ReturnTypeInference& inference);
    void resolve_pending_null_returns(ReturnTypeInference& inference);
    void report_return_inference_diagnostic(syntax::StmtId stmt, std::string_view message) const;
    void validate_function_return_type(const syntax::ItemNode& function, TypeHandle return_type) const;
    void ensure_function_return_known(const FunctionSignature& signature, const base::SourceRange& use_range);

    // Expression analysis contract:
    // - analyze_expr(expr, expected) owns final-type cache lookup and expected-type key recording.
    // - category helpers may recurse through analyze_expr and must record the current expr result.
    // - intrinsic/final split and coercion recording stay in the typed helper that creates the adjustment.
    [[nodiscard]] TypeHandle analyze_expr(syntax::ExprId expr);
    [[nodiscard]] TypeHandle analyze_expr(syntax::ExprId expr, TypeHandle expected_type);
    [[nodiscard]] ExprView expr_view(syntax::ExprId expr) const noexcept;
    [[nodiscard]] TypeHandle analyze_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_literal_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_value_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_control_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_operator_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_projection_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_aggregate_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_builtin_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_name_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_generic_apply_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_unary_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_binary_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] BinaryExprAnalysis analyze_binary_operands(const ExprView& expr, TypeHandle expected_type);
    void diagnose_binary_operand_mismatch(const ExprView& expr, const BinaryExprAnalysis& analysis) const;
    void diagnose_binary_literal_hazards(const ExprView& expr, TypeHandle lhs) const;
    void diagnose_binary_rhs_literal_hazards(const ExprView& expr, TypeHandle lhs) const;
    void diagnose_signed_binary_literal_overflow(const ExprView& expr, TypeHandle lhs) const;
    [[nodiscard]] TypeHandle record_binary_operator_expr(
        syntax::ExprId expr_id, const ExprView& expr, const BinaryExprAnalysis& analysis);
    [[nodiscard]] TypeHandle record_ordering_binary_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle lhs);
    [[nodiscard]] TypeHandle record_equality_binary_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle lhs, bool null_pointer_comparison);
    [[nodiscard]] TypeHandle record_logical_binary_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle lhs, TypeHandle rhs);
    [[nodiscard]] TypeHandle record_integer_binary_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle result_intrinsic, TypeHandle lhs);
    [[nodiscard]] TypeHandle record_numeric_binary_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle result_intrinsic, TypeHandle lhs);
    [[nodiscard]] TypeHandle analyze_field_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_index_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_slice_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_struct_literal_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_cast_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_size_or_align_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_ptr_addr_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_paddr_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_slice_projection_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_str_projection_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_str_utf8_slice_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_str_from_bytes_unchecked_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_call_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_explicit_generic_method_call_expr(
        syntax::ExprId expr_id, const ExprView& expr, const ExprView& generic_apply, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_enum_constructor_call(
        syntax::ExprId expr_id, const ExprView& expr, const EnumCaseInfo& enum_case);
    [[nodiscard]] TypeHandle analyze_field_call_expr(syntax::ExprId expr_id, const ExprView& expr,
        const ExprView& callee, std::string_view name, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_function_call_expr(syntax::ExprId expr_id, const ExprView& expr,
        const ExprView& callee, std::string_view name, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_function_value_call_expr(
        syntax::ExprId expr_id, const ExprView& expr, std::string_view name);
    [[nodiscard]] TypeHandle analyze_explicit_generic_function_call_expr(
        syntax::ExprId expr_id, const ExprView& expr, const ExprView& apply, std::string_view name);
    void validate_call_arguments(const ExprView& expr, std::string_view name, std::span<const TypeHandle> param_types,
        base::usize receiver_count, bool is_variadic);
    [[nodiscard]] TypeHandle analyze_try_expr(syntax::ExprId expr_id, const ExprView& expr);
    [[nodiscard]] TypeHandle analyze_if_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_block_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_unsafe_block_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_match_expr(syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_array_literal_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_tuple_literal_expr(
        syntax::ExprId expr_id, const ExprView& expr, TypeHandle expected_type);
    void define_local_pattern(
        syntax::PatternId pattern, TypeHandle type, bool is_mutable, bool allow_refutable = false);
    [[nodiscard]] bool analyze_pattern(
        syntax::PatternId pattern, TypeHandle matched, std::vector<PatternBinding>& bindings);
    [[nodiscard]] bool pattern_is_irrefutable(syntax::PatternId pattern, TypeHandle matched) const;
    void define_pattern_bindings(const std::vector<PatternBinding>& bindings, bool is_mutable);
    void analyze_single_value_pattern(syntax::PatternId pattern, TypeHandle matched, bool& covered_true,
        bool& covered_false, bool& saw_wildcard) const;
    [[nodiscard]] TypeHandle resolve_type(syntax::TypeId type);
    [[nodiscard]] TypeHandle resolve_type(syntax::TypeId type, bool opaque_allowed_as_pointee);
    [[nodiscard]] TypeHandle resolve_named_type(
        syntax::TypeId type_id, const syntax::TypeNode& type, bool opaque_allowed_as_pointee);
    [[nodiscard]] TypeHandle instantiate_generic_struct(const GenericTemplateInfo& info,
        const syntax::TypeNode& use_type, syntax::TypeId use_type_id, const std::vector<TypeHandle>& args);
    [[nodiscard]] TypeHandle instantiate_generic_enum(const GenericTemplateInfo& info, const syntax::TypeNode& use_type,
        syntax::TypeId use_type_id, const std::vector<TypeHandle>& args);
    [[nodiscard]] TypeHandle instantiate_generic_type_alias(const GenericTemplateInfo& info,
        const syntax::TypeNode& use_type, syntax::TypeId use_type_id, const std::vector<TypeHandle>& args,
        bool opaque_allowed_as_pointee);
    [[nodiscard]] TypeHandle resolve_type_alias(const TypeAliasInfo& alias, bool opaque_allowed_as_pointee);
    [[nodiscard]] bool infer_generic_arguments(
        const GenericTemplateInfo& info, const ExprView& call, std::vector<TypeHandle>& args);
    [[nodiscard]] bool infer_generic_method_arguments(const GenericTemplateInfo& info, TypeHandle owner_type,
        const ExprView& call, base::usize receiver_count, std::vector<TypeHandle>& args);
    [[nodiscard]] bool apply_explicit_generic_method_arguments(const GenericTemplateInfo& info, TypeHandle owner_type,
        std::span<const syntax::TypeId> explicit_type_args, const base::SourceRange& use_range,
        std::vector<TypeHandle>& args);
    [[nodiscard]] bool unify_generic_type(TypeHandle pattern, TypeHandle actual,
        std::unordered_map<GenericParamIdentity, TypeHandle, GenericParamIdentityHash>& inferred) const;
    [[nodiscard]] const GenericTemplateInfo* find_generic_function_in_visible_modules(
        IdentId name_id, std::string_view name, const base::SourceRange& range, bool report_unknown = true);
    [[nodiscard]] const GenericTemplateInfo* find_generic_function_in_module(syntax::ModuleId module, IdentId name_id,
        std::string_view name, const base::SourceRange& range, bool report_unknown = true);
    [[nodiscard]] const GenericTemplateInfo* find_generic_struct_in_visible_modules(
        IdentId name_id, std::string_view name, const base::SourceRange& range, bool report_unknown = true);
    [[nodiscard]] const GenericTemplateInfo* find_generic_struct_in_module(syntax::ModuleId module, IdentId name_id,
        std::string_view name, const base::SourceRange& range, bool report_unknown = true);
    [[nodiscard]] const GenericTemplateInfo* find_generic_enum_in_visible_modules(
        IdentId name_id, std::string_view name, const base::SourceRange& range, bool report_unknown = true);
    [[nodiscard]] const GenericTemplateInfo* find_generic_enum_in_module(syntax::ModuleId module, IdentId name_id,
        std::string_view name, const base::SourceRange& range, bool report_unknown = true);
    [[nodiscard]] const GenericTemplateInfo* find_generic_type_alias_in_visible_modules(
        IdentId name_id, std::string_view name, const base::SourceRange& range, bool report_unknown = true);
    [[nodiscard]] const GenericTemplateInfo* find_generic_type_alias_in_module(syntax::ModuleId module, IdentId name_id,
        std::string_view name, const base::SourceRange& range, bool report_unknown = true);
    [[nodiscard]] bool generic_type_template_exists_in_module(
        syntax::ModuleId module, IdentId name_id, std::string_view name) const;
    [[nodiscard]] const GenericTemplateInfo* find_any_generic_type_template_in_module(
        syntax::ModuleId module, IdentId name_id, std::string_view name) const;
    [[nodiscard]] bool report_generic_type_requires_args_if_visible(
        IdentId name_id, std::string_view name, const base::SourceRange& range);
    void report_generic_type_template_in_module(
        syntax::ModuleId module, IdentId name_id, std::string_view name, const base::SourceRange& range);
    [[nodiscard]] FunctionSignature* instantiate_generic_function(
        const GenericTemplateInfo& info, const std::vector<TypeHandle>& args, const base::SourceRange& use_range);
    [[nodiscard]] FunctionSignature* instantiate_generic_method(const GenericTemplateInfo& info, TypeHandle owner_type,
        const std::vector<TypeHandle>& args, const base::SourceRange& use_range);
    [[nodiscard]] FunctionSignature* instantiate_generic_placeholder_function(
        const GenericTemplateInfo& info, const std::vector<TypeHandle>& args, const base::SourceRange& use_range);
    [[nodiscard]] FunctionSignature* find_generic_method_in_visible_modules(TypeHandle owner_type, IdentId name_id,
        std::string_view name, const base::SourceRange& range, bool require_self, bool report_unknown = true,
        const ExprView* call = nullptr, base::usize receiver_count = 0, bool has_explicit_type_args = false,
        std::span<const syntax::TypeId> explicit_type_args = {}, bool* saw_matching_template = nullptr);
    [[nodiscard]] bool type_contains_generic_param(TypeHandle type) const;
    void populate_generic_param_identities(GenericTemplateInfo& info);
    [[nodiscard]] GenericParamIdentity make_generic_param_identity(
        const GenericTemplateInfo& info, base::usize index) const;
    [[nodiscard]] std::string_view generic_param_name(const GenericTemplateInfo& info, base::usize index) const;
    [[nodiscard]] GenericParamIdentity generic_param_identity(const GenericTemplateInfo& info, base::usize index) const;
    [[nodiscard]] GenericParamIdentity generic_param_identity(const TypeInfo& info) const;
    [[nodiscard]] TypeHandle generic_param_placeholder(const GenericTemplateInfo& info, base::usize index);
    void populate_generic_placeholder_context(const GenericTemplateInfo& info, GenericContext& context);
    void populate_generic_concrete_context(
        const GenericTemplateInfo& info, const std::vector<TypeHandle>& args, GenericContext& context) const;
    [[nodiscard]] std::string generic_instance_key_suffix(const std::vector<TypeHandle>& args) const;
    [[nodiscard]] std::string generic_instance_abi_suffix(const query::GenericInstanceKey& key) const;
    [[nodiscard]] std::string generic_instance_key(
        const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    [[nodiscard]] std::string generic_struct_instance_key(
        const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    [[nodiscard]] std::string generic_enum_instance_key(
        const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    [[nodiscard]] std::string generic_type_alias_instance_key(
        const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    [[nodiscard]] std::string generic_function_instance_key(
        const GenericTemplateInfo& info, const std::vector<TypeHandle>& args) const;
    [[nodiscard]] query::PackageKey query_package_key(syntax::ModuleId module) const noexcept;
    [[nodiscard]] query::ModuleKey query_module_key(syntax::ModuleId module) const noexcept;
    [[nodiscard]] query::ModulePartKey query_module_part_key(
        syntax::ModuleId module, base::u32 part_index) const noexcept;
    [[nodiscard]] query::ModulePartKey query_module_part_key(syntax::ItemId item) const noexcept;
    [[nodiscard]] DeclContext declaration_context(syntax::ModuleId module) const noexcept;
    [[nodiscard]] DeclContext declaration_context(syntax::ItemId item) const noexcept;
    [[nodiscard]] AccessContext current_access_context() const noexcept;
    [[nodiscard]] query::DefKey generic_template_query_key(
        const GenericTemplateInfo& info, query::DefNamespace name_space) const noexcept;
    [[nodiscard]] std::optional<query::DefKey> canonical_nominal_type_query_key(
        TypeHandle handle, const TypeInfo& info) const;
    [[nodiscard]] std::optional<query::GenericParamKey> canonical_generic_param_query_key(
        const GenericTemplateInfo& owner, query::DefKey owner_key, const TypeInfo& info) const;
    [[nodiscard]] query::ParamEnvKey generic_param_env_key(const GenericTemplateInfo& info) const;
    [[nodiscard]] base::Result<GenericInstanceIdentity> generic_instance_identity(
        const GenericTemplateInfo& info, std::span<const TypeHandle> args, query::DefNamespace name_space) const;
    [[nodiscard]] base::Result<std::string> generic_instance_signature_fingerprint(const GenericTemplateInfo& info,
        const GenericInstanceIdentity& identity, TypeHandle return_type, std::span<const TypeHandle> param_types,
        bool is_method, bool is_variadic) const;
    [[nodiscard]] base::Result<std::string> generic_struct_instance_signature_fingerprint(
        const GenericTemplateInfo& info, const GenericInstanceIdentity& identity, const StructInfo& struct_info) const;
    [[nodiscard]] base::Result<std::string> generic_enum_instance_signature_fingerprint(
        const GenericTemplateInfo& info, const GenericInstanceIdentity& identity, TypeHandle enum_type) const;
    [[nodiscard]] base::Result<std::string> generic_type_alias_instance_signature_fingerprint(
        const GenericTemplateInfo& info, const GenericInstanceIdentity& identity, TypeHandle target_type) const;
    [[nodiscard]] bool can_assign(TypeHandle dst, TypeHandle src, syntax::ExprId value) const noexcept;
    [[nodiscard]] bool is_valid_storage_type(TypeHandle type) const;
    [[nodiscard]] bool check_m2_value_abi(
        TypeHandle type, ValueAbiContext context, const base::SourceRange& range) const;
    [[nodiscard]] bool is_valid_cast(syntax::ExprKind kind, TypeHandle dst, TypeHandle src) const;
    [[nodiscard]] bool parse_integer_literal_text(std::string_view text, base::u64& value) const noexcept;
    [[nodiscard]] bool integer_literal_fits_type(TypeHandle destination, std::string_view text) const noexcept;
    [[nodiscard]] bool negative_integer_literal_fits_type(TypeHandle destination, std::string_view text) const noexcept;
    [[nodiscard]] TypeHandle analyze_integer_literal(
        syntax::ExprId expr, std::string_view text, const base::SourceRange& range, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_negative_integer_literal(
        syntax::ExprId expr, std::string_view text, const base::SourceRange& range, TypeHandle expected_type);
    [[nodiscard]] TypeHandle analyze_float_literal(
        syntax::ExprId expr, std::string_view text, const base::SourceRange& range, TypeHandle expected_type);
    [[nodiscard]] bool is_const_evaluable_expr(syntax::ExprId expr, ModuleLookupSet& dependencies);
    [[nodiscard]] TypeAbiLayout abi_layout(TypeHandle type) const;
    [[nodiscard]] base::u64 abi_size(TypeHandle type) const;
    [[nodiscard]] base::u64 abi_align(TypeHandle type) const;
    [[nodiscard]] base::u32 target_pointer_bit_width() const noexcept;
    [[nodiscard]] bool is_integer_literal(syntax::ExprId expr) const noexcept;
    [[nodiscard]] bool is_null_literal(syntax::ExprId expr) const noexcept;
    [[nodiscard]] bool is_null_result_expr(syntax::ExprId expr) const noexcept;
    [[nodiscard]] bool is_place_expr(syntax::ExprId expr);
    [[nodiscard]] bool is_writable_place(syntax::ExprId expr);
    [[nodiscard]] bool is_array_containing_value_type(TypeHandle type) const noexcept;
    [[nodiscard]] const StructInfo* find_struct(TypeHandle type) const noexcept;
    [[nodiscard]] ModuleSelector resolve_module_selector(syntax::ExprId expr, bool report_unknown) const;
    [[nodiscard]] NamedTypeSelector resolve_named_type_selector(syntax::ExprId expr, bool report_unknown);
    [[nodiscard]] TypeHandle resolve_type_selector(syntax::ExprId expr, bool report_unknown);
    [[nodiscard]] TypeHandle resolve_named_type_selector_type(
        const NamedTypeSelector& selector, bool opaque_allowed_as_pointee, bool report_unknown);
    [[nodiscard]] TypeHandle resolve_generic_type_selector(const NamedTypeSelector& selector,
        syntax::TypeId use_type_id, bool opaque_allowed_as_pointee, bool report_unknown);
    [[nodiscard]] bool selector_base_has_non_module_meaning(IdentId name_id, std::string_view name) const;
    [[nodiscard]] bool module_alias_visible(std::string_view name) const;
    [[nodiscard]] bool visible_root_module_name_exists(std::string_view name) const;
    [[nodiscard]] bool visible_module_path_prefix_exists(const std::vector<std::string_view>& parts) const;
    [[nodiscard]] ModuleSelectorPath expr_selector_path(syntax::ExprId expr) const;
    [[nodiscard]] bool current_generic_param_exists(IdentId name_id, std::string_view name) const;
    [[nodiscard]] bool visible_type_name_exists(IdentId name_id, std::string_view name) const;
    [[nodiscard]] bool can_define_local_name(
        IdentId name_id, std::string_view name, const base::SourceRange& range) const;
    [[nodiscard]] bool module_type_or_value_name_exists(
        syntax::ModuleId module, IdentId name_id, std::string_view name) const;
    [[nodiscard]] bool top_level_value_name_exists(
        syntax::ModuleId module, IdentId name_id, std::string_view name) const;
    [[nodiscard]] bool type_member_name_exists(TypeHandle owner_type, IdentId name_id, std::string_view name) const;
    [[nodiscard]] const FunctionSignature* find_function_selector(syntax::ExprId callee, IdentId name_id,
        std::string_view name, const base::SourceRange& range, bool report_unknown);
    [[nodiscard]] const GenericTemplateInfo* find_generic_function_selector(
        const NamedTypeSelector& selector, const base::SourceRange& range, bool report_unknown);
    [[nodiscard]] TypeHandle analyze_module_member_expr(
        syntax::ExprId expr_id, syntax::ModuleId module, const ExprView& expr);
    [[nodiscard]] bool record_no_payload_enum_case_expr(
        syntax::ExprId expr_id, const EnumCaseInfo& enum_case, const base::SourceRange& range);
    [[nodiscard]] TypeHandle resolve_associated_type_owner(syntax::ExprId object, bool report_unknown);
    [[nodiscard]] TypeHandle resolve_associated_generic_type_owner(syntax::ExprId apply, bool report_unknown);
    [[nodiscard]] TypeHandle function_type_from_signature(const FunctionSignature& signature);
    [[nodiscard]] TypeHandle function_type_from_symbol(const Symbol& symbol, const base::SourceRange& range);
    [[nodiscard]] TryShape classify_try_shape(TypeHandle type) const noexcept;
    [[nodiscard]] bool in_unsafe_context() const noexcept;
    void require_unsafe_context(const base::SourceRange& range, std::string_view operation) const;
    void validate_unsafe_call(const FunctionSignature& signature, const base::SourceRange& range) const;
    void validate_unsafe_function_value_call(TypeHandle callee_type, const base::SourceRange& range) const;
    [[nodiscard]] syntax::ModuleId item_module(syntax::ItemId item) const noexcept;
    [[nodiscard]] base::u32 item_part_index(syntax::ItemId item) const noexcept;
    [[nodiscard]] const syntax::ItemImportScope* item_import_scope(syntax::ItemId item) const noexcept;
    [[nodiscard]] std::span<const syntax::ResolvedImport> imports_for_scope(syntax::ModuleId module) const noexcept;
    [[nodiscard]] bool uses_item_import_scope(syntax::ModuleId module) const noexcept;
    [[nodiscard]] bool import_alias_exists_outside_current_scope(std::string_view alias) const noexcept;
    [[nodiscard]] syntax::ModuleId resolve_import_alias(
        std::string_view alias, const base::SourceRange& range, bool report_unknown = true) const;
    [[nodiscard]] std::vector<std::string_view> type_scope_parts(const syntax::TypeNode& type) const;
    [[nodiscard]] syntax::ModuleId resolve_type_scope(const syntax::TypeNode& type, bool report_unknown);
    [[nodiscard]] syntax::ModuleId find_visible_module_path(const std::vector<std::string_view>& parts) const;
    [[nodiscard]] const ModuleIdList& visible_modules(syntax::ModuleId module) const;
    [[nodiscard]] const ModuleIdList& module_export_modules(syntax::ModuleId module) const;
    [[nodiscard]] ModuleIdList accessible_module_export_modules(syntax::ModuleId module) const;
    [[nodiscard]] SelectiveReexportTargetList accessible_selective_reexports(
        syntax::ModuleId module, IdentId name_id, std::string_view name) const;
    void append_public_reexports(
        syntax::ModuleId module, ModuleIdList& result, std::unordered_set<base::u32>& seen) const;
    [[nodiscard]] std::string module_name(syntax::ModuleId module) const;
    [[nodiscard]] std::string qualified_name(syntax::ModuleId module, std::string_view name) const;
    [[nodiscard]] std::string_view nearest_visible_value_name(std::string_view name) const;
    [[nodiscard]] std::string_view nearest_value_name_in_module(syntax::ModuleId module, std::string_view name) const;
    [[nodiscard]] std::string_view nearest_visible_type_name(std::string_view name) const;
    [[nodiscard]] std::string_view nearest_type_name_in_module(syntax::ModuleId module, std::string_view name) const;
    [[nodiscard]] std::string_view nearest_visible_function_name(std::string_view name) const;
    [[nodiscard]] std::string_view nearest_function_name_in_module(
        syntax::ModuleId module, std::string_view name) const;
    [[nodiscard]] std::string_view nearest_import_alias_name(std::string_view name) const;
    [[nodiscard]] std::string_view nearest_field_name(const StructInfo& info, std::string_view name) const;
    [[nodiscard]] std::string_view nearest_enum_case_name(TypeHandle enum_type, std::string_view name) const;
    [[nodiscard]] std::string_view nearest_visible_enum_case_name(std::string_view name) const;
    [[nodiscard]] std::string c_symbol_name(syntax::ModuleId module, std::string_view name) const;
    [[nodiscard]] std::string generic_template_key_prefix(
        syntax::ModuleId module, IdentId name_id, std::string_view fallback_name = {}) const;
    [[nodiscard]] FunctionLookupKey function_key(const syntax::ItemNode& function, syntax::ItemId function_id);
    [[nodiscard]] std::string method_c_symbol_name(TypeHandle owner_type, std::string_view name) const;
    [[nodiscard]] std::string trait_impl_method_key_name(const syntax::ItemNode& function) const;
    [[nodiscard]] std::string trait_impl_method_c_symbol_name(
        TypeHandle owner_type, std::string_view trait_key, std::string_view name) const;
    [[nodiscard]] ModuleLookupKey module_lookup_key(syntax::ModuleId module, IdentId name) const noexcept;
    [[nodiscard]] MethodLookupKey method_lookup_key(
        syntax::ModuleId module, TypeHandle owner_type, IdentId name) const noexcept;
    [[nodiscard]] FunctionLookupKey function_lookup_key(syntax::ModuleId module, IdentId name) const noexcept;
    [[nodiscard]] FunctionLookupKey method_function_lookup_key(
        syntax::ModuleId module, TypeHandle owner_type, IdentId name) const noexcept;
    [[nodiscard]] FunctionLookupKey function_lookup_key_from_method(MethodLookupKey key) const noexcept;
    [[nodiscard]] StableModuleId stable_module_id(syntax::ModuleId module) const noexcept;
    [[nodiscard]] StableDefId stable_definition_id(syntax::ModuleId module, StableSymbolKind kind, IdentId name_id,
        std::string_view fallback_name, base::u32 disambiguator = 0) const;
    [[nodiscard]] StableMemberKey stable_member_key(const StableDefId& owner, StableSymbolKind kind, IdentId name_id,
        std::string_view fallback_name, base::u32 disambiguator = 0) const;
    [[nodiscard]] IncrementalKey stable_incremental_key(
        const StableDefId& definition, std::string_view semantic_fingerprint) const;
    [[nodiscard]] std::string function_incremental_fingerprint(std::string_view name, TypeHandle return_type,
        std::span<const TypeHandle> param_types, bool is_method, bool is_variadic) const;
    [[nodiscard]] InternedText source_name_text(IdentId name_id, std::string_view fallback_name);
    [[nodiscard]] IdentId intern_generated_key(std::string_view key);
    [[nodiscard]] ModuleLookupKey intern_module_lookup_key(syntax::ModuleId module, IdentId name) const noexcept;
    [[nodiscard]] ModuleLookupKey find_module_lookup_key(syntax::ModuleId module, IdentId name) const noexcept;
    [[nodiscard]] MethodLookupKey intern_method_lookup_key(
        syntax::ModuleId module, TypeHandle owner_type, IdentId name) const noexcept;
    [[nodiscard]] MethodLookupKey find_method_lookup_key(
        syntax::ModuleId module, TypeHandle owner_type, IdentId name) const noexcept;
    void index_named_type(syntax::ModuleId module, IdentId name_id, TypeHandle type, syntax::Visibility visibility);
    void index_type_alias(const TypeAliasInfo& info);
    void index_generic_struct_template(const GenericTemplateInfo& info);
    void index_generic_enum_template(const GenericTemplateInfo& info);
    void index_generic_type_alias_template(const GenericTemplateInfo& info);
    void index_generic_function_template(const GenericTemplateInfo& info);
    void index_generic_method_template(const GenericTemplateInfo& info);
    void index_function_lookup(const FunctionSignature& signature);
    void index_method_lookup(
        syntax::ModuleId module, TypeHandle owner_type, IdentId name_id, const FunctionSignature& signature);
    void index_function_value(const FunctionSignature& signature);
    void index_global_value(const Symbol& symbol);
    [[nodiscard]] bool named_type_lookup_complete() const noexcept;
    [[nodiscard]] bool type_alias_lookup_complete() const noexcept;
    [[nodiscard]] bool generic_struct_lookup_complete() const noexcept;
    [[nodiscard]] bool generic_enum_lookup_complete() const noexcept;
    [[nodiscard]] bool generic_type_alias_lookup_complete() const noexcept;
    [[nodiscard]] bool generic_function_lookup_complete() const noexcept;
    [[nodiscard]] bool generic_method_lookup_complete() const noexcept;
    [[nodiscard]] bool function_lookup_complete() const noexcept;
    [[nodiscard]] bool global_value_lookup_complete() const noexcept;
    [[nodiscard]] bool enum_case_module_lookup_complete() const noexcept;
    [[nodiscard]] bool can_access(const DeclContext& declaration, syntax::Visibility visibility) const noexcept;
    [[nodiscard]] bool can_access_module(syntax::ModuleId owner, syntax::Visibility visibility) const noexcept;
    void record_stmt_local_type(syntax::StmtId stmt, TypeHandle type);
    void record_expr_c_name(syntax::ExprId expr, std::string_view c_name);
    void record_pattern_c_name(syntax::PatternId pattern, std::string_view c_name);
    void record_pattern_case_name(syntax::PatternId pattern, std::string_view c_name);
    void merge_pattern_case_names(syntax::PatternId pattern, syntax::PatternId alternative);
    void record_syntax_type_handle(syntax::TypeId type, TypeHandle resolved);
    [[nodiscard]] bool method_receiver_matches(
        const FunctionSignature& signature, TypeHandle receiver_type, syntax::ExprId receiver);
    [[nodiscard]] syntax::ModuleId owner_module(TypeHandle owner_type) const noexcept;
    [[nodiscard]] const FunctionSignature* find_method_in_owner_module(
        TypeHandle owner_type, IdentId name_id, std::string_view name, bool require_self) const;
    [[nodiscard]] const FunctionSignature* find_method_in_visible_modules(TypeHandle owner_type, IdentId name_id,
        std::string_view name, const base::SourceRange& range, bool require_self, bool report_unknown = true);
    [[nodiscard]] TypeHandle find_type_in_visible_modules(IdentId name_id, std::string_view name,
        const base::SourceRange& range, bool opaque_allowed_as_pointee, bool report_unknown = true);
    [[nodiscard]] TypeHandle find_type_in_module(syntax::ModuleId module, IdentId name_id, std::string_view name,
        const base::SourceRange& range, bool opaque_allowed_as_pointee, bool report_unknown = true);
    [[nodiscard]] const FunctionSignature* find_function_in_visible_modules(
        IdentId name_id, std::string_view name, const base::SourceRange& range, bool report_unknown = true);
    [[nodiscard]] const FunctionSignature* find_function_in_module(syntax::ModuleId module, IdentId name_id,
        std::string_view name, const base::SourceRange& range, bool report_unknown = true);
    [[nodiscard]] const EnumCaseInfo* find_enum_case_in_visible_modules(
        IdentId name_id, std::string_view name, const base::SourceRange& range, bool report_unknown = true);
    [[nodiscard]] const EnumCaseInfo* find_enum_case_by_type_and_case(
        TypeHandle enum_type, IdentId case_name_id, std::string_view case_name) const;
    [[nodiscard]] const EnumCaseList* find_enum_cases_by_type(TypeHandle enum_type) const noexcept;
    [[nodiscard]] const EnumCaseInfo* find_enum_case_by_scoped_name(IdentId enum_name_id, std::string_view enum_name,
        IdentId case_name_id, std::string_view case_name, const base::SourceRange& range, bool report_unknown = true);
    [[nodiscard]] const EnumCaseInfo* find_enum_case_by_pattern_type(
        syntax::TypeId enum_type, IdentId case_name_id, std::string_view case_name, const base::SourceRange& range);
    [[nodiscard]] const EnumCaseInfo* find_enum_constructor(syntax::ExprId callee, bool report_unknown);
    [[nodiscard]] const Symbol* find_symbol(IdentId name_id, std::string_view name, const base::SourceRange& range);
    [[nodiscard]] const Symbol* find_symbol_in_module(syntax::ModuleId module, IdentId name_id, std::string_view name,
        const base::SourceRange& range, bool report_unknown = true);
    [[nodiscard]] TypeHandle record_expr_intrinsic_type(syntax::ExprId expr, TypeHandle type);
    [[nodiscard]] TypeHandle record_expr_types(syntax::ExprId expr, TypeHandle intrinsic_type, TypeHandle final_type);
    [[nodiscard]] TypeHandle record_expr_type(syntax::ExprId expr, TypeHandle type);
    void record_expr_expected_type(syntax::ExprId expr, TypeHandle expected_type);
    void record_expr_owned_use_mode(syntax::ExprId expr, OwnedUseMode mode);
    void record_coercion(syntax::ExprId expr, TypeHandle from_type, TypeHandle to_type, CoercionKind kind);
    [[nodiscard]] TypeHandle cached_expr_intrinsic_type(syntax::ExprId expr) const noexcept;
    [[nodiscard]] TypeHandle cached_expr_type(syntax::ExprId expr) const noexcept;
    [[nodiscard]] TypeHandle cached_expr_expected_type(syntax::ExprId expr) const noexcept;
    [[nodiscard]] OwnedUseMode cached_expr_owned_use_mode(syntax::ExprId expr) const noexcept;
    [[nodiscard]] TypeHandle cached_expr_type_for_expected(
        syntax::ExprId expr, TypeHandle expected_type) const noexcept;
    [[nodiscard]] TypeHandle cached_syntax_type(syntax::TypeId type) const noexcept;
    [[nodiscard]] TypeHandle cached_stmt_local_type(syntax::StmtId stmt) const noexcept;
    [[nodiscard]] std::string_view cached_expr_c_name(syntax::ExprId expr) const noexcept;
    [[nodiscard]] std::string_view cached_pattern_c_name(syntax::PatternId pattern) const noexcept;
    [[nodiscard]] SemaTypeTable& active_expr_intrinsic_types() noexcept;
    [[nodiscard]] SemaTypeTable& active_expr_types() noexcept;
    [[nodiscard]] SemaTypeTable& active_expr_expected_types() noexcept;
    [[nodiscard]] SemaOwnedUseModeTable& active_expr_owned_use_modes() noexcept;
    [[nodiscard]] SemaIdentTable& active_expr_c_name_ids() noexcept;
    [[nodiscard]] SemaIdentTable& active_pattern_c_name_ids() noexcept;
    [[nodiscard]] PatternCaseNameTable& active_pattern_case_name_ids() noexcept;
    [[nodiscard]] SemaTypeTable& active_syntax_type_handles() noexcept;
    [[nodiscard]] SemaTypeTable& active_stmt_local_types() noexcept;
    [[nodiscard]] PlaceInfo analyze_place_info(syntax::ExprId expr_id, bool emit_diagnostics);
    void require_place_projection_safety(const PlaceInfo& place, const base::SourceRange& range);
    [[nodiscard]] syntax::TypeId push_synthetic_type(syntax::TypeNode node);
    void ensure_expr_side_table_size(base::usize size);
    void ensure_type_side_table_size(base::usize size);
    void index_enum_case(const EnumCaseInfo& info);
    [[nodiscard]] GenericTemplateList& generic_method_template_bucket(const ModuleLookupKey& key);
    [[nodiscard]] EnumCaseList& enum_case_type_bucket(TypeHandle enum_type);
    [[nodiscard]] ModuleIdList make_module_id_list() const;
    [[nodiscard]] SemanticServiceBundle services() noexcept;
    [[nodiscard]] SemanticLookupService lookup_service() noexcept;
    [[nodiscard]] SemanticTypeService type_service() noexcept;
    [[nodiscard]] SemanticGenericService generic_service() noexcept;
    [[nodiscard]] SemanticBodyCheckService body_check_service() noexcept;
    [[nodiscard]] BuiltinExpressionAnalyzer builtin_expression_analyzer() noexcept;
    [[nodiscard]] ControlExpressionAnalyzer control_expression_analyzer() noexcept;
    [[nodiscard]] ExpressionAnalyzer expression_analyzer() noexcept;
    [[nodiscard]] LookupResolver lookup_resolver() noexcept;
    [[nodiscard]] LookupResolver lookup_resolver() const noexcept;
    [[nodiscard]] OperatorExpressionAnalyzer operator_expression_analyzer() noexcept;
    [[nodiscard]] ProjectionAggregateExpressionAnalyzer projection_aggregate_expression_analyzer() noexcept;
    [[nodiscard]] SemanticTypeResolver type_resolver() noexcept;
    [[nodiscard]] SemanticTypeValidator type_validator() const noexcept;
    [[nodiscard]] SemanticAbiChecker abi_checker() const noexcept;
    [[nodiscard]] SemanticSideTableStore side_table_store() noexcept;
    [[nodiscard]] SemanticSideTableReader side_table_reader() const noexcept;
    [[nodiscard]] SemanticDiagnosticReporter diagnostic_reporter() const noexcept;
    void report(const base::SourceRange& range, SemanticDiagnosticKind kind, std::string message) const;
    void report_general(const base::SourceRange& range, std::string message) const;
    void report_type(const base::SourceRange& range, std::string message) const;
    void report_lookup(const base::SourceRange& range, std::string message) const;
    void report_duplicate(const base::SourceRange& range, std::string message) const;
    void report_visibility(const base::SourceRange& range, std::string message) const;
    void report_unsupported(const base::SourceRange& range, std::string message) const;
    void report_unsafe_required(const base::SourceRange& range, std::string message) const;
    void report_capability(const base::SourceRange& range, std::string message) const;
    void report_pattern(const base::SourceRange& range, std::string message) const;
    void report_pattern_exhaustiveness(const base::SourceRange& range, std::string message) const;
    void report_pattern_unreachable(const base::SourceRange& range, std::string message) const;
    void report_internal_contract(const base::SourceRange& range, std::string message) const;
    void report(const base::SourceRange& range, std::string message, base::DiagnosticCategory category,
        base::DiagnosticCode code) const;
    void report_note(const base::SourceRange& range, SemanticDiagnosticKind kind, std::string message) const;
    void report_help(const base::SourceRange& range, SemanticDiagnosticKind kind, std::string message) const;
    void report_type_mismatch(
        const base::SourceRange& range, std::string message, TypeHandle expected, TypeHandle actual) const;
    void report_lookup_suggestion(const base::SourceRange& range, std::string_view suggestion) const;

    std::optional<syntax::AstModule> owned_module_;
    SemaContext ctx_;
    SemaState state_;

    friend class ModuleVisibilityResolver;
    friend class SemanticAnalysisPipeline;
    friend class SemanticAbiChecker;
    friend class SemanticBodyCheckService;
    friend class SemanticSideTableReader;
    friend class SemanticSideTableStore;
    friend class SemanticGenericService;
    friend class SemanticLookupService;
    friend class SemanticServiceBundle;
    friend class SemanticTypeService;
    friend class SemanticTypeResolver;
    friend class SemanticTypeValidator;
};

} // namespace aurex::sema
