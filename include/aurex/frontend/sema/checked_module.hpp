#pragma once

#include <aurex/frontend/sema/function.hpp>
#include <aurex/frontend/sema/storage.hpp>
#include <aurex/frontend/sema/type.hpp>
#include <aurex/frontend/syntax/core/ast.hpp>
#include <aurex/infrastructure/query/generic_instance_key.hpp>

#include <algorithm>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace aurex::query {
struct TypeCheckBodyAuthority;
} // namespace aurex::query

namespace aurex::sema {

using CNameIdSet = SemaSet<IdentId, IdentIdHash>;
using SemaTypeTable = SemaVector<TypeHandle>;
using SemaIdentTable = SemaVector<IdentId>;
using SemaIndexTable = SemaVector<base::u32>;

enum class OwnedUseMode : base::u8 {
    none,
    owned_copy,
    owned_consume,
    shared_borrow,
    mutable_borrow,
    place_only,
};

using SemaOwnedUseModeTable = SemaVector<OwnedUseMode>;

[[nodiscard]] std::string_view owned_use_mode_name(OwnedUseMode mode) noexcept;

inline constexpr base::usize SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX = static_cast<base::usize>(-1);
inline constexpr base::usize SEMA_GENERIC_SIDE_TABLE_BLOCK_BYTES = 1024U;
inline constexpr base::usize SEMA_PATTERN_CASE_NAME_TABLE_BLOCK_BYTES = 1024U;
inline constexpr base::usize SEMA_GENERIC_SIDE_TABLE_INVALID_LAYOUT_INDEX = static_cast<base::usize>(-1);
inline constexpr base::u32 SEMA_TRAIT_PREDICATE_INVALID_INDEX = static_cast<base::u32>(-1);

enum class CapabilityKind {
    sized,
    eq,
    ord,
    hash,
    copy,
};

struct CapabilityKindHash {
    [[nodiscard]] std::size_t operator()(const CapabilityKind kind) const noexcept
    {
        return static_cast<std::size_t>(kind);
    }
};

[[nodiscard]] std::string_view capability_name(CapabilityKind capability) noexcept;

class PatternCaseNameTable final {
public:
    using Map = SemaMap<base::u32, CNameIdSet>;
    using iterator = Map::iterator;
    using const_iterator = Map::const_iterator;

    PatternCaseNameTable();
    PatternCaseNameTable(const PatternCaseNameTable& other);
    PatternCaseNameTable& operator=(const PatternCaseNameTable& other);
    PatternCaseNameTable(PatternCaseNameTable&& other) noexcept;
    PatternCaseNameTable& operator=(PatternCaseNameTable&& other) noexcept;
    ~PatternCaseNameTable() = default;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] base::usize size() const noexcept;
    [[nodiscard]] bool contains(base::u32 pattern) const;
    void reserve(base::usize pattern_count);
    void clear() noexcept;

    [[nodiscard]] iterator begin() noexcept;
    [[nodiscard]] iterator end() noexcept;
    [[nodiscard]] const_iterator begin() const noexcept;
    [[nodiscard]] const_iterator end() const noexcept;
    [[nodiscard]] const_iterator find(base::u32 pattern) const;
    [[nodiscard]] iterator find(base::u32 pattern);
    [[nodiscard]] CNameIdSet& operator[](base::u32 pattern);
    void insert(base::u32 pattern, IdentId c_name_id);
    void merge(base::u32 pattern, const CNameIdSet& source);

    [[nodiscard]] base::usize arena_bytes() const noexcept;
    [[nodiscard]] base::usize arena_blocks() const noexcept;

#if defined(AUREX_SEMA_WHITEBOX_TESTS)
public:
#else
private:
#endif
    [[nodiscard]] CNameIdSet make_bucket();
    void ensure_storage();
    void swap(PatternCaseNameTable& other) noexcept;
    void copy_from(const PatternCaseNameTable& other);

    std::unique_ptr<base::BumpAllocator> arena_;
    Map names_;
};

struct StructFieldInfo {
    InternedText name;
    IdentId name_id = INVALID_IDENT_ID;
    InternedText c_name;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    TypeHandle type = INVALID_TYPE_HANDLE;
    base::SourceRange range{};
    syntax::Visibility visibility = syntax::Visibility::public_;
    StableMemberKey stable_key;
};

struct StructInfo {
    InternedText name;
    IdentId name_id = INVALID_IDENT_ID;
    InternedText c_name;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    TypeHandle type = INVALID_TYPE_HANDLE;
    SemaVector<StructFieldInfo> fields;
    bool is_opaque = false;
    bool is_generic_placeholder = false;
    syntax::Visibility visibility = syntax::Visibility::public_;
    StableDefId stable_id;
    IncrementalKey incremental_key;
    query::GenericInstanceKey generic_instance_key;
    base::u32 part_index = 0;
};

struct EnumCaseInfo {
    InternedText name;
    IdentId name_id = INVALID_IDENT_ID;
    InternedText c_name;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    TypeHandle type = INVALID_TYPE_HANDLE;
    TypeHandle payload_type = INVALID_TYPE_HANDLE;
    TypeHandleList payload_types;
    InternedText value_text;
    base::SourceRange range{};
    InternedText enum_name;
    InternedText case_name;
    IdentId case_name_id = INVALID_IDENT_ID;
    syntax::Visibility visibility = syntax::Visibility::public_;
    StableDefId stable_id;
    StableMemberKey stable_case_key;
    IncrementalKey incremental_key;
    query::GenericInstanceKey generic_instance_key;
    base::u32 part_index = 0;
};

struct TypeAliasInfo {
    InternedText name;
    IdentId name_id = INVALID_IDENT_ID;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    syntax::TypeId target = syntax::INVALID_TYPE_ID;
    base::SourceRange range{};
    syntax::Visibility visibility = syntax::Visibility::public_;
    StableDefId stable_id;
    IncrementalKey incremental_key;
    base::u32 part_index = 0;
};

enum class BorrowContractSelectorKind : base::u8 {
    parameter,
    self,
    static_,
    unknown,
};

enum class FunctionBorrowContractSource : base::u8 {
    inferred,
    declared,
    conservative_unknown,
};

struct BorrowContractSelector {
    BorrowContractSelectorKind kind = BorrowContractSelectorKind::parameter;
    base::u32 param_index = SEMA_TRAIT_PREDICATE_INVALID_INDEX;
    IdentId name_id = INVALID_IDENT_ID;
    base::SourceRange range{};
};

struct FunctionBorrowContract {
    FunctionLookupKey function;
    TypeHandle return_type = INVALID_TYPE_HANDLE;
    std::vector<BorrowContractSelector> return_selectors;
    FunctionBorrowContractSource source = FunctionBorrowContractSource::inferred;
    bool return_type_can_contain_borrow = false;
    bool unknown_return_allowed = false;
    bool has_local_return_escape = false;
    bool has_contract_mismatch = false;
    base::SourceRange range{};
    query::StableFingerprint128 fingerprint;
    base::u32 part_index = 0;
};

struct TraitMethodRequirement {
    InternedText name;
    IdentId name_id = INVALID_IDENT_ID;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    TypeHandle return_type = INVALID_TYPE_HANDLE;
    TypeHandleList param_types;
    syntax::StmtId default_body = syntax::INVALID_STMT_ID;
    base::SourceRange range{};
    bool is_unsafe = false;
    bool is_variadic = false;
    bool has_self_param = false;
    bool has_default_body = false;
    bool has_borrow_contract = false;
    syntax::Visibility visibility = syntax::Visibility::public_;
    StableMemberKey stable_key;
    FunctionBorrowContract borrow_contract;
    base::u32 ordinal = 0;
};

