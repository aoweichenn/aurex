#include <aurex/syntax/identifier.hpp>

#include <memory>
#include <utility>

namespace aurex::syntax {
namespace {

constexpr base::u64 SYNTAX_STABLE_HASH_OFFSET = 14695981039346656037ULL;
constexpr base::u64 SYNTAX_STABLE_HASH_PRIME = 1099511628211ULL;

} // namespace

IdentifierInterner::IdentifierInterner()
    : arena_(std::make_unique<base::BumpAllocator>()),
      texts_(base::BumpAllocatorAdapter<std::string_view>{*this->arena_}),
      ids_(0, IdentifierTextHash{}, std::equal_to<>{}, base::BumpAllocatorAdapter<IdMapEntry>{*this->arena_})
{
}

IdentifierInterner::IdentifierInterner(const IdentifierInterner& other) : IdentifierInterner()
{
    this->reserve(other.size());
    for (const std::string_view text : other.texts_) {
        static_cast<void>(this->intern(text));
    }
}

IdentifierInterner& IdentifierInterner::operator=(const IdentifierInterner& other)
{
    if (this == &other) {
        return *this;
    }
    IdentifierInterner copy(other);
    *this = std::move(copy);
    return *this;
}

IdentifierInterner::IdentifierInterner(IdentifierInterner&& other) noexcept
    : arena_(std::move(other.arena_)), texts_(std::move(other.texts_)), ids_(std::move(other.ids_))
{
    other.texts_.clear();
    other.ids_.clear();
}

IdentifierInterner& IdentifierInterner::operator=(IdentifierInterner&& other) noexcept
{
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

std::size_t IdentifierTextHash::operator()(const std::string_view value) const noexcept
{
    return std::hash<std::string_view>{}(value);
}

StableHash64 stable_hash_text(const std::string_view text) noexcept
{
    base::u64 hash = SYNTAX_STABLE_HASH_OFFSET;
    for (const unsigned char byte : text) {
        hash ^= static_cast<base::u64>(byte);
        hash *= SYNTAX_STABLE_HASH_PRIME;
    }
    return StableHash64{hash};
}

void IdentifierInterner::reserve(const base::usize expected_identifiers)
{
    this->ensure_storage();
    this->texts_.reserve(expected_identifiers);
    this->ids_.reserve(expected_identifiers);
}

IdentId IdentifierInterner::intern(const std::string_view text)
{
    if (text.empty()) {
        return INVALID_IDENT_ID;
    }
    if (const IdentId existing = this->find(text); is_valid(existing)) {
        return existing;
    }

    this->ensure_storage();
    const IdentId id{static_cast<base::u32>(this->texts_.size())};
    const std::string_view stable_text = this->arena_->copy_string(text);
    this->texts_.push_back(stable_text);
    this->ids_.emplace(stable_text, id);
    return id;
}

IdentId IdentifierInterner::find(const std::string_view text) const noexcept
{
    const auto found = this->ids_.find(text);
    return found == this->ids_.end() ? INVALID_IDENT_ID : found->second;
}

std::string_view IdentifierInterner::text(const IdentId id) const noexcept
{
    if (!is_valid(id) || id.value >= this->texts_.size()) {
        return {};
    }
    return this->texts_[id.value];
}

StableHash64 IdentifierInterner::stable_hash(const IdentId id) const noexcept
{
    return stable_hash_text(this->text(id));
}

base::usize IdentifierInterner::size() const noexcept
{
    return this->texts_.size();
}

base::usize IdentifierInterner::arena_bytes() const noexcept
{
    return this->arena_ == nullptr ? 0 : this->arena_->allocated_bytes();
}

base::usize IdentifierInterner::arena_blocks() const noexcept
{
    return this->arena_ == nullptr ? 0 : this->arena_->block_count();
}

void IdentifierInterner::ensure_storage()
{
    if (this->arena_ != nullptr) {
        return;
    }
    this->arena_ = std::make_unique<base::BumpAllocator>();
    this->texts_ = TextVector(base::BumpAllocatorAdapter<std::string_view>{*this->arena_});
    this->ids_ =
        IdMap(0, IdentifierTextHash{}, std::equal_to<>{}, base::BumpAllocatorAdapter<IdMapEntry>{*this->arena_});
}

} // namespace aurex::syntax
