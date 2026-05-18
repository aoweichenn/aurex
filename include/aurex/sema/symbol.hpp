#pragma once

#include <aurex/base/diagnostic.hpp>
#include <aurex/base/result.hpp>
#include <aurex/sema/identifier.hpp>
#include <aurex/sema/storage.hpp>
#include <aurex/sema/type.hpp>
#include <aurex/syntax/ast.hpp>
#include <aurex/syntax/ast_ids.hpp>

#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace aurex::sema {

enum class SymbolKind {
    type,
    function,
    const_,
    local,
    parameter,
    enum_case,
};

struct SymbolId {
    base::u32 value = INVALID_VALUE;
    static constexpr base::u32 INVALID_VALUE = std::numeric_limits<base::u32>::max();
};

inline constexpr SymbolId INVALID_SYMBOL_ID{SymbolId::INVALID_VALUE};

[[nodiscard]] inline constexpr bool is_valid(const SymbolId id) noexcept
{
    return id.value != SymbolId::INVALID_VALUE;
}

struct Symbol {
    SymbolKind kind = SymbolKind::local;
    InternedText name;
    IdentId name_id = INVALID_IDENT_ID;
    InternedText c_name;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    TypeHandle type = INVALID_TYPE_HANDLE;
    base::SourceRange range{};
    bool is_mutable = false;
    syntax::Visibility visibility = syntax::Visibility::public_;
    StableDefId stable_id;
};

using IdentSymbolMap = SemaMap<IdentId, SymbolId, IdentIdHash>;

class SymbolTable final {
public:
    SymbolTable();
    SymbolTable(const SymbolTable& other);
    SymbolTable& operator=(const SymbolTable& other);
    SymbolTable(SymbolTable&& other) noexcept;
    SymbolTable& operator=(SymbolTable&& other) noexcept;
    ~SymbolTable() = default;

    void push_scope(base::usize expected_symbols = 0);
    void pop_scope() noexcept;

    [[nodiscard]] base::Result<SymbolId> insert(Symbol symbol, base::DiagnosticSink& diagnostics);
    [[nodiscard]] const Symbol* find(IdentId name) const noexcept;
    [[nodiscard]] const Symbol* get(SymbolId id) const noexcept;
    void append_visible_names(std::vector<std::string_view>& names) const;

private:
    [[nodiscard]] IdentSymbolMap make_scope(base::usize expected_symbols) const;
    void copy_from(const SymbolTable& other);
    void swap(SymbolTable& other) noexcept;

    std::unique_ptr<base::BumpAllocator> arena_;
    SemaVector<Symbol> symbols_;
    SemaVector<IdentSymbolMap> scopes_;
};

} // namespace aurex::sema