using TraitMethodRequirementList = SemaVector<TraitMethodRequirement>;

struct TraitAssociatedTypeRequirement {
    InternedText name;
    IdentId name_id = INVALID_IDENT_ID;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    base::SourceRange range{};
    syntax::Visibility visibility = syntax::Visibility::public_;
    StableMemberKey stable_key;
    query::MemberKey member_key;
    base::u32 ordinal = 0;
};

using TraitAssociatedTypeRequirementList = SemaVector<TraitAssociatedTypeRequirement>;

struct TraitSignature {
    InternedText name;
    IdentId name_id = INVALID_IDENT_ID;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    syntax::Visibility visibility = syntax::Visibility::public_;
    StableDefId stable_id;
    IncrementalKey incremental_key;
    SemaVector<IdentId> generic_params;
    TraitAssociatedTypeRequirementList associated_types;
    TraitMethodRequirementList requirements;
    base::SourceRange range{};
    base::u32 part_index = 0;
};

struct TraitImplLookupKey {
    base::u32 trait_module = SEMA_LOOKUP_INVALID_KEY_PART;
    IdentId trait_name = INVALID_IDENT_ID;
    base::u32 self_type = TypeHandle::INVALID_VALUE;
    query::StableFingerprint128 trait_args;

    [[nodiscard]] friend constexpr bool operator==(TraitImplLookupKey lhs, TraitImplLookupKey rhs) noexcept = default;
};

[[nodiscard]] inline constexpr bool is_valid(const TraitImplLookupKey key) noexcept
{
    return key.trait_module != SEMA_LOOKUP_INVALID_KEY_PART && is_valid(key.trait_name)
        && key.self_type != TypeHandle::INVALID_VALUE;
}

struct TraitImplLookupKeyHash {
    [[nodiscard]] std::size_t operator()(TraitImplLookupKey key) const noexcept;
};

enum class TraitImplMethodOrigin {
    impl_override,
    trait_default,
};

struct TraitImplMethodInfo {
    InternedText name;
    IdentId name_id = INVALID_IDENT_ID;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    FunctionLookupKey function_key;
    base::u32 requirement_ordinal = 0;
    TraitImplMethodOrigin origin = TraitImplMethodOrigin::impl_override;
};

using TraitImplMethodInfoList = SemaVector<TraitImplMethodInfo>;

struct TraitImplAssociatedTypeInfo {
    InternedText name;
    IdentId name_id = INVALID_IDENT_ID;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    syntax::TypeId syntax_type = syntax::INVALID_TYPE_ID;
    TypeHandle value_type = INVALID_TYPE_HANDLE;
    query::MemberKey member_key;
    base::u32 requirement_ordinal = 0;
};

using TraitImplAssociatedTypeInfoList = SemaVector<TraitImplAssociatedTypeInfo>;

struct TraitImplInfo {
    TraitImplLookupKey key;
    InternedText trait_name;
    IdentId trait_name_id = INVALID_IDENT_ID;
    syntax::ModuleId trait_module = syntax::INVALID_MODULE_ID;
    TypeHandle self_type = INVALID_TYPE_HANDLE;
    TypeHandleList trait_args;
    query::StableFingerprint128 coherence_fingerprint;
    base::u32 predicate_index = SEMA_TRAIT_PREDICATE_INVALID_INDEX;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    syntax::Visibility visibility = syntax::Visibility::public_;
    StableDefId stable_id;
    IncrementalKey incremental_key;
    TraitImplAssociatedTypeInfoList associated_types;
    TraitImplMethodInfoList methods;
    base::SourceRange range{};
    base::u32 part_index = 0;
};

enum class TraitPredicateKind {
    builtin,
    declared_trait,
};

enum class TraitPredicateOrigin {
    explicit_where,
    explicit_impl,
    trait_self,
};

enum class TraitEvidenceKind {
    param_env,
    builtin,
    explicit_impl,
};

enum class TraitMethodDispatchKind {
    param_env,
    impl_override,
    trait_default,
};

struct TraitPredicate {
    base::u32 index = SEMA_TRAIT_PREDICATE_INVALID_INDEX;
    TraitPredicateKind kind = TraitPredicateKind::declared_trait;
    TraitPredicateOrigin origin = TraitPredicateOrigin::explicit_where;
    TypeHandle subject_type = INVALID_TYPE_HANDLE;
    IdentId subject_param_name_id = INVALID_IDENT_ID;
    GenericParamIdentity subject_param_identity = INVALID_GENERIC_PARAM_IDENTITY;
    base::u32 subject_param_index = SEMA_TRAIT_PREDICATE_INVALID_INDEX;
    CapabilityKind builtin_capability = CapabilityKind::sized;
    InternedText trait_name;
    IdentId trait_name_id = INVALID_IDENT_ID;
    syntax::ModuleId trait_module = syntax::INVALID_MODULE_ID;
    StableDefId trait_stable_id;
    TypeHandleList trait_args;
    TraitImplAssociatedTypeInfoList associated_type_equalities;
    query::StableFingerprint128 canonical_fingerprint;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    base::SourceRange range{};
    base::u32 part_index = 0;
};

struct TraitObligation {
    base::u32 predicate_index = SEMA_TRAIT_PREDICATE_INVALID_INDEX;
    query::StableFingerprint128 predicate_fingerprint;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    base::SourceRange range{};
    base::u32 part_index = 0;
};

struct TraitEvidence {
    TraitEvidenceKind kind = TraitEvidenceKind::param_env;
    base::u32 predicate_index = SEMA_TRAIT_PREDICATE_INVALID_INDEX;
    query::StableFingerprint128 predicate_fingerprint;
    TraitImplLookupKey impl_key;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    base::SourceRange range{};
    base::u32 part_index = 0;
};

enum class ReceiverAccessKind : base::u8 {
    none,
    shared,
    mutable_,
    consuming,
};

struct TraitMethodCallBinding {
    syntax::ExprId call_expr = syntax::INVALID_EXPR_ID;
    syntax::ExprId callee_expr = syntax::INVALID_EXPR_ID;
    TraitMethodDispatchKind dispatch = TraitMethodDispatchKind::param_env;
    base::u32 predicate_index = SEMA_TRAIT_PREDICATE_INVALID_INDEX;
    query::StableFingerprint128 predicate_fingerprint;
    TraitImplLookupKey impl_key;
    FunctionLookupKey function_key;
    syntax::ModuleId trait_module = syntax::INVALID_MODULE_ID;
    IdentId trait_name_id = INVALID_IDENT_ID;
    InternedText method_name;
    IdentId method_name_id = INVALID_IDENT_ID;
    base::u32 requirement_ordinal = SEMA_TRAIT_PREDICATE_INVALID_INDEX;
    TypeHandle receiver_type = INVALID_TYPE_HANDLE;
    TypeHandle self_type = INVALID_TYPE_HANDLE;
    TypeHandle return_type = INVALID_TYPE_HANDLE;
    ReceiverAccessKind receiver_access = ReceiverAccessKind::none;
    bool receiver_auto_borrow = false;
    bool receiver_two_phase_eligible = false;
    base::SourceRange range{};
    base::u32 part_index = 0;
};

