#include <aurex/sema/sema_messages.hpp>
#include <aurex/sema/symbol.hpp>

#include <cassert>
#include <memory>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace aurex::sema {
namespace {

constexpr std::string_view SEMA_SYMBOL_TABLE_ID_CONTEXT = "semantic symbol table id";

} // namespace

SymbolTable::SymbolTable()
    : arena_(std::make_unique<base::BumpAllocator>()), symbols_(make_sema_vector<Symbol>(*this->arena_)),
      scopes_(make_sema_vector<IdentSymbolMap>(*this->arena_))
{
    this->push_scope();
}

SymbolTable::SymbolTable(const SymbolTable& other)
    : arena_(std::make_unique<base::BumpAllocator>()), symbols_(make_sema_vector<Symbol>(*this->arena_)),
      scopes_(make_sema_vector<IdentSymbolMap>(*this->arena_))
{
    this->copy_from(other);
}

SymbolTable& SymbolTable::operator=(const SymbolTable& other)
{
    if (this == &other) {
        return *this;
    }
    SymbolTable copy(other);
    *this = std::move(copy);
    return *this;
}

SymbolTable::SymbolTable(SymbolTable&& other) noexcept
    : arena_(std::move(other.arena_)), symbols_(std::move(other.symbols_)), scopes_(std::move(other.scopes_))
{
}

SymbolTable& SymbolTable::operator=(SymbolTable&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    this->swap(other);
    return *this;
}

void SymbolTable::push_scope(const base::usize expected_symbols)
{
    this->scopes_.push_back(this->make_scope(expected_symbols));
}

void SymbolTable::pop_scope() noexcept
{
    assert(!this->scopes_.empty());
    this->scopes_.pop_back();
}

base::Result<SymbolId> SymbolTable::insert(Symbol symbol, base::DiagnosticSink& diagnostics)
{
    assert(!this->scopes_.empty());
    const auto existing = this->scopes_.back().find(symbol.name_id);
    if (existing != this->scopes_.back().end()) {
        diagnostics.push(base::Diagnostic{
            base::Severity::error,
            symbol.range,
            std::string(SEMA_DUPLICATE_DEFINITION_OR_SHADOWING) + std::string(symbol.name.view()),
            base::DiagnosticCategory::name_resolution,
            base::DiagnosticCode::semantic_duplicate,
        });
        if (const Symbol* previous = this->get(existing->second); previous != nullptr) {
            diagnostics.push(base::Diagnostic{
                base::Severity::note,
                previous->range,
                sema_previous_declaration_note_message(symbol.name.view()),
                base::DiagnosticCategory::name_resolution,
                base::DiagnosticCode::semantic_duplicate,
            });
        }
        return base::Result<SymbolId>::fail({base::ErrorCode::sema_error, std::string(SEMA_DUPLICATE_SYMBOL)});
    }

    const SymbolId id{base::checked_u32(this->symbols_.size(), SEMA_SYMBOL_TABLE_ID_CONTEXT)};
    const IdentId name = symbol.name_id;
    this->symbols_.push_back(symbol);
    this->scopes_.back().emplace(name, id);
    return base::Result<SymbolId>::ok(id);
}

const Symbol* SymbolTable::find(const IdentId name) const noexcept
{
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

const Symbol* SymbolTable::get(const SymbolId id) const noexcept
{
    if (!is_valid(id) || id.value >= this->symbols_.size()) {
        return nullptr;
    }
    return &this->symbols_[id.value];
}

void SymbolTable::append_visible_names(std::vector<std::string_view>& names) const
{
    std::unordered_set<IdentId, IdentIdHash> seen;
    for (auto scope = this->scopes_.rbegin(); scope != this->scopes_.rend(); ++scope) {
        seen.reserve(seen.size() + scope->size());
        for (const auto& entry : *scope) {
            if (!seen.insert(entry.first).second) {
                continue;
            }
            const Symbol* symbol = this->get(entry.second);
            if (symbol != nullptr && !symbol->name.empty()) {
                names.push_back(symbol->name.view());
            }
        }
    }
}

IdentSymbolMap SymbolTable::make_scope(const base::usize expected_symbols) const
{
    IdentSymbolMap scope = make_sema_map<IdentId, SymbolId, IdentIdHash>(*this->arena_, IdentIdHash{});
    scope.reserve(expected_symbols);
    return scope;
}

void SymbolTable::copy_from(const SymbolTable& other)
{
    this->symbols_.clear();
    this->symbols_.reserve(other.symbols_.size());
    this->symbols_.insert(this->symbols_.end(), other.symbols_.begin(), other.symbols_.end());
    this->scopes_.clear();
    this->scopes_.reserve(other.scopes_.size());
    for (const IdentSymbolMap& source_scope : other.scopes_) {
        IdentSymbolMap scope = this->make_scope(source_scope.size());
        scope.insert(source_scope.begin(), source_scope.end());
        this->scopes_.push_back(std::move(scope));
    }
}

void SymbolTable::swap(SymbolTable& other) noexcept
{
    using std::swap;
    this->symbols_.swap(other.symbols_);
    this->scopes_.swap(other.scopes_);
    swap(this->arena_, other.arena_);
}

} // namespace aurex::sema
