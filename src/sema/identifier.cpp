#include <aurex/sema/identifier.hpp>

namespace aurex::sema {
namespace {

constexpr base::u64 SEMA_LOOKUP_KEY_U32_SHIFT = 32;

} // namespace

std::size_t IdentifierTextHash::operator()(const std::string_view value) const noexcept {
    return std::hash<std::string_view> {}(value);
}

void IdentifierInterner::reserve(const base::usize expected_identifiers) {
    this->texts_.reserve(expected_identifiers);
    this->ids_.reserve(expected_identifiers);
}

IdentId IdentifierInterner::intern(const std::string_view text) {
    if (const IdentId existing = this->find(text); is_valid(existing)) {
        return existing;
    }

    const IdentId id {static_cast<base::u32>(this->texts_.size())};
    this->storage_.emplace_back(text);
    const std::string& stored_text = this->storage_.back();
    const std::string_view stable_text {stored_text.data(), stored_text.size()};
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

std::size_t EnumCaseLookupKeyHash::operator()(const EnumCaseLookupKey key) const noexcept {
    const base::u64 packed = (static_cast<base::u64>(key.enum_type) << SEMA_LOOKUP_KEY_U32_SHIFT) |
        static_cast<base::u64>(key.case_name.value);
    return std::hash<base::u64> {}(packed);
}

} // namespace aurex::sema