struct FunctionCallBinding {
    syntax::ExprId call_expr = syntax::INVALID_EXPR_ID;
    syntax::ExprId callee_expr = syntax::INVALID_EXPR_ID;
    FunctionLookupKey function_key;
    TypeHandle return_type = INVALID_TYPE_HANDLE;
    base::u32 receiver_arg_count = 0;
    ReceiverAccessKind receiver_access = ReceiverAccessKind::none;
    bool receiver_auto_borrow = false;
    bool receiver_two_phase_eligible = false;
    base::SourceRange range{};
    base::u32 part_index = 0;
};

enum class BorrowSummaryOriginKind : base::u8 {
    none,
    parameter,
    static_,
    local,
    temporary,
    unknown,
};

struct BorrowSummaryOrigin {
    BorrowSummaryOriginKind kind = BorrowSummaryOriginKind::none;
    base::u32 param_index = SEMA_TRAIT_PREDICATE_INVALID_INDEX;
    IdentId name_id = INVALID_IDENT_ID;
    syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    base::SourceRange range{};
    bool storage_slot = false;
};

inline constexpr base::u32 SEMA_BORROW_SUMMARY_INVALID_INDEX = static_cast<base::u32>(-1);

struct FunctionBorrowReturnOrigin {
    base::u32 origin_index = SEMA_BORROW_SUMMARY_INVALID_INDEX;
    syntax::ExprId return_expr = syntax::INVALID_EXPR_ID;
    base::SourceRange range{};
};

struct FunctionBorrowStorageEscape {
    base::u32 origin_index = SEMA_BORROW_SUMMARY_INVALID_INDEX;
    syntax::ExprId stored_expr = syntax::INVALID_EXPR_ID;
    base::SourceRange range{};
};

struct FunctionBorrowSummary {
    FunctionLookupKey function;
    TypeHandle return_type = INVALID_TYPE_HANDLE;
    std::vector<BorrowSummaryOrigin> origins;
    std::vector<FunctionBorrowReturnOrigin> return_origins;
    std::vector<FunctionBorrowStorageEscape> storage_escapes;
    bool return_type_can_contain_borrow = false;
    bool has_unknown_return_origin = false;
    bool has_local_return_escape = false;
    query::StableFingerprint128 fingerprint;
    base::u32 part_index = 0;
};

struct LifetimeOriginParamInfo {
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    InternedText name;
    IdentId name_id = INVALID_IDENT_ID;
    base::SourceRange range{};
    base::u32 ordinal = 0;
    base::u32 part_index = 0;
};

struct ReferenceOriginFact {
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    syntax::TypeId syntax_type = syntax::INVALID_TYPE_ID;
    TypeHandle semantic_type = INVALID_TYPE_HANDLE;
    std::vector<InternedText> origin_names;
    std::vector<IdentId> origin_name_ids;
    base::SourceRange range{};
    base::u32 part_index = 0;
};

enum class LifetimeRegionKind : base::u8 {
    parameter,
    self,
    static_,
    explicit_origin,
    inferred,
    local,
    temporary,
    unknown,
};

enum class LifetimeConstraintReason : base::u8 {
    declared_origin,
    reference_type,
    return_contract,
    return_type,
    call,
    reborrow,
    dropck,
};

enum class LifetimeViolationKind : base::u8 {
    unknown_origin,
    ambiguous_elision,
    return_origin_outside_type,
    local_escape,
    unknown_escape,
    type_outlives,
};

inline constexpr base::u32 SEMA_LIFETIME_INVALID_INDEX = static_cast<base::u32>(-1);
inline constexpr base::u32 SEMA_BODY_FLOW_INVALID_INDEX = static_cast<base::u32>(-1);

enum class GenericLifetimePredicateSource : base::u8 {
    explicit_origin,
    inferred_reference,
    associated_projection,
};

struct LifetimeRegion {
    LifetimeRegionKind kind = LifetimeRegionKind::inferred;
    IdentId name_id = INVALID_IDENT_ID;
    InternedText name;
    base::u32 param_index = SEMA_LIFETIME_INVALID_INDEX;
    base::SourceRange range{};
};

struct LifetimeOutlivesConstraint {
    base::u32 longer_region = SEMA_LIFETIME_INVALID_INDEX;
    base::u32 shorter_region = SEMA_LIFETIME_INVALID_INDEX;
    LifetimeConstraintReason reason = LifetimeConstraintReason::return_contract;
    base::SourceRange range{};
};

struct LifetimeTypeOutlivesConstraint {
    TypeHandle type = INVALID_TYPE_HANDLE;
    base::u32 region = SEMA_LIFETIME_INVALID_INDEX;
    LifetimeConstraintReason reason = LifetimeConstraintReason::reference_type;
    base::SourceRange range{};
};

struct LifetimeRegionLiveRange {
    base::u32 region = SEMA_LIFETIME_INVALID_INDEX;
    base::u32 first_point = SEMA_BODY_FLOW_INVALID_INDEX;
    base::u32 last_point = SEMA_BODY_FLOW_INVALID_INDEX;
    base::u32 point_count = 0;
    base::SourceRange range{};
};

struct LifetimeReturnRegion {
    base::u32 region = SEMA_LIFETIME_INVALID_INDEX;
    syntax::ExprId return_expr = syntax::INVALID_EXPR_ID;
    base::SourceRange range{};
};

struct LifetimeViolation {
    LifetimeViolationKind kind = LifetimeViolationKind::type_outlives;
    base::u32 region = SEMA_LIFETIME_INVALID_INDEX;
    base::u32 related_region = SEMA_LIFETIME_INVALID_INDEX;
    TypeHandle type = INVALID_TYPE_HANDLE;
    syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    bool diagnostic_emitted = false;
    base::SourceRange range{};
};

struct FunctionLifetimeFacts {
    FunctionLookupKey function;
    TypeHandle return_type = INVALID_TYPE_HANDLE;
    std::vector<LifetimeRegion> regions;
    std::vector<LifetimeOutlivesConstraint> outlives_constraints;
    std::vector<LifetimeTypeOutlivesConstraint> type_outlives_constraints;
    std::vector<LifetimeRegionLiveRange> live_ranges;
    std::vector<LifetimeReturnRegion> return_regions;
    std::vector<LifetimeViolation> violations;
    bool solved = false;
    bool diagnostic_mode_enforced = false;
    query::StableFingerprint128 fingerprint;
    base::u32 part_index = 0;
};

enum class DropCheckActionKind : base::u8 {
    lexical_cleanup,
    overwrite,
    early_exit,
    explicit_drop,
    defer_cleanup,
};

enum class DropCheckViolationKind : base::u8 {
    borrowed_drop,
    borrowed_field_dangling,
    generic_type_outlives,
    destructor_escape,
    drop_glue_missing,
};

inline constexpr base::u32 SEMA_DROP_CHECK_INVALID_ACTION_INDEX = static_cast<base::u32>(-1);

struct DropCheckRequiredOutlives {
    TypeHandle type = INVALID_TYPE_HANDLE;
    base::u32 region = SEMA_LIFETIME_INVALID_INDEX;
    LifetimeConstraintReason reason = LifetimeConstraintReason::dropck;
    base::SourceRange range{};
};

struct DropCheckFact {
    TypeHandle type = INVALID_TYPE_HANDLE;
    FunctionLookupKey destructor_function;
    std::vector<DropCheckRequiredOutlives> required_outlives;
    query::StableFingerprint128 drop_glue_fingerprint;
    query::StableFingerprint128 fingerprint;
    bool may_observe_fields = true;
    bool may_move_fields = false;
};

