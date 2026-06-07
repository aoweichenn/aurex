#include <aurex/frontend/sema/sema_messages.hpp>
#include <aurex/infrastructure/base/integer.hpp>
#include <aurex/infrastructure/query/stable_hash.hpp>

#include <algorithm>
#include <sstream>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include <frontend/sema/internal/borrow/private/contract.hpp>
#include <frontend/sema/internal/diagnostics/private/sema_diagnostics.hpp>

namespace aurex::sema {
namespace {

constexpr std::string_view SEMA_BORROW_CONTRACT_FINGERPRINT_MARKER = "sema.borrow_contract.v1";
constexpr base::usize SEMA_BORROW_CONTRACT_TYPE_STACK_CAPACITY = 32;

void mix_function_key(query::StableHashBuilder& builder, const FunctionLookupKey key) noexcept
{
    builder.mix_u32(key.module);
    builder.mix_u32(key.owner_type);
    builder.mix_u32(key.name.value);
}

void append_optional_name_id(std::ostringstream& stream, const IdentId name)
{
    if (syntax::is_valid(name)) {
        stream << '#' << name.value;
        return;
    }
    stream << '-';
}

[[nodiscard]] bool same_selector_identity(const BorrowContractSelector& lhs, const BorrowContractSelector& rhs) noexcept
{
    return lhs.kind == rhs.kind && lhs.param_index == rhs.param_index && lhs.name_id == rhs.name_id;
}

} // namespace

std::string_view borrow_contract_selector_kind_name(const BorrowContractSelectorKind kind) noexcept
{
    switch (kind) {
        case BorrowContractSelectorKind::parameter:
            return "parameter";
        case BorrowContractSelectorKind::self:
            return "self";
        case BorrowContractSelectorKind::static_:
            return "static";
        case BorrowContractSelectorKind::unknown:
            return "unknown";
    }
    return "<invalid>";
}

std::string_view function_borrow_contract_source_name(const FunctionBorrowContractSource source) noexcept
{
    switch (source) {
        case FunctionBorrowContractSource::inferred:
            return "inferred";
        case FunctionBorrowContractSource::declared:
            return "declared";
        case FunctionBorrowContractSource::conservative_unknown:
            return "conservative_unknown";
    }
    return "<invalid>";
}

query::StableFingerprint128 function_borrow_contract_fingerprint(const FunctionBorrowContract& contract) noexcept
{
    query::StableHashBuilder builder;
    builder.mix_string(SEMA_BORROW_CONTRACT_FINGERPRINT_MARKER);
    mix_function_key(builder, contract.function);
    builder.mix_u32(contract.return_type.value);
    builder.mix_u8(static_cast<base::u8>(contract.source));
    builder.mix_bool(contract.return_type_can_contain_borrow);
    builder.mix_bool(contract.unknown_return_allowed);
    builder.mix_bool(contract.has_local_return_escape);
    builder.mix_bool(contract.has_contract_mismatch);
    builder.mix_u64(static_cast<base::u64>(contract.return_selectors.size()));
    for (const BorrowContractSelector& selector : contract.return_selectors) {
        builder.mix_u8(static_cast<base::u8>(selector.kind));
        builder.mix_u32(selector.param_index);
        builder.mix_u32(selector.name_id.value);
    }
    return builder.finish();
}

std::string dump_function_borrow_contract(const FunctionBorrowContract& contract)
{
    std::ostringstream stream;
    stream << "borrow_contract function=" << contract.function.module << ':' << contract.function.owner_type << ':';
    append_optional_name_id(stream, contract.function.name);
    stream << " source=" << function_borrow_contract_source_name(contract.source)
           << " return_type=" << contract.return_type.value
           << " can_borrow=" << (contract.return_type_can_contain_borrow ? "true" : "false")
           << " unknown=" << (contract.unknown_return_allowed ? "true" : "false")
           << " local_escape=" << (contract.has_local_return_escape ? "true" : "false")
           << " mismatch=" << (contract.has_contract_mismatch ? "true" : "false")
           << " fingerprint=" << query::debug_string(contract.fingerprint) << '\n';
    stream << "return_selectors:\n";
    for (base::usize index = 0; index < contract.return_selectors.size(); ++index) {
        const BorrowContractSelector& selector = contract.return_selectors[index];
        stream << "  s" << index << ' ' << borrow_contract_selector_kind_name(selector.kind)
               << " param=" << selector.param_index << " name=";
        append_optional_name_id(stream, selector.name_id);
        stream << '\n';
    }
    return stream.str();
}

SemanticAnalyzerCore::BorrowContractAnalyzer::BorrowContractAnalyzer(SemanticAnalyzerCore& core) noexcept : core_(core)
{
}

FunctionBorrowContract SemanticAnalyzerCore::BorrowContractAnalyzer::resolve_signature_contract(
    const syntax::ItemNode& function, const FunctionLookupKey key, const TypeHandle return_type,
    const std::span<const TypeHandle> param_types, const base::u32 part_index, const bool has_body)
{
    if (function.borrow_contract.present) {
        return this->declared_contract(function, key, return_type, param_types, part_index);
    }
    if (!has_body && this->type_can_contain_borrow(return_type)) {
        return this->conservative_unknown_contract(function, key, return_type, part_index);
    }

    FunctionBorrowContract contract;
    contract.function = key;
    contract.return_type = return_type;
    contract.source = FunctionBorrowContractSource::inferred;
    contract.return_type_can_contain_borrow = this->type_can_contain_borrow(return_type);
    contract.range = function.range;
    contract.part_index = part_index;
    contract.fingerprint = function_borrow_contract_fingerprint(contract);
    return contract;
}

void SemanticAnalyzerCore::BorrowContractAnalyzer::record_declared(
    const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature)
{
    const bool has_body = syntax::is_valid(function.body) && !function.is_extern_c && !function.is_prototype;
    FunctionBorrowContract contract = this->resolve_signature_contract(
        function, key, signature.return_type, signature.param_types, signature.part_index, has_body);
    if (function.borrow_contract.present || contract.source == FunctionBorrowContractSource::conservative_unknown) {
        if (function.borrow_contract.present) {
            const auto existing = this->core_.state_.checked.borrow_contracts.find(key);
            if (existing != this->core_.state_.checked.borrow_contracts.end()
                && existing->second.source == FunctionBorrowContractSource::declared
                && existing->second.fingerprint != contract.fingerprint) {
                this->core_.report_general(contract.range, std::string(SEMA_BORROW_CONTRACT_MISMATCH));
                this->core_.report_note(existing->second.range, SemanticDiagnosticKind::general,
                    std::string(SEMA_BORROW_CONTRACT_DECLARED_HERE));
            }
        }
        this->core_.state_.checked.borrow_contracts[key] = std::move(contract);
    }
}

void SemanticAnalyzerCore::BorrowContractAnalyzer::check(
    const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature)
{
    const auto summary = this->core_.state_.checked.borrow_summaries.find(key);
    if (summary == this->core_.state_.checked.borrow_summaries.end()) {
        return;
    }

    FunctionBorrowContract inferred = this->inferred_contract(function, summary->second, signature);
    const auto declared = this->core_.state_.checked.borrow_contracts.find(key);
    if (declared == this->core_.state_.checked.borrow_contracts.end()
        || declared->second.source != FunctionBorrowContractSource::declared) {
        this->core_.state_.checked.borrow_contracts[key] = std::move(inferred);
        return;
    }

    FunctionBorrowContract checked = declared->second;
    this->enforce_declared_subset(checked, inferred);
    checked.has_local_return_escape = inferred.has_local_return_escape;
    checked.has_contract_mismatch = inferred.has_contract_mismatch;
    checked.fingerprint = function_borrow_contract_fingerprint(checked);
    this->core_.state_.checked.borrow_contracts[key] = std::move(checked);
}

bool SemanticAnalyzerCore::BorrowContractAnalyzer::contract_is_subset(
    const FunctionBorrowContract& narrowed, const FunctionBorrowContract& boundary) const noexcept
{
    if (narrowed.has_local_return_escape || narrowed.has_contract_mismatch) {
        return false;
    }
    if (boundary.unknown_return_allowed) {
        return true;
    }
    if (narrowed.unknown_return_allowed) {
        return false;
    }
    for (const BorrowContractSelector& selector : narrowed.return_selectors) {
        if (!this->contract_contains_selector(boundary, selector)) {
            return false;
        }
    }
    return true;
}

bool SemanticAnalyzerCore::BorrowContractAnalyzer::type_can_contain_borrow(const TypeHandle type) const
{
    if (!is_valid(type)) {
        return false;
    }

    std::vector<TypeHandle> pending;
    pending.reserve(SEMA_BORROW_CONTRACT_TYPE_STACK_CAPACITY);
    pending.push_back(type);
    std::unordered_set<base::u32> visited;
    while (!pending.empty()) {
        const TypeHandle current = pending.back();
        pending.pop_back();
        if (!is_valid(current) || current.value >= this->core_.state_.checked.types.size()
            || !visited.insert(current.value).second) {
            continue;
        }
        const TypeInfo& info = this->core_.state_.checked.types.get(current);
        switch (info.kind) {
            case TypeKind::builtin:
                if (info.builtin == BuiltinType::str) {
                    return true;
                }
                break;
            case TypeKind::reference:
            case TypeKind::slice:
            case TypeKind::generic_param:
            case TypeKind::associated_projection:
                return true;
            case TypeKind::array:
                pending.push_back(info.array_element);
                break;
            case TypeKind::tuple:
                pending.insert(pending.end(), info.tuple_elements.begin(), info.tuple_elements.end());
                break;
            case TypeKind::struct_: {
                const StructInfo* const structure = this->core_.find_struct(current);
                if (structure != nullptr) {
                    for (const StructFieldInfo& field : structure->fields) {
                        pending.push_back(field.type);
                    }
                }
                break;
            }
            case TypeKind::enum_: {
                const EnumCaseList* const cases = this->core_.find_enum_cases_by_type(current);
                if (cases != nullptr) {
                    for (const EnumCaseInfo* const enum_case : *cases) {
                        if (enum_case != nullptr) {
                            pending.insert(
                                pending.end(), enum_case->payload_types.begin(), enum_case->payload_types.end());
                        }
                    }
                }
                break;
            }
            case TypeKind::pointer:
            case TypeKind::function:
            case TypeKind::opaque_struct:
            case TypeKind::trait_object:
                break;
        }
    }
    return false;
}

FunctionBorrowContract SemanticAnalyzerCore::BorrowContractAnalyzer::declared_contract(const syntax::ItemNode& function,
    const FunctionLookupKey key, const TypeHandle return_type, const std::span<const TypeHandle> param_types,
    const base::u32 part_index)
{
    FunctionBorrowContract contract;
    contract.function = key;
    contract.return_type = return_type;
    contract.source = FunctionBorrowContractSource::declared;
    contract.return_type_can_contain_borrow = this->type_can_contain_borrow(return_type);
    contract.range = function.borrow_contract.range;
    contract.part_index = part_index;
    contract.return_selectors.reserve(function.borrow_contract.return_selectors.size());
    if (!contract.return_type_can_contain_borrow) {
        this->core_.report_general(function.borrow_contract.range, std::string(SEMA_BORROW_CONTRACT_REDUNDANT));
    }
    for (const syntax::BorrowContractSelectorDecl& selector : function.borrow_contract.return_selectors) {
        contract.return_selectors.push_back(this->selector_from_decl(function, selector, param_types));
    }
    this->validate_declared_selector_set(contract);
    this->sort_unique_selectors(contract);
    contract.fingerprint = function_borrow_contract_fingerprint(contract);
    return contract;
}

FunctionBorrowContract SemanticAnalyzerCore::BorrowContractAnalyzer::conservative_unknown_contract(
    const syntax::ItemNode& function, const FunctionLookupKey key, const TypeHandle return_type,
    const base::u32 part_index)
{
    FunctionBorrowContract contract;
    contract.function = key;
    contract.return_type = return_type;
    contract.source = FunctionBorrowContractSource::conservative_unknown;
    contract.return_type_can_contain_borrow = true;
    contract.unknown_return_allowed = true;
    contract.range = function.range;
    contract.part_index = part_index;
    contract.return_selectors.push_back(BorrowContractSelector{
        .kind = BorrowContractSelectorKind::unknown,
        .param_index = SEMA_BORROW_SUMMARY_INVALID_INDEX,
        .name_id = INVALID_IDENT_ID,
        .range = function.range,
    });
    contract.fingerprint = function_borrow_contract_fingerprint(contract);
    return contract;
}

FunctionBorrowContract SemanticAnalyzerCore::BorrowContractAnalyzer::inferred_contract(
    const syntax::ItemNode& function, const FunctionBorrowSummary& summary, const FunctionSignature& signature)
{
    FunctionBorrowContract contract;
    contract.function = summary.function;
    contract.return_type = summary.return_type;
    contract.source = FunctionBorrowContractSource::inferred;
    contract.return_type_can_contain_borrow = summary.return_type_can_contain_borrow;
    contract.unknown_return_allowed = summary.has_unknown_return_origin;
    contract.has_local_return_escape = summary.has_local_return_escape;
    contract.range = function.range;
    contract.part_index = signature.part_index;
    contract.return_selectors.reserve(summary.return_origins.size() + (summary.has_unknown_return_origin ? 1U : 0U));
    if (summary.has_unknown_return_origin) {
        contract.return_selectors.push_back(BorrowContractSelector{
            .kind = BorrowContractSelectorKind::unknown,
            .param_index = SEMA_BORROW_SUMMARY_INVALID_INDEX,
            .name_id = INVALID_IDENT_ID,
            .range = function.range,
        });
    }
    for (const FunctionBorrowReturnOrigin& return_origin : summary.return_origins) {
        if (return_origin.origin_index >= summary.origins.size()) {
            contract.unknown_return_allowed = true;
            continue;
        }
        const BorrowSummaryOrigin& origin = summary.origins[return_origin.origin_index];
        if (origin.kind == BorrowSummaryOriginKind::parameter && !origin.storage_slot) {
            contract.return_selectors.push_back(this->selector_from_origin(function, origin));
        } else if (origin.kind == BorrowSummaryOriginKind::static_) {
            contract.return_selectors.push_back(BorrowContractSelector{
                .kind = BorrowContractSelectorKind::static_,
                .param_index = SEMA_BORROW_SUMMARY_INVALID_INDEX,
                .name_id = INVALID_IDENT_ID,
                .range = origin.range,
            });
        } else if (origin.kind == BorrowSummaryOriginKind::unknown) {
            contract.unknown_return_allowed = true;
            contract.return_selectors.push_back(BorrowContractSelector{
                .kind = BorrowContractSelectorKind::unknown,
                .param_index = SEMA_BORROW_SUMMARY_INVALID_INDEX,
                .name_id = INVALID_IDENT_ID,
                .range = origin.range,
            });
        } else if (origin.kind == BorrowSummaryOriginKind::local || origin.kind == BorrowSummaryOriginKind::temporary
            || (origin.kind == BorrowSummaryOriginKind::parameter && origin.storage_slot)) {
            contract.has_local_return_escape = true;
        }
    }
    this->sort_unique_selectors(contract);
    contract.fingerprint = function_borrow_contract_fingerprint(contract);
    return contract;
}

BorrowContractSelector SemanticAnalyzerCore::BorrowContractAnalyzer::selector_from_decl(
    const syntax::ItemNode& function, const syntax::BorrowContractSelectorDecl& decl,
    const std::span<const TypeHandle> param_types)
{
    switch (decl.kind) {
        case syntax::BorrowContractSelectorKind::self:
            if (function.params.empty() || function.params.front().name != "self") {
                this->core_.report_general(decl.range, std::string(SEMA_BORROW_CONTRACT_SELF_SELECTOR));
                return BorrowContractSelector{
                    .kind = BorrowContractSelectorKind::self,
                    .param_index = SEMA_BORROW_SUMMARY_INVALID_INDEX,
                    .name_id = decl.name_id,
                    .range = decl.range,
                };
            }
            if (param_types.empty() || !this->type_can_contain_borrow(param_types.front())) {
                this->core_.report_general(decl.range, std::string(SEMA_BORROW_CONTRACT_NON_BORROWING_SELECTOR));
            }
            return BorrowContractSelector{
                .kind = BorrowContractSelectorKind::self,
                .param_index = 0,
                .name_id = function.params.front().name_id,
                .range = decl.range,
            };
        case syntax::BorrowContractSelectorKind::static_:
            return BorrowContractSelector{
                .kind = BorrowContractSelectorKind::static_,
                .param_index = SEMA_BORROW_SUMMARY_INVALID_INDEX,
                .name_id = decl.name_id,
                .range = decl.range,
            };
        case syntax::BorrowContractSelectorKind::unknown:
            return BorrowContractSelector{
                .kind = BorrowContractSelectorKind::unknown,
                .param_index = SEMA_BORROW_SUMMARY_INVALID_INDEX,
                .name_id = decl.name_id,
                .range = decl.range,
            };
        case syntax::BorrowContractSelectorKind::parameter:
            break;
    }

    for (base::usize index = 0; index < function.params.size() && index < param_types.size(); ++index) {
        if (function.params[index].name_id == decl.name_id) {
            if (!this->type_can_contain_borrow(param_types[index])) {
                this->core_.report_general(decl.range, std::string(SEMA_BORROW_CONTRACT_NON_BORROWING_SELECTOR));
            }
            return BorrowContractSelector{
                .kind = BorrowContractSelectorKind::parameter,
                .param_index = base::checked_u32(index, "sema borrow contract parameter index"),
                .name_id = decl.name_id,
                .range = decl.range,
            };
        }
    }
    this->core_.report_general(decl.range, std::string(SEMA_BORROW_CONTRACT_UNKNOWN_SELECTOR));
    return BorrowContractSelector{
        .kind = BorrowContractSelectorKind::parameter,
        .param_index = SEMA_BORROW_SUMMARY_INVALID_INDEX,
        .name_id = decl.name_id,
        .range = decl.range,
    };
}

BorrowContractSelector SemanticAnalyzerCore::BorrowContractAnalyzer::selector_from_origin(
    const syntax::ItemNode& function, const BorrowSummaryOrigin& origin) const noexcept
{
    BorrowContractSelector selector;
    selector.kind = BorrowContractSelectorKind::parameter;
    selector.param_index = origin.param_index;
    selector.name_id = origin.name_id;
    selector.range = origin.range;
    if (origin.param_index == 0 && !function.params.empty() && function.params.front().name == "self") {
        selector.kind = BorrowContractSelectorKind::self;
        selector.name_id = function.params.front().name_id;
    }
    return selector;
}

bool SemanticAnalyzerCore::BorrowContractAnalyzer::selector_matches(
    const BorrowContractSelector& declared, const BorrowContractSelector& inferred) const noexcept
{
    if (declared.kind == BorrowContractSelectorKind::unknown || inferred.kind == BorrowContractSelectorKind::unknown) {
        return declared.kind == inferred.kind;
    }
    if (declared.kind == BorrowContractSelectorKind::static_ || inferred.kind == BorrowContractSelectorKind::static_) {
        return declared.kind == inferred.kind;
    }
    if (declared.kind == BorrowContractSelectorKind::self || inferred.kind == BorrowContractSelectorKind::self) {
        return declared.param_index == inferred.param_index && declared.param_index == 0;
    }
    return declared.kind == inferred.kind && declared.param_index == inferred.param_index;
}

bool SemanticAnalyzerCore::BorrowContractAnalyzer::contract_contains_selector(
    const FunctionBorrowContract& declared, const BorrowContractSelector& inferred) const noexcept
{
    return std::ranges::any_of(declared.return_selectors, [this, &inferred](const BorrowContractSelector& selector) {
        return this->selector_matches(selector, inferred);
    });
}

void SemanticAnalyzerCore::BorrowContractAnalyzer::sort_unique_selectors(FunctionBorrowContract& contract) const
{
    std::ranges::sort(
        contract.return_selectors, [](const BorrowContractSelector& lhs, const BorrowContractSelector& rhs) {
            return std::tie(lhs.kind, lhs.param_index, lhs.name_id.value)
                < std::tie(rhs.kind, rhs.param_index, rhs.name_id.value);
        });
    contract.return_selectors.erase(std::ranges::unique(contract.return_selectors, same_selector_identity).begin(),
        contract.return_selectors.end());
}

void SemanticAnalyzerCore::BorrowContractAnalyzer::validate_declared_selector_set(FunctionBorrowContract& contract)
{
    for (base::usize index = 0; index < contract.return_selectors.size(); ++index) {
        BorrowContractSelector& selector = contract.return_selectors[index];
        if (selector.kind == BorrowContractSelectorKind::unknown) {
            contract.unknown_return_allowed = true;
        }
        for (base::usize prior = 0; prior < index; ++prior) {
            if (same_selector_identity(contract.return_selectors[prior], selector)) {
                this->core_.report_general(selector.range, std::string(SEMA_BORROW_CONTRACT_DUPLICATE_SELECTOR));
                break;
            }
        }
    }
}

void SemanticAnalyzerCore::BorrowContractAnalyzer::enforce_declared_subset(
    const FunctionBorrowContract& declared, FunctionBorrowContract& inferred)
{
    bool reported = false;
    const auto report_mismatch = [&](const base::SourceRange& range) {
        inferred.has_contract_mismatch = true;
        if (reported) {
            return;
        }
        this->core_.report_general(range, std::string(SEMA_BORROW_CONTRACT_MISMATCH));
        this->core_.report_note(
            declared.range, SemanticDiagnosticKind::general, std::string(SEMA_BORROW_CONTRACT_DECLARED_HERE));
        reported = true;
    };

    if (inferred.has_local_return_escape) {
        report_mismatch(inferred.range);
    }
    if (inferred.unknown_return_allowed && !declared.unknown_return_allowed) {
        report_mismatch(inferred.range);
    }
    for (const BorrowContractSelector& selector : inferred.return_selectors) {
        if (selector.kind == BorrowContractSelectorKind::unknown) {
            continue;
        }
        if (!this->contract_contains_selector(declared, selector)) {
            report_mismatch(selector.range);
        }
    }
}

void SemanticAnalyzerCore::record_declared_borrow_contract(
    const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature)
{
    BorrowContractAnalyzer(*this).record_declared(function, key, signature);
}

void SemanticAnalyzerCore::check_borrow_contract(
    const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature)
{
    BorrowContractAnalyzer(*this).check(function, key, signature);
}

} // namespace aurex::sema
