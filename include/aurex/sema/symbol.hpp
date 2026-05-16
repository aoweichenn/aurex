#pragma once

#include <aurex/base/diagnostic.hpp>
#include <aurex/base/result.hpp>
#include <aurex/sema/identifier.hpp>
#include <aurex/sema/type.hpp>
#include <aurex/syntax/ast.hpp>
#include <aurex/syntax/ast_ids.hpp>

#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
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

inline constexpr SymbolId INVALID_SYMBOL_ID {SymbolId::INVALID_VALUE};

[[nodiscard]] inline constexpr bool is_valid(const SymbolId id) noexcept {
    return id.value != SymbolId::INVALID_VALUE;
}

struct Symbol {
    SymbolKind kind = SymbolKind::local;
    std::string name;
    IdentId name_id = INVALID_IDENT_ID;
    std::string c_name;
    syntax::ModuleId module = syntax::INVALID_MODULE_ID;
    TypeHandle type = INVALID_TYPE_HANDLE;
    base::SourceRange range {};
    bool is_mutable = false;
    syntax::Visibility visibility = syntax::Visibility::public_;
};

using IdentSymbolMap = std::unordered_map<IdentId, SymbolId, IdentIdHash>;

class SymbolTable final {
public:
    SymbolTable();

    void push_scope(base::usize expected_symbols = 0);
    void pop_scope() noexcept;

    [[nodiscard]] base::Result<SymbolId> insert(Symbol symbol, base::DiagnosticSink& diagnostics);
    [[nodiscard]] const Symbol* find(IdentId name) const noexcept;
    [[nodiscard]] const Symbol* get(SymbolId id) const noexcept;

private:
    std::vector<Symbol> symbols_;
    std::vector<IdentSymbolMap> scopes_;
};

} // namespace aurex::sema