struct DropActionFact {
    DropCheckActionKind kind = DropCheckActionKind::lexical_cleanup;
    base::u32 action = SEMA_DROP_CHECK_INVALID_ACTION_INDEX;
    base::u32 point = SEMA_BODY_FLOW_INVALID_INDEX;
    base::u32 place = SEMA_BODY_FLOW_INVALID_INDEX;
    TypeHandle type = INVALID_TYPE_HANDLE;
    query::StableFingerprint128 destructor_key;
    base::SourceRange range{};
};

struct DropCheckViolation {
    DropCheckViolationKind kind = DropCheckViolationKind::generic_type_outlives;
    base::u32 action = SEMA_DROP_CHECK_INVALID_ACTION_INDEX;
    base::u32 point = SEMA_BODY_FLOW_INVALID_INDEX;
    base::u32 place = SEMA_BODY_FLOW_INVALID_INDEX;
    TypeHandle type = INVALID_TYPE_HANDLE;
    base::u32 region = SEMA_LIFETIME_INVALID_INDEX;
    bool diagnostic_emitted = false;
    base::SourceRange range{};
};

struct FunctionDropCheckFacts {
    FunctionLookupKey function;
    std::vector<DropCheckFact> facts;
    std::vector<DropActionFact> actions;
    std::vector<DropCheckViolation> violations;
    bool solved = false;
    bool diagnostic_mode_enforced = false;
    bool graph_missing = false;
    query::StableFingerprint128 fingerprint;
    base::u32 part_index = 0;
};

struct TypeLifetimeInfo {
    TypeHandle type = INVALID_TYPE_HANDLE;
    std::vector<InternedText> origin_names;
    bool can_contain_borrow = false;
    bool has_concrete_borrow_surface = false;
    query::StableFingerprint128 fingerprint;
    base::u32 part_index = 0;
};

struct GenericLifetimePredicate {
    TypeHandle subject_type = INVALID_TYPE_HANDLE;
    InternedText origin_name;
    IdentId origin_name_id = INVALID_IDENT_ID;
    GenericLifetimePredicateSource source = GenericLifetimePredicateSource::inferred_reference;
    base::SourceRange range{};
    query::StableFingerprint128 fingerprint;
    base::u32 part_index = 0;
};

struct ParamEnvInfo {
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    InternedText owner_name;
    IdentId owner_name_id = INVALID_IDENT_ID;
    StableDefId owner_stable_id;
    query::ParamEnvKey key;
    SemaIndexTable predicate_indices;
    base::SourceRange range{};
    base::u32 part_index = 0;
};

struct GenericTemplateSignatureInfo {
    InternedText name;
    IdentId name_id = INVALID_IDENT_ID;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    syntax::Visibility visibility = syntax::Visibility::public_;
    StableDefId stable_id;
    IncrementalKey incremental_key;
    query::DefNamespace name_space = query::DefNamespace::value;
    base::u64 param_count = 0;
    base::u64 constraint_count = 0;
    base::u32 part_index = 0;
};

struct GenericEnumInstanceInfo {
    ModuleLookupKey key;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    query::GenericInstanceKey generic_instance_key;
    TypeHandle type = INVALID_TYPE_HANDLE;
    StableDefId stable_id;
    IncrementalKey incremental_key;
    base::u32 part_index = 0;
};

struct GenericTypeAliasInstanceInfo {
    ModuleLookupKey key;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    query::GenericInstanceKey generic_instance_key;
    TypeHandle resolved_type = INVALID_TYPE_HANDLE;
    StableDefId stable_id;
    IncrementalKey incremental_key;
    base::u32 part_index = 0;
};

using CheckedFunctionMap = SemaMap<FunctionLookupKey, FunctionSignature, FunctionLookupKeyHash>;
using CheckedModuleInfoMap = SemaMap<ModuleLookupKey, StructInfo, ModuleLookupKeyHash>;
using CheckedEnumCaseMap = SemaMap<ModuleLookupKey, EnumCaseInfo, ModuleLookupKeyHash>;
using CheckedTypeAliasMap = SemaMap<ModuleLookupKey, TypeAliasInfo, ModuleLookupKeyHash>;
using CheckedTraitMap = SemaMap<ModuleLookupKey, TraitSignature, ModuleLookupKeyHash>;
using CheckedTraitImplMap = SemaMap<TraitImplLookupKey, TraitImplInfo, TraitImplLookupKeyHash>;
using TraitPredicateList = SemaVector<TraitPredicate>;
using TraitObligationList = SemaVector<TraitObligation>;
using TraitEvidenceList = SemaVector<TraitEvidence>;
using TraitMethodCallBindingList = SemaVector<TraitMethodCallBinding>;
using FunctionCallBindingList = SemaVector<FunctionCallBinding>;
using CallBindingExprIndexMap = SemaMap<base::u32, base::u32>;
using FunctionBorrowSummaryMap = SemaMap<FunctionLookupKey, FunctionBorrowSummary, FunctionLookupKeyHash>;
using FunctionBorrowContractMap = SemaMap<FunctionLookupKey, FunctionBorrowContract, FunctionLookupKeyHash>;
using LifetimeOriginParamList = SemaVector<LifetimeOriginParamInfo>;
using ReferenceOriginFactList = SemaVector<ReferenceOriginFact>;
using TypeLifetimeInfoList = SemaVector<TypeLifetimeInfo>;
using GenericLifetimePredicateList = SemaVector<GenericLifetimePredicate>;
using FunctionLifetimeFactsMap = SemaMap<FunctionLookupKey, FunctionLifetimeFacts, FunctionLookupKeyHash>;
using FunctionDropCheckFactsMap = SemaMap<FunctionLookupKey, FunctionDropCheckFacts, FunctionLookupKeyHash>;
using ParamEnvList = SemaVector<ParamEnvInfo>;

enum class BodyFlowPointKind : base::u8 {
    entry,
    exit,
    statement_entry,
    statement_exit,
    expression_entry,
    expression_exit,
    sequence,
    cleanup_scope,
};

enum class BodyFlowActionKind : base::u8 {
    read,
    write,
    reinit,
    move_candidate,
    drop,
    borrow_shared,
    borrow_mutable,
    call_receiver_reserve,
    call_receiver_activate,
    call,
    return_,
    branch,
    cleanup_scope,
    cleanup_storage,
};

enum class BodyFlowPlaceRootKind : base::u8 {
    none,
    local,
    temporary,
    unknown,
};

enum class BodyFlowPlaceProjectionKind : base::u8 {
    field,
    index,
    dereference,
    slice,
};

struct BodyFlowPlaceProjection {
    BodyFlowPlaceProjectionKind kind = BodyFlowPlaceProjectionKind::field;
    IdentId field_name_id = INVALID_IDENT_ID;
    syntax::ExprId expr = syntax::INVALID_EXPR_ID;
};

struct BodyFlowPlace {
    BodyFlowPlaceRootKind root_kind = BodyFlowPlaceRootKind::none;
    IdentId root_name_id = INVALID_IDENT_ID;
    syntax::ExprId root_expr = syntax::INVALID_EXPR_ID;
    std::vector<BodyFlowPlaceProjection> projections;
    base::SourceRange range{};
};

struct BodyFlowPoint {
    BodyFlowPointKind kind = BodyFlowPointKind::entry;
    syntax::StmtId stmt = syntax::INVALID_STMT_ID;
    syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    base::SourceRange range{};
};

struct BodyFlowEdge {
    base::u32 from = SEMA_BODY_FLOW_INVALID_INDEX;
    base::u32 to = SEMA_BODY_FLOW_INVALID_INDEX;
};

