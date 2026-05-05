#pragma once

#include "aurex/base/diagnostic.hpp"
#include "aurex/base/result.hpp"
#include "aurex/sema/type.hpp"
#include "aurex/syntax/ast.hpp"
#include "aurex/syntax/ast_ids.hpp"

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
    base::u32 value = invalid_value;
    static constexpr base::u32 invalid_value = std::numeric_limits<base::u32>::max();
};

inline constexpr SymbolId invalid_symbol_id {SymbolId::invalid_value};

[[nodiscard]] inline constexpr bool is_valid(const SymbolId id) noexcept {
    return id.value != SymbolId::invalid_value;
}

struct Symbol {
    SymbolKind kind = SymbolKind::local;
    std::string name;
    std::string c_name;
    syntax::ModuleId module = syntax::invalid_module_id;
    TypeHandle type = invalid_type_handle;
    base::SourceRange range {};
    bool is_mutable = false;
    syntax::Visibility visibility = syntax::Visibility::public_;
};

class SymbolTable final {
public:
    SymbolTable();

    void push_scope();
    void pop_scope() noexcept;

    [[nodiscard]] base::Result<SymbolId> insert(Symbol symbol, base::DiagnosticSink& diagnostics);
    [[nodiscard]] const Symbol* find(std::string_view name) const noexcept;
    [[nodiscard]] const Symbol* get(SymbolId id) const noexcept;

private:
    std::vector<Symbol> symbols_;
    std::vector<std::unordered_map<std::string, SymbolId>> scopes_;
};

} // namespace aurex::sema
