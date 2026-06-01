#include <aurex/base/bump_allocator.hpp>

#include <algorithm>
#include <limits>
#include <new>
#include <utility>

namespace aurex::base {

namespace {

constexpr usize BASE_BUMP_MIN_BLOCK_BYTES = 1024U;
constexpr usize BASE_BUMP_STRING_NUL_BYTES = 1U;
constexpr usize BASE_BUMP_ALIGNMENT_FLOOR = alignof(std::max_align_t);
constexpr usize BASE_BUMP_TOUCH_PAGE_BYTES = 4096U;
constexpr std::byte BASE_BUMP_TOUCH_VALUE{0};
constexpr std::string_view BASE_BUMP_ALLOCATE_CONTEXT = "bump allocator allocation";
constexpr std::string_view BASE_BUMP_ALIGN_CONTEXT = "bump allocator alignment";
constexpr std::string_view BASE_BUMP_STATS_CONTEXT = "bump allocator statistics";

} // namespace

BumpAllocator::Block::Block(std::byte* const data, const usize capacity, const usize alignment) noexcept
    : data(data), capacity(capacity), alignment(alignment)
{
}

BumpAllocator::Block::Block(Block&& other) noexcept
    : data(std::exchange(other.data, nullptr)), capacity(std::exchange(other.capacity, 0)),
      used(std::exchange(other.used, 0)), alignment(std::exchange(other.alignment, BASE_BUMP_ALIGNMENT_FLOOR))
{
}

BumpAllocator::Block::~Block()
{
    this->release();
}

void BumpAllocator::Block::release() noexcept
{
    if (this->data == nullptr) {
        return;
    }
    ::operator delete(this->data, std::align_val_t{this->alignment});
    this->data = nullptr;
    this->capacity = 0;
    this->used = 0;
    this->alignment = BASE_BUMP_ALIGNMENT_FLOOR;
}

BumpAllocator::BumpAllocator(const usize block_size) noexcept
    : block_size_(std::max(block_size, BASE_BUMP_MIN_BLOCK_BYTES))
{
}

BumpAllocator::BumpAllocator(BumpAllocator&& other) noexcept
    : block_size_(std::exchange(other.block_size_, BASE_BUMP_MIN_BLOCK_BYTES)), blocks_(std::move(other.blocks_)),
      allocated_bytes_(std::exchange(other.allocated_bytes_, 0))
{
}

BumpAllocator& BumpAllocator::operator=(BumpAllocator&& other) noexcept
{
    if (this == &other) {
        return *this;
    }
    this->blocks_.clear();
    this->block_size_ = std::exchange(other.block_size_, BASE_BUMP_MIN_BLOCK_BYTES);
    this->blocks_.swap(other.blocks_);
    this->allocated_bytes_ = std::exchange(other.allocated_bytes_, 0);
    return *this;
}

void* BumpAllocator::allocate(const usize size, const usize alignment)
{
    if (size == 0) {
        return nullptr;
    }
    const usize effective_alignment = normalize_alignment(alignment);
    if (this->blocks_.empty() || this->blocks_.back().alignment < effective_alignment) {
        this->add_block(checked_add_usize(size, effective_alignment, BASE_BUMP_ALLOCATE_CONTEXT), effective_alignment);
    }

    Block* block = &this->blocks_.back();
    const auto block_base = reinterpret_cast<usize>(block->data);
    usize aligned =
        align_address(checked_add_usize(block_base, block->used, BASE_BUMP_ALLOCATE_CONTEXT), effective_alignment)
        - block_base;
    if (aligned > block->capacity || size > block->capacity - aligned) {
        this->add_block(checked_add_usize(size, effective_alignment, BASE_BUMP_ALLOCATE_CONTEXT), effective_alignment);
        block = &this->blocks_.back();
        const auto new_block_base = reinterpret_cast<usize>(block->data);
        aligned = align_address(
                      checked_add_usize(new_block_base, block->used, BASE_BUMP_ALLOCATE_CONTEXT), effective_alignment)
            - new_block_base;
    }

    std::byte* const result = block->data + aligned;
    block->used = checked_add_usize(aligned, size, BASE_BUMP_ALLOCATE_CONTEXT);
    return result;
}

std::string_view BumpAllocator::copy_string(const std::string_view text)
{
    if (text.empty()) {
        return {};
    }
    char* const storage = static_cast<char*>(this->allocate(
        checked_add_usize(text.size(), BASE_BUMP_STRING_NUL_BYTES, BASE_BUMP_ALLOCATE_CONTEXT), alignof(char)));
    std::copy_n(text.data(), text.size(), storage);
    storage[text.size()] = '\0';
    return {storage, text.size()};
}

void BumpAllocator::reserve(const usize bytes)
{
    if (bytes == 0) {
        return;
    }
    if (!this->blocks_.empty()) {
        const Block& block = this->blocks_.back();
        if (block.capacity - block.used >= bytes) {
            return;
        }
    }
    this->add_block(bytes, BASE_BUMP_ALIGNMENT_FLOOR);
}

void BumpAllocator::reserve_touched(const usize bytes)
{
    if (bytes == 0) {
        return;
    }
    if (!this->blocks_.empty()) {
        const Block& block = this->blocks_.back();
        if (block.capacity - block.used >= bytes) {
            touch_memory(block.data + block.used, bytes);
            return;
        }
    }
    this->add_block(bytes, BASE_BUMP_ALIGNMENT_FLOOR, true);
}

void BumpAllocator::reset() noexcept
{
    this->blocks_.clear();
    this->allocated_bytes_ = 0;
}

usize BumpAllocator::allocated_bytes() const noexcept
{
    return this->allocated_bytes_;
}

usize BumpAllocator::used_bytes() const noexcept
{
    usize used = 0;
    for (const Block& block : this->blocks_) {
        used += block.used;
    }
    return used;
}

usize BumpAllocator::block_count() const noexcept
{
    return this->blocks_.size();
}

usize BumpAllocator::align_address(const usize address, const usize alignment)
{
    const usize remainder = address % alignment;
    return remainder == 0 ? address : checked_add_usize(address, alignment - remainder, BASE_BUMP_ALIGN_CONTEXT);
}

usize BumpAllocator::normalize_alignment(const usize alignment)
{
    const usize normalized = std::max(alignment, BASE_BUMP_ALIGNMENT_FLOOR);
    if ((normalized & (normalized - 1U)) == 0U) {
        return normalized;
    }
    usize power = BASE_BUMP_ALIGNMENT_FLOOR;
    while (power < normalized) {
        if (power > std::numeric_limits<usize>::max() / 2U) {
            throw std::bad_array_new_length();
        }
        power <<= 1U;
    }
    return power;
}

void BumpAllocator::touch_memory(std::byte* const data, const usize bytes) noexcept
{
    if (data == nullptr || bytes == 0) {
        return;
    }
    volatile std::byte* const touched = data;
    for (usize offset = 0; offset < bytes; offset += BASE_BUMP_TOUCH_PAGE_BYTES) {
        touched[offset] = BASE_BUMP_TOUCH_VALUE;
    }
    touched[bytes - 1U] = BASE_BUMP_TOUCH_VALUE;
}

void BumpAllocator::add_block(const usize min_capacity, const usize alignment, const bool touch_pages)
{
    const usize block_alignment = normalize_alignment(alignment);
    const usize capacity = std::max(this->block_size_, min_capacity);
    auto* const data = static_cast<std::byte*>(::operator new(capacity, std::align_val_t{block_alignment}));
    if (touch_pages) {
        touch_memory(data, capacity);
    }
    this->blocks_.push_back(Block{data, capacity, block_alignment});
    this->allocated_bytes_ = checked_add_usize(this->allocated_bytes_, capacity, BASE_BUMP_STATS_CONTEXT);
}

} // namespace aurex::base