struct BodyFlowAction {
    BodyFlowActionKind kind = BodyFlowActionKind::read;
    base::u32 point = SEMA_BODY_FLOW_INVALID_INDEX;
    base::u32 place = SEMA_BODY_FLOW_INVALID_INDEX;
    syntax::StmtId stmt = syntax::INVALID_STMT_ID;
    syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    base::SourceRange range{};
};

struct BodyFlowGraph {
    FunctionLookupKey function;
    syntax::StmtId body = syntax::INVALID_STMT_ID;
    std::vector<BodyFlowPoint> points;
    std::vector<BodyFlowEdge> edges;
    std::vector<BodyFlowPlace> places;
    std::vector<BodyFlowAction> actions;
    bool collect_only = true;
};

using BodyFlowGraphMap = SemaMap<FunctionLookupKey, BodyFlowGraph, FunctionLookupKeyHash>;

inline constexpr base::u32 SEMA_BODY_LOAN_INVALID_INDEX = static_cast<base::u32>(-1);

enum class BodyLoanKind : base::u8 {
    shared,
    mutable_,
};

enum class BodyLoanOriginKind : base::u8 {
    none,
    local,
    temporary,
    unknown,
};

enum class BodyLoanDiagnosticMode : base::u8 {
    shadow,
    enforced,
};

enum class BodyLoanConflictKind : base::u8 {
    read,
    write,
    reinit,
    move,
    drop,
    shared_borrow,
    mutable_borrow,
    cleanup,
    reborrow_parent_use,
    two_phase_reservation,
    two_phase_activation,
};

struct BodyLoanOrigin {
    BodyLoanOriginKind kind = BodyLoanOriginKind::none;
    IdentId name_id = INVALID_IDENT_ID;
    syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    base::SourceRange range{};
};

struct BodyLoan {
    BodyLoanKind kind = BodyLoanKind::shared;
    BodyLoanOrigin origin;
    base::u32 parent_loan = SEMA_BODY_LOAN_INVALID_INDEX;
    base::u32 issued_action = SEMA_BODY_LOAN_INVALID_INDEX;
    base::u32 issued_point = SEMA_BODY_FLOW_INVALID_INDEX;
    base::u32 place = SEMA_BODY_FLOW_INVALID_INDEX;
    IdentId carrier_name_id = INVALID_IDENT_ID;
    base::u32 carrier_definition_point = SEMA_BODY_FLOW_INVALID_INDEX;
    syntax::StmtId enclosing_stmt = syntax::INVALID_STMT_ID;
    syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    base::SourceRange range{};
};

struct BodyTwoPhaseBorrow {
    base::u32 reservation_action = SEMA_BODY_LOAN_INVALID_INDEX;
    base::u32 activation_action = SEMA_BODY_LOAN_INVALID_INDEX;
    base::u32 reservation_point = SEMA_BODY_FLOW_INVALID_INDEX;
    base::u32 activation_point = SEMA_BODY_FLOW_INVALID_INDEX;
    base::u32 place = SEMA_BODY_FLOW_INVALID_INDEX;
    syntax::ExprId call_expr = syntax::INVALID_EXPR_ID;
    bool diagnostic_emitted = false;
    base::SourceRange range{};
};

struct BodyLoanConflict {
    BodyLoanConflictKind kind = BodyLoanConflictKind::write;
    base::u32 loan = SEMA_BODY_LOAN_INVALID_INDEX;
    base::u32 two_phase_borrow = SEMA_BODY_LOAN_INVALID_INDEX;
    base::u32 action = SEMA_BODY_LOAN_INVALID_INDEX;
    base::u32 point = SEMA_BODY_FLOW_INVALID_INDEX;
    base::u32 place = SEMA_BODY_FLOW_INVALID_INDEX;
    bool diagnostic_emitted = false;
    base::SourceRange range{};
    base::u32 later_use_point = SEMA_BODY_FLOW_INVALID_INDEX;
    base::SourceRange later_use_range{};
};

struct BodyLoanCheckResult {
    FunctionLookupKey function;
    BodyLoanDiagnosticMode diagnostic_mode = BodyLoanDiagnosticMode::shadow;
    std::vector<BodyLoanOrigin> origins;
    std::vector<BodyLoan> loans;
    std::vector<BodyTwoPhaseBorrow> two_phase_borrows;
    std::vector<BodyLoanConflict> conflicts;
    bool graph_missing = false;
};

using BodyLoanCheckResultMap = SemaMap<FunctionLookupKey, BodyLoanCheckResult, FunctionLookupKeyHash>;

[[nodiscard]] std::string_view receiver_access_kind_name(ReceiverAccessKind kind) noexcept;
[[nodiscard]] std::string_view body_loan_kind_name(BodyLoanKind kind) noexcept;
[[nodiscard]] std::string_view body_loan_origin_kind_name(BodyLoanOriginKind kind) noexcept;
[[nodiscard]] std::string_view body_loan_diagnostic_mode_name(BodyLoanDiagnosticMode mode) noexcept;
[[nodiscard]] std::string_view body_loan_conflict_kind_name(BodyLoanConflictKind kind) noexcept;
[[nodiscard]] std::string_view borrow_contract_selector_kind_name(BorrowContractSelectorKind kind) noexcept;
[[nodiscard]] std::string_view function_borrow_contract_source_name(FunctionBorrowContractSource source) noexcept;
[[nodiscard]] std::string_view lifetime_region_kind_name(LifetimeRegionKind kind) noexcept;
[[nodiscard]] std::string_view lifetime_constraint_reason_name(LifetimeConstraintReason reason) noexcept;
[[nodiscard]] std::string_view lifetime_violation_kind_name(LifetimeViolationKind kind) noexcept;
[[nodiscard]] std::string_view generic_lifetime_predicate_source_name(GenericLifetimePredicateSource source) noexcept;
[[nodiscard]] std::string_view drop_check_action_kind_name(DropCheckActionKind kind) noexcept;
[[nodiscard]] std::string_view drop_check_violation_kind_name(DropCheckViolationKind kind) noexcept;
[[nodiscard]] std::string_view drop_check_violation_message(DropCheckViolationKind kind) noexcept;
[[nodiscard]] query::StableFingerprint128 body_loan_check_fingerprint(const BodyLoanCheckResult& result) noexcept;
[[nodiscard]] query::StableFingerprint128 function_borrow_contract_fingerprint(
    const FunctionBorrowContract& contract) noexcept;
[[nodiscard]] query::StableFingerprint128 type_lifetime_info_fingerprint(const TypeLifetimeInfo& info) noexcept;
[[nodiscard]] query::StableFingerprint128 generic_lifetime_predicate_fingerprint(
    const GenericLifetimePredicate& predicate) noexcept;
[[nodiscard]] query::StableFingerprint128 function_lifetime_facts_fingerprint(
    const FunctionLifetimeFacts& facts) noexcept;
[[nodiscard]] query::StableFingerprint128 drop_check_fact_fingerprint(const DropCheckFact& fact) noexcept;
[[nodiscard]] query::StableFingerprint128 function_drop_check_facts_fingerprint(
    const FunctionDropCheckFacts& facts) noexcept;
[[nodiscard]] std::string dump_function_lifetime_facts(const FunctionLifetimeFacts& facts);
[[nodiscard]] std::string dump_function_drop_check_facts(const FunctionDropCheckFacts& facts);

enum class CoercionKind {
    contextual_integer_literal,
    contextual_float_literal,
    null_to_pointer,
    slice_to_expected_slice,
};

struct CoercionRecord {
    syntax::ExprId expr = syntax::INVALID_EXPR_ID;
    TypeHandle from_type = INVALID_TYPE_HANDLE;
    TypeHandle to_type = INVALID_TYPE_HANDLE;
    CoercionKind kind = CoercionKind::contextual_integer_literal;
};

