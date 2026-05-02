#include "aurex/sema/symbol.hpp"

#include <cassert>
#include <utility>

namespace aurex::sema {

SymbolTable::SymbolTable() {
    push_scope();
}

void SymbolTable::push_scope() {
    scopes_.emplace_back();
}

void SymbolTable::pop_scope() noexcept {
    assert(!scopes_.empty());
    scopes_.pop_back();
}

base::Result<SymbolId> SymbolTable::insert(Symbol symbol, base::DiagnosticSink& diagnostics) {
    assert(!scopes_.empty());
    if (find(symbol.name) != nullptr) {
        diagnostics.push(base::Diagnostic {
            base::Severity::error,
            symbol.range,
            "duplicate definition or shadowing is not allowed: " + symbol.name,
        });
        return base::Result<SymbolId>::fail({base::ErrorCode::sema_error, "duplicate symbol"});
    }

    const SymbolId id {static_cast<base::u32>(symbols_.size())};
    const std::string name = symbol.name;
    symbols_.push_back(std::move(symbol));
    scopes_.back().emplace(name, id);
    return base::Result<SymbolId>::ok(id);
}

const Symbol* SymbolTable::find(const std::string_view name) const noexcept {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        const auto found = scope->find(std::string(name));
        if (found != scope->end()) {
            return get(found->second);
        }
    }
    return nullptr;
}

const Symbol* SymbolTable::get(const SymbolId id) const noexcept {
    if (!is_valid(id) || id.value >= symbols_.size()) {
        return nullptr;
    }
    return &symbols_[id.value];
}

} // namespace aurex::sema
