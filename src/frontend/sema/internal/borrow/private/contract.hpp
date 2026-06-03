#pragma once

#include <aurex/frontend/sema/checked_module.hpp>

#include <span>
#include <string>
#include <string_view>

#include <frontend/sema/internal/core/private/sema_core.hpp>

namespace aurex::sema {

class SemanticAnalyzerCore::BorrowContractAnalyzer final {
public:
    explicit BorrowContractAnalyzer(SemanticAnalyzerCore& core) noexcept;

    [[nodiscard]] FunctionBorrowContract resolve_signature_contract(const syntax::ItemNode& function,
        FunctionLookupKey key, TypeHandle return_type, std::span<const TypeHandle> param_types, base::u32 part_index,
        bool has_body);

    void record_declared(
        const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature);
    void check(const syntax::ItemNode& function, const FunctionLookupKey& key, const FunctionSignature& signature);
    [[nodiscard]] bool contract_is_subset(
        const FunctionBorrowContract& narrowed, const FunctionBorrowContract& boundary) const noexcept;

private:
    [[nodiscard]] bool type_can_contain_borrow(TypeHandle type) const;
    [[nodiscard]] FunctionBorrowContract declared_contract(const syntax::ItemNode& function, FunctionLookupKey key,
        TypeHandle return_type, std::span<const TypeHandle> param_types, base::u32 part_index);
    [[nodiscard]] FunctionBorrowContract conservative_unknown_contract(
        const syntax::ItemNode& function, FunctionLookupKey key, TypeHandle return_type, base::u32 part_index);
    [[nodiscard]] FunctionBorrowContract inferred_contract(
        const syntax::ItemNode& function, const FunctionBorrowSummary& summary, const FunctionSignature& signature);
    [[nodiscard]] BorrowContractSelector selector_from_decl(const syntax::ItemNode& function,
        const syntax::BorrowContractSelectorDecl& decl, std::span<const TypeHandle> param_types);
    [[nodiscard]] BorrowContractSelector selector_from_origin(
        const syntax::ItemNode& function, const BorrowSummaryOrigin& origin) const noexcept;
    [[nodiscard]] bool selector_matches(
        const BorrowContractSelector& declared, const BorrowContractSelector& inferred) const noexcept;
    [[nodiscard]] bool contract_contains_selector(
        const FunctionBorrowContract& declared, const BorrowContractSelector& inferred) const noexcept;
    void sort_unique_selectors(FunctionBorrowContract& contract) const;
    void validate_declared_selector_set(FunctionBorrowContract& contract);
    void enforce_declared_subset(const FunctionBorrowContract& declared, FunctionBorrowContract& inferred);

    SemanticAnalyzerCore& core_;
};

[[nodiscard]] std::string_view borrow_contract_selector_kind_name(BorrowContractSelectorKind kind) noexcept;
[[nodiscard]] std::string_view function_borrow_contract_source_name(FunctionBorrowContractSource source) noexcept;
[[nodiscard]] std::string dump_function_borrow_contract(const FunctionBorrowContract& contract);

} // namespace aurex::sema