struct GenericNodeSpan {
    base::u32 begin = 0;
    base::u32 count = 0;

    [[nodiscard]] bool empty() const noexcept
    {
        return this->count == 0;
    }

    [[nodiscard]] bool contains(const base::u32 value) const noexcept
    {
        return value >= this->begin && value - this->begin < this->count;
    }

    [[nodiscard]] base::usize local_index(const base::u32 value) const noexcept
    {
        return this->contains(value) ? static_cast<base::usize>(value - this->begin)
                                     : SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX;
    }
};

enum class GenericSparseFallbackKind {
    expr_intrinsic_type,
    expr_type,
    expr_expected_type,
    expr_owned_use_mode,
    expr_c_name,
    pattern_c_name,
    pattern_case_name,
    syntax_type,
    stmt_local_type,
};

struct GenericSparseFallbackStats {
    base::usize expr_intrinsic_types = 0;
    base::usize expr_types = 0;
    base::usize expr_expected_types = 0;
    base::usize expr_owned_use_modes = 0;
    base::usize expr_c_name_ids = 0;
    base::usize pattern_c_name_ids = 0;
    base::usize pattern_case_name_ids = 0;
    base::usize syntax_type_handles = 0;
    base::usize stmt_local_types = 0;

    [[nodiscard]] base::usize total() const noexcept
    {
        return this->expr_intrinsic_types + this->expr_types + this->expr_expected_types + this->expr_owned_use_modes
            + this->expr_c_name_ids + this->pattern_c_name_ids + this->pattern_case_name_ids + this->syntax_type_handles
            + this->stmt_local_types;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return this->total() == 0;
    }
};

struct GenericSideTableLayout {
    GenericNodeSpan expr_span;
    GenericNodeSpan pattern_span;
    GenericNodeSpan type_span;
    GenericNodeSpan stmt_span;
    SemaIndexTable expr_node_ids;
    SemaIndexTable pattern_node_ids;
    SemaIndexTable type_node_ids;
    SemaIndexTable stmt_node_ids;
};

struct GenericSideTableLocalLayoutView {
    GenericNodeSpan expr_span;
    GenericNodeSpan pattern_span;
    GenericNodeSpan type_span;
    GenericNodeSpan stmt_span;
    std::span<const base::u32> expr_node_ids{};
    std::span<const base::u32> pattern_node_ids{};
    std::span<const base::u32> type_node_ids{};
    std::span<const base::u32> stmt_node_ids{};
};

struct GenericSideTables {
#if defined(AUREX_SEMA_WHITEBOX_TESTS)
public:
#else
private:
#endif
    std::unique_ptr<base::BumpAllocator> arena_;
    std::unique_ptr<base::BumpAllocator> analysis_arena_;

public:
    GenericSideTables();
    GenericSideTables(const GenericSideTables& other);
    GenericSideTables& operator=(const GenericSideTables& other);
    GenericSideTables(GenericSideTables&& other) noexcept;
    GenericSideTables& operator=(GenericSideTables&& other) noexcept;
    ~GenericSideTables() = default;

    bool sparse = false;
    bool local_dense = false;
    GenericNodeSpan expr_span;
    GenericNodeSpan pattern_span;
    GenericNodeSpan type_span;
    GenericNodeSpan stmt_span;
    const GenericSideTableLayout* layout = nullptr;
    SemaIndexTable expr_node_ids;
    SemaIndexTable pattern_node_ids;
    SemaIndexTable type_node_ids;
    SemaIndexTable stmt_node_ids;
    SemaTypeTable expr_intrinsic_types;
    SemaTypeTable expr_types;
    SemaTypeTable expr_expected_types;
    SemaOwnedUseModeTable expr_owned_use_modes;
    SemaIdentTable expr_c_name_ids;
    SemaIdentTable pattern_c_name_ids;
    SemaTypeTable syntax_type_handles;
    SemaTypeTable stmt_local_types;
    SemaMap<base::u32, TypeHandle> sparse_expr_intrinsic_types;
    SemaMap<base::u32, TypeHandle> sparse_expr_types;
    SemaMap<base::u32, TypeHandle> sparse_expr_expected_types;
    SemaMap<base::u32, OwnedUseMode> sparse_expr_owned_use_modes;
    SemaMap<base::u32, IdentId> sparse_expr_c_name_ids;
    SemaMap<base::u32, IdentId> sparse_pattern_c_name_ids;
    PatternCaseNameTable pattern_case_name_ids;
    SemaMap<base::u32, TypeHandle> sparse_syntax_type_handles;
    SemaMap<base::u32, TypeHandle> sparse_stmt_local_types;
    GenericSparseFallbackStats sparse_fallbacks;

    [[nodiscard]] base::usize arena_bytes() const noexcept;
    [[nodiscard]] base::usize arena_blocks() const noexcept;
    void record_sparse_fallback(GenericSparseFallbackKind kind) noexcept;
    void configure_local_dense(
        GenericNodeSpan expr, GenericNodeSpan pattern, GenericNodeSpan type, GenericNodeSpan stmt);
    void configure_local_dense(const GenericSideTableLocalLayoutView& layout);
    void configure_local_dense(const GenericSideTableLayout& shared_layout);
    void bind_local_dense_layout(const GenericSideTableLayout& shared_layout) noexcept;
    void prepare_analysis_only_storage(base::usize expr_count);
    void release_analysis_only_storage();

    [[nodiscard]] base::usize local_expr_index(syntax::ExprId expr) const noexcept
    {
        return syntax::is_valid(expr) && this->local_dense
            ? this->local_index(expr.value, this->active_expr_span(), this->active_expr_node_ids())
            : SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX;
    }

    [[nodiscard]] base::usize local_pattern_index(syntax::PatternId pattern) const noexcept
    {
        return syntax::is_valid(pattern) && this->local_dense
            ? this->local_index(pattern.value, this->active_pattern_span(), this->active_pattern_node_ids())
            : SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX;
    }

    [[nodiscard]] base::usize local_type_index(syntax::TypeId type) const noexcept
    {
        return syntax::is_valid(type) && this->local_dense
            ? this->local_index(type.value, this->active_type_span(), this->active_type_node_ids())
            : SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX;
    }

    [[nodiscard]] base::usize local_stmt_index(syntax::StmtId stmt) const noexcept
    {
        return syntax::is_valid(stmt) && this->local_dense
            ? this->local_index(stmt.value, this->active_stmt_span(), this->active_stmt_node_ids())
            : SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX;
    }

#if defined(AUREX_SEMA_WHITEBOX_TESTS)
public:
#else
private:
#endif
    [[nodiscard]] GenericNodeSpan active_expr_span() const noexcept
    {
        return this->layout == nullptr ? this->expr_span : this->layout->expr_span;
    }

    [[nodiscard]] GenericNodeSpan active_pattern_span() const noexcept
    {
        return this->layout == nullptr ? this->pattern_span : this->layout->pattern_span;
    }

    [[nodiscard]] GenericNodeSpan active_type_span() const noexcept
    {
        return this->layout == nullptr ? this->type_span : this->layout->type_span;
    }

    [[nodiscard]] GenericNodeSpan active_stmt_span() const noexcept
    {
        return this->layout == nullptr ? this->stmt_span : this->layout->stmt_span;
    }

    [[nodiscard]] const SemaIndexTable& active_expr_node_ids() const noexcept
    {
        return this->layout == nullptr ? this->expr_node_ids : this->layout->expr_node_ids;
    }

