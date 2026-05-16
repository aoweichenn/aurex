#include <aurex/sema/symbol.hpp>

#include <aurex/sema/sema_messages.hpp>

#include <cassert>
#include <utility>

namespace aurex::sema {

SymbolTable::SymbolTable() {
    this->push_scope();
}

void SymbolTable::push_scope(const base::usize expected_symbols) {
    IdentSymbolMap& scope = this->scopes_.emplace_back();
    scope.reserve(expected_symbols);
}

void SymbolTable::pop_scope() noexcept {
    assert(!this->scopes_.empty());
    this->scopes_.pop_back();
}

base::Result<SymbolId> SymbolTable::insert(Symbol symbol, base::DiagnosticSink& diagnostics) {
    assert(!this->scopes_.empty());
    if (this->scopes_.back().contains(symbol.name_id)) {
        diagnostics.push(base::Diagnostic {
            base::Severity::error,
            symbol.range,
            std::string(SEMA_DUPLICATE_DEFINITION_OR_SHADOWING) + symbol.name,
        });
        return base::Result<SymbolId>::fail({base::ErrorCode::sema_error, std::string(SEMA_DUPLICATE_SYMBOL)});
    }

    const SymbolId id {static_cast<base::u32>(this->symbols_.size())};
    const IdentId name = symbol.name_id;
    this->symbols_.push_back(std::move(symbol));
    this->scopes_.back().emplace(name, id);
    return base::Result<SymbolId>::ok(id);
}

const Symbol* SymbolTable::find(const IdentId name) const noexcept {
    if (!is_valid(name)) {
        return nullptr;
    }
    for (auto scope = this->scopes_.rbegin(); scope != this->scopes_.rend(); ++scope) {
        const auto found = scope->find(name);
        if (found != scope->end()) {
            return this->get(found->second);
        }
    }
    return nullptr;
}

const Symbol* SymbolTable::get(const SymbolId id) const noexcept {
    if (!is_valid(id) || id.value >= this->symbols_.size()) {
        return nullptr;
    }
    return &this->symbols_[id.value];
}

} // namespace aurex::sema
