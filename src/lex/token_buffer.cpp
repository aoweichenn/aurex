#include <aurex/lex/token_buffer.hpp>

#include <utility>

namespace aurex::lex {

TokenBuffer::TokenBuffer()
    : arena_(std::make_unique<base::BumpAllocator>()), tokens_(base::BumpAllocatorAdapter<syntax::Token>{*this->arena_})
{
}

TokenBuffer::TokenBuffer(const TokenBuffer& other) : TokenBuffer()
{
    this->tokens_.assign(other.tokens_.begin(), other.tokens_.end());
}

TokenBuffer& TokenBuffer::operator=(const TokenBuffer& other)
{
    if (this == &other) {
        return *this;
    }
    TokenBuffer copy(other);
    *this = std::move(copy);
    return *this;
}

TokenBuffer::TokenBuffer(TokenBuffer&& other) noexcept
    : arena_(std::move(other.arena_)), tokens_(std::move(other.tokens_))
{
    other.tokens_ = base::BumpVector<syntax::Token>{};
}

TokenBuffer& TokenBuffer::operator=(TokenBuffer&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    this->swap(other);
    return *this;
}

void TokenBuffer::reserve(const base::usize token_count)
{
    this->ensure_storage();
    this->arena_->reserve(token_count * sizeof(syntax::Token));
    this->tokens_.reserve(token_count);
}

void TokenBuffer::push_back(const syntax::Token& token)
{
    this->ensure_storage();
    this->tokens_.push_back(token);
}

base::usize TokenBuffer::size() const noexcept
{
    return this->tokens_.size();
}

bool TokenBuffer::empty() const noexcept
{
    return this->tokens_.empty();
}

base::usize TokenBuffer::arena_bytes() const noexcept
{
    return this->arena_ == nullptr ? 0 : this->arena_->allocated_bytes();
}

base::usize TokenBuffer::arena_blocks() const noexcept
{
    return this->arena_ == nullptr ? 0 : this->arena_->block_count();
}

std::span<const syntax::Token> TokenBuffer::span() const noexcept
{
    return std::span<const syntax::Token>{this->tokens_.data(), this->tokens_.size()};
}

const syntax::Token& TokenBuffer::operator[](const base::usize index) const noexcept
{
    return this->tokens_[index];
}

const syntax::Token& TokenBuffer::front() const noexcept
{
    return this->tokens_.front();
}

const syntax::Token& TokenBuffer::back() const noexcept
{
    return this->tokens_.back();
}

const syntax::Token* TokenBuffer::begin() const noexcept
{
    return this->tokens_.data();
}

const syntax::Token* TokenBuffer::end() const noexcept
{
    return this->tokens_.data() + this->tokens_.size();
}

TokenBuffer::operator std::span<const syntax::Token>() const noexcept
{
    return this->span();
}

void TokenBuffer::ensure_storage()
{
    if (this->arena_ != nullptr) {
        return;
    }
    this->arena_ = std::make_unique<base::BumpAllocator>();
    this->tokens_ = base::BumpVector<syntax::Token>{base::BumpAllocatorAdapter<syntax::Token>{*this->arena_}};
}

void TokenBuffer::swap(TokenBuffer& other) noexcept
{
    using std::swap;
    swap(this->arena_, other.arena_);
    this->tokens_.swap(other.tokens_);
}

} // namespace aurex::lex