    [[nodiscard]] const SemaIndexTable& active_pattern_node_ids() const noexcept
    {
        return this->layout == nullptr ? this->pattern_node_ids : this->layout->pattern_node_ids;
    }

    [[nodiscard]] const SemaIndexTable& active_type_node_ids() const noexcept
    {
        return this->layout == nullptr ? this->type_node_ids : this->layout->type_node_ids;
    }

    [[nodiscard]] const SemaIndexTable& active_stmt_node_ids() const noexcept
    {
        return this->layout == nullptr ? this->stmt_node_ids : this->layout->stmt_node_ids;
    }

    [[nodiscard]] static base::usize local_index(
        base::u32 value, GenericNodeSpan span, const SemaIndexTable& ids) noexcept
    {
        if (ids.empty()) {
            return span.local_index(value);
        }
        const auto found = std::ranges::lower_bound(ids, value);
        return found != ids.end() && *found == value ? static_cast<base::usize>(found - ids.begin())
                                                     : SEMA_GENERIC_SIDE_TABLE_MISSING_INDEX;
    }

    void swap(GenericSideTables& other) noexcept;
    void copy_from(const GenericSideTables& other);
};

struct GenericFunctionInstanceInfo {
    FunctionLookupKey key;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    syntax::StmtId body = syntax::INVALID_STMT_ID;
    query::GenericInstanceKey generic_instance_key;
    FunctionSignature signature;
    base::usize side_table_layout_index = SEMA_GENERIC_SIDE_TABLE_INVALID_LAYOUT_INDEX;
    GenericSideTables side_tables;
};

struct GenericFunctionInstanceBodyView {
    const GenericFunctionInstanceInfo* instance = nullptr;
    const FunctionSignature* signature = nullptr;
    const GenericSideTables* side_tables = nullptr;
    const syntax::ItemNode* item = nullptr;
    syntax::StmtId body = syntax::INVALID_STMT_ID;
};

struct TraitDefaultMethodInstanceInfo {
    FunctionLookupKey key;
    syntax::ItemId item = syntax::INVALID_ITEM_ID;
    syntax::StmtId body = syntax::INVALID_STMT_ID;
    TraitImplLookupKey impl_key;
    syntax::ModuleId trait_module = syntax::INVALID_MODULE_ID;
    IdentId trait_name_id = INVALID_IDENT_ID;
    base::u32 requirement_ordinal = SEMA_TRAIT_PREDICATE_INVALID_INDEX;
    FunctionSignature signature;
    base::usize side_table_layout_index = SEMA_GENERIC_SIDE_TABLE_INVALID_LAYOUT_INDEX;
    GenericSideTables side_tables;
};

struct TraitDefaultMethodInstanceBodyView {
    const TraitDefaultMethodInstanceInfo* instance = nullptr;
    const FunctionSignature* signature = nullptr;
    const GenericSideTables* side_tables = nullptr;
    const syntax::ItemNode* item = nullptr;
    syntax::StmtId body = syntax::INVALID_STMT_ID;
};

struct NormalizedAstOverlay {
    // Sema normalizes the caller-owned AST in place. The checked module keeps
    // only normalization bounds/flags, never an owning AST snapshot.
    base::u64 original_expr_count = 0;
    base::u64 original_type_count = 0;
    base::u64 final_expr_count = 0;
    base::u64 final_type_count = 0;
    bool parser_only_module_contract_added = false;

    [[nodiscard]] bool added_syntax_nodes() const noexcept
    {
        return final_expr_count > original_expr_count || final_type_count > original_type_count;
    }
};

struct CheckedModule {
#if defined(AUREX_SEMA_WHITEBOX_TESTS)
public:
#else
private:
#endif
    std::unique_ptr<base::BumpAllocator> arena_;
    std::unique_ptr<base::BumpAllocator> analysis_arena_;

public:
    CheckedModule();
    CheckedModule(const CheckedModule& other);
    CheckedModule& operator=(const CheckedModule& other);
    CheckedModule(CheckedModule&& other) noexcept;
    CheckedModule& operator=(CheckedModule&& other) noexcept;
    ~CheckedModule() = default;

    // CheckedModule is the bridge between syntax and codegen. It is deliberately
    // side-table based so AST nodes remain parse-only data.
    IdentifierInterner c_names;
    TypeTable types;
    SemaTypeTable expr_intrinsic_types;
    SemaTypeTable expr_types;
    SemaTypeTable expr_expected_types;
    SemaOwnedUseModeTable expr_owned_use_modes;
    SemaIdentTable expr_c_name_ids;
    SemaIdentTable pattern_c_name_ids;
    PatternCaseNameTable pattern_case_name_ids;
    SemaTypeTable syntax_type_handles;
    SemaTypeTable stmt_local_types;
    SemaIdentTable item_c_name_ids;
    SemaVector<CoercionRecord> coercions;
    CheckedFunctionMap functions;
    CheckedModuleInfoMap structs;
    CheckedEnumCaseMap enum_cases;
    CheckedTypeAliasMap type_aliases;
    CheckedTraitMap traits;
    CheckedTraitImplMap trait_impls;
    TraitPredicateList trait_predicates;
    TraitObligationList trait_obligations;
    TraitEvidenceList trait_evidence;
    TraitMethodCallBindingList trait_method_calls;
    FunctionCallBindingList function_calls;
    CallBindingExprIndexMap trait_method_call_by_expr;
    CallBindingExprIndexMap function_call_by_expr;
    FunctionBorrowSummaryMap borrow_summaries;
    FunctionBorrowContractMap borrow_contracts;
    LifetimeOriginParamList lifetime_origin_params;
    ReferenceOriginFactList reference_origin_facts;
    TypeLifetimeInfoList type_lifetime_infos;
    GenericLifetimePredicateList generic_lifetime_predicates;
    FunctionLifetimeFactsMap lifetime_facts;
    FunctionDropCheckFactsMap dropck_facts;
    BodyFlowGraphMap body_flow_graphs;
    BodyLoanCheckResultMap body_loan_checks;
    ParamEnvList param_envs;
    SemaVector<GenericTemplateSignatureInfo> generic_template_signatures;
    SemaDeque<GenericSideTableLayout> generic_side_table_layouts;
    SemaDeque<GenericEnumInstanceInfo> generic_enum_instances;
    SemaDeque<GenericTypeAliasInstanceInfo> generic_type_alias_instances;
    SemaDeque<GenericFunctionInstanceInfo> generic_function_instances;
    SemaDeque<TraitDefaultMethodInstanceInfo> trait_default_method_instances;
    NormalizedAstOverlay normalized_ast;

    [[nodiscard]] IdentId intern_c_name(const std::string_view c_name)
    {
        return this->intern_text(c_name).id;
    }

    [[nodiscard]] InternedText intern_text(const std::string_view text)
    {
        return sema::intern_text(this->c_names, text);
    }

    [[nodiscard]] std::string_view c_name_text(const IdentId id) const noexcept
    {
        return this->c_names.text(id);
    }

