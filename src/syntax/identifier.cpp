#include <aurex/syntax/identifier.hpp>

#include <utility>

namespace aurex::syntax {

IdentifierInterner::IdentifierInterner(const IdentifierInterner& other) {
    this->reserve(other.size());
    for (const std::string_view text : other.texts_) {
        static_cast<void>(this->intern(text));
    }
}

IdentifierInterner& IdentifierInterner::operator=(const IdentifierInterner& other) {
    if (this == &other) {
        return *this;
    }
    this->texts_.clear();
    this->ids_.clear();
    this->arena_.reset();
    this->reserve(other.size());
    for (const std::string_view text : other.texts_) {
        static_cast<void>(this->intern(text));
    }
    return *this;
}

IdentifierInterner::IdentifierInterner(IdentifierInterner&& other) noexcept
    : arena_(std::move(other.arena_)),
      texts_(std::move(other.texts_)),
      ids_(std::move(other.ids_)) {
    other.texts_.clear();
    other.ids_.clear();
}

IdentifierInterner& IdentifierInterner::operator=(IdentifierInterner&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    this->texts_.clear();
    this->ids_.clear();
    this->arena_ = std::move(other.arena_);
    this->texts_ = std::move(other.texts_);
    this->ids_ = std::move(other.ids_);
    other.texts_.clear();
    other.ids_.clear();
    return *this;
}

std::size_t IdentifierTextHash::operator()(const std::string_view value) const noexcept {
    return std::hash<std::string_view> {}(value);
}

void IdentifierInterner::reserve(const base::usize expected_identifiers) {
    this->texts_.reserve(expected_identifiers);
    this->ids_.reserve(expected_identifiers);
}

IdentId IdentifierInterner::intern(const std::string_view text) {
    if (text.empty()) {
        return INVALID_IDENT_ID;
    }
    if (const IdentId existing = this->find(text); is_valid(existing)) {
        return existing;
    }

    const IdentId id {static_cast<base::u32>(this->texts_.size())};
    const std::string_view stable_text = this->arena_.copy_string(text);
    this->texts_.push_back(stable_text);
    this->ids_.emplace(stable_text, id);
    return id;
}

IdentId IdentifierInterner::find(const std::string_view text) const noexcept {
    const auto found = this->ids_.find(text);
    return found == this->ids_.end() ? INVALID_IDENT_ID : found->second;
}

std::string_view IdentifierInterner::text(const IdentId id) const noexcept {
    if (!is_valid(id) || id.value >= this->texts_.size()) {
        return {};
    }
    return this->texts_[id.value];
}

base::usize IdentifierInterner::size() const noexcept {
    return this->texts_.size();
}

base::usize IdentifierInterner::arena_bytes() const noexcept {
    return this->arena_.allocated_bytes();
}

base::usize IdentifierInterner::arena_blocks() const noexcept {
    return this->arena_.block_count();
}

} // namespace aurex::syntax
