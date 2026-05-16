#include <aurex/sema/symbol.hpp>

#include <aurex/sema/sema_messages.hpp>

#include <cassert>
#include <memory>
#include <utility>

namespace aurex::sema {

SymbolTable::SymbolTable()
    : arena_(std::make_unique<base::BumpAllocator>()),
      symbols_(make_sema_vector<Symbol>(*this->arena_)),
      scopes_(make_sema_vector<IdentSymbolMap>(*this->arena_)) {
    this->push_scope();
}

SymbolTable::SymbolTable(const SymbolTable& other)
    : arena_(std::make_unique<base::BumpAllocator>()),
      symbols_(make_sema_vector<Symbol>(*this->arena_)),
      scopes_(make_sema_vector<IdentSymbolMap>(*this->arena_)) {
    this->copy_from(other);
}

SymbolTable& SymbolTable::operator=(const SymbolTable& other) {
    if (this == &other) {
        return *this;
    }
    SymbolTable copy(other);
    *this = std::move(copy);
    return *this;
}

SymbolTable::SymbolTable(SymbolTable&& other) noexcept
    : arena_(std::move(other.arena_)),
      texts_(std::move(other.texts_)),
      symbols_(std::move(other.symbols_)),
      scopes_(std::move(other.scopes_)) {}

SymbolTable& SymbolTable::operator=(SymbolTable&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    this->swap(other);
    return *this;
}

void SymbolTable::push_scope(const base::usize expected_symbols) {
    this->scopes_.push_back(this->make_scope(expected_symbols));
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
            std::string(SEMA_DUPLICATE_DEFINITION_OR_SHADOWING) + std::string(symbol.name.view()),
        });
        return base::Result<SymbolId>::fail({base::ErrorCode::sema_error, std::string(SEMA_DUPLICATE_SYMBOL)});
    }

    const SymbolId id {static_cast<base::u32>(this->symbols_.size())};
    const IdentId name = symbol.name_id;
    this->symbols_.push_back(this->clone_symbol(symbol));
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

IdentSymbolMap SymbolTable::make_scope(const base::usize expected_symbols) const
{
    IdentSymbolMap scope = make_sema_map<IdentId, SymbolId, IdentIdHash>(*this->arena_, IdentIdHash {});
    scope.reserve(expected_symbols);
    return scope;
}

Symbol SymbolTable::clone_symbol(const Symbol& symbol) {
    Symbol copy = symbol;
    copy.name = sema::intern_text(this->texts_, symbol.name);
    copy.c_name = sema::intern_text(this->texts_, symbol.c_name);
    return copy;
}

void SymbolTable::copy_from(const SymbolTable& other) {
    this->symbols_.clear();
    this->symbols_.reserve(other.symbols_.size());
    for (const Symbol& symbol : other.symbols_) {
        this->symbols_.push_back(this->clone_symbol(symbol));
    }
    this->scopes_.clear();
    this->scopes_.reserve(other.scopes_.size());
    for (const IdentSymbolMap& source_scope : other.scopes_) {
        IdentSymbolMap scope = this->make_scope(source_scope.size());
        scope.insert(source_scope.begin(), source_scope.end());
        this->scopes_.push_back(std::move(scope));
    }
}

void SymbolTable::swap(SymbolTable& other) noexcept {
    using std::swap;
    this->symbols_.swap(other.symbols_);
    this->scopes_.swap(other.scopes_);
    swap(this->texts_, other.texts_);
    swap(this->arena_, other.arena_);
}

} // namespace aurex::sema