    [[nodiscard]] TypeHandleList make_type_handle_list() const;
    [[nodiscard]] TypeHandleList copy_type_handle_list(std::span<const TypeHandle> values) const;
    [[nodiscard]] SemaVector<StructFieldInfo> make_struct_field_list() const;
    [[nodiscard]] SemaVector<StructFieldInfo> copy_struct_field_list(std::span<const StructFieldInfo> values);
    [[nodiscard]] SemaIndexTable make_index_table() const;
    [[nodiscard]] SemaIndexTable copy_index_table(std::span<const base::u32> values) const;
    [[nodiscard]] FunctionSignature make_function_signature() const;
    [[nodiscard]] StructInfo make_struct_info() const;
    [[nodiscard]] EnumCaseInfo make_enum_case_info() const;
    [[nodiscard]] TraitMethodRequirement make_trait_method_requirement() const;
    [[nodiscard]] TraitAssociatedTypeRequirement make_trait_associated_type_requirement() const;
    [[nodiscard]] TraitSignature make_trait_signature() const;
    [[nodiscard]] TraitImplMethodInfo make_trait_impl_method_info() const;
    [[nodiscard]] TraitImplAssociatedTypeInfo make_trait_impl_associated_type_info() const;
    [[nodiscard]] TraitImplInfo make_trait_impl_info() const;
    [[nodiscard]] TraitPredicate make_trait_predicate() const;
    [[nodiscard]] TraitObligation make_trait_obligation() const;
    [[nodiscard]] TraitEvidence make_trait_evidence() const;
    [[nodiscard]] TraitMethodCallBinding make_trait_method_call_binding() const;
    [[nodiscard]] FunctionCallBinding make_function_call_binding() const;
    [[nodiscard]] LifetimeOriginParamInfo clone_lifetime_origin_param(const LifetimeOriginParamInfo& other);
    [[nodiscard]] ReferenceOriginFact clone_reference_origin_fact(const ReferenceOriginFact& other);
    [[nodiscard]] TypeLifetimeInfo clone_type_lifetime_info(const TypeLifetimeInfo& other);
    [[nodiscard]] GenericLifetimePredicate clone_generic_lifetime_predicate(
        const GenericLifetimePredicate& other);
    void append_trait_method_call_binding(TraitMethodCallBinding binding);
    void append_function_call_binding(FunctionCallBinding binding);
    [[nodiscard]] const TraitMethodCallBinding* trait_method_call_binding_for_expr(
        syntax::ExprId call_expr) const noexcept;
    [[nodiscard]] const FunctionCallBinding* function_call_binding_for_expr(syntax::ExprId call_expr) const noexcept;
    [[nodiscard]] ParamEnvInfo make_param_env_info() const;
    [[nodiscard]] GenericTemplateSignatureInfo clone_generic_template_signature_info(
        const GenericTemplateSignatureInfo& other);
    [[nodiscard]] GenericSideTableLayout make_generic_side_table_layout(
        const GenericSideTableLocalLayoutView& layout) const;
    [[nodiscard]] base::usize append_generic_side_table_layout(const GenericSideTableLocalLayoutView& layout);
    [[nodiscard]] const GenericSideTableLayout* generic_side_table_layout(base::usize index) const noexcept;
    [[nodiscard]] FunctionSignature clone_function_signature(const FunctionSignature& other);
    [[nodiscard]] FunctionLifetimeFacts clone_function_lifetime_facts(const FunctionLifetimeFacts& other);
    [[nodiscard]] FunctionDropCheckFacts clone_function_drop_check_facts(const FunctionDropCheckFacts& other);
    [[nodiscard]] StructInfo clone_struct_info(const StructInfo& other);
    [[nodiscard]] EnumCaseInfo clone_enum_case_info(const EnumCaseInfo& other);
    [[nodiscard]] TraitMethodRequirement clone_trait_method_requirement(const TraitMethodRequirement& other);
    [[nodiscard]] TraitAssociatedTypeRequirement clone_trait_associated_type_requirement(
        const TraitAssociatedTypeRequirement& other);
    [[nodiscard]] TraitSignature clone_trait_signature(const TraitSignature& other);
    [[nodiscard]] TraitImplMethodInfo clone_trait_impl_method_info(const TraitImplMethodInfo& other);
    [[nodiscard]] TraitImplAssociatedTypeInfo clone_trait_impl_associated_type_info(
        const TraitImplAssociatedTypeInfo& other);
    [[nodiscard]] TraitImplInfo clone_trait_impl_info(const TraitImplInfo& other);
    [[nodiscard]] TraitPredicate clone_trait_predicate(const TraitPredicate& other);
    [[nodiscard]] TraitObligation clone_trait_obligation(const TraitObligation& other) const;
    [[nodiscard]] TraitEvidence clone_trait_evidence(const TraitEvidence& other) const;
    [[nodiscard]] TraitMethodCallBinding clone_trait_method_call_binding(const TraitMethodCallBinding& other);
    [[nodiscard]] FunctionCallBinding clone_function_call_binding(const FunctionCallBinding& other) const;
    [[nodiscard]] ParamEnvInfo clone_param_env_info(const ParamEnvInfo& other);
    [[nodiscard]] GenericSideTableLayout clone_generic_side_table_layout(const GenericSideTableLayout& other) const;
    [[nodiscard]] GenericEnumInstanceInfo clone_generic_enum_instance(const GenericEnumInstanceInfo& other) const;
    [[nodiscard]] GenericTypeAliasInstanceInfo clone_generic_type_alias_instance(
        const GenericTypeAliasInstanceInfo& other) const;
    [[nodiscard]] GenericFunctionInstanceInfo clone_generic_function_instance(const GenericFunctionInstanceInfo& other);
    [[nodiscard]] TraitDefaultMethodInstanceInfo clone_trait_default_method_instance(
        const TraitDefaultMethodInstanceInfo& other);
    [[nodiscard]] GenericFunctionInstanceBodyView generic_function_instance_body_view(
        const syntax::AstModule& ast, base::usize index) const noexcept;
    [[nodiscard]] GenericFunctionInstanceBodyView generic_function_instance_body_view(
        const syntax::AstModule& ast, const GenericFunctionInstanceInfo& instance) const noexcept;
    [[nodiscard]] TraitDefaultMethodInstanceBodyView trait_default_method_instance_body_view(
        const syntax::AstModule& ast, base::usize index) const noexcept;
    [[nodiscard]] TraitDefaultMethodInstanceBodyView trait_default_method_instance_body_view(
        const syntax::AstModule& ast, const TraitDefaultMethodInstanceInfo& instance) const noexcept;
    void prepare_analysis_only_storage(base::usize expr_count);
    void release_analysis_only_storage();
    void reserve_side_table_storage(base::usize expr_count, base::usize pattern_count, base::usize type_count,
        base::usize stmt_count, base::usize item_count) const;

    [[nodiscard]] base::usize arena_bytes() const noexcept;
    [[nodiscard]] base::usize arena_blocks() const noexcept;

#if defined(AUREX_SEMA_WHITEBOX_TESTS)
public:
#else
private:
#endif
    void swap(CheckedModule& other) noexcept;
    void copy_from(const CheckedModule& other);
    void rebind_interned_texts(const IdentifierInterner* from, const IdentifierInterner& to) noexcept;
    void rebind_generic_instance_layouts() noexcept;
};

[[nodiscard]] std::string dump_checked_module(const CheckedModule& checked);
void populate_type_check_body_borrow_authority(
    query::TypeCheckBodyAuthority& authority, const CheckedModule& checked, FunctionLookupKey function);
[[nodiscard]] std::string struct_display_name(const TypeTable& types, const StructInfo& info);
[[nodiscard]] std::string enum_display_name(const TypeTable& types, const EnumCaseInfo& info);
[[nodiscard]] std::string enum_case_display_name(const TypeTable& types, const EnumCaseInfo& info);
[[nodiscard]] bool is_valid(const GenericFunctionInstanceBodyView& view) noexcept;
[[nodiscard]] bool is_valid(const TraitDefaultMethodInstanceBodyView& view) noexcept;

} // namespace aurex::sema
