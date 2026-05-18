#pragma once

#include <aurex/base/integer.hpp>

#include <cstddef>
#include <deque>
#include <functional>
#include <limits>
#include <new>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace aurex::base {

inline constexpr usize BASE_BUMP_DEFAULT_BLOCK_BYTES = 64U * 1024U;

class BumpAllocator final {
public:
    explicit BumpAllocator(usize block_size = BASE_BUMP_DEFAULT_BLOCK_BYTES) noexcept;
    BumpAllocator(const BumpAllocator&) = delete;
    BumpAllocator& operator=(const BumpAllocator&) = delete;
    BumpAllocator(BumpAllocator&& other) noexcept;
    BumpAllocator& operator=(BumpAllocator&& other) noexcept;
    ~BumpAllocator() = default;

    [[nodiscard]] void* allocate(usize size, usize alignment = alignof(std::max_align_t));
    [[nodiscard]] std::string_view copy_string(std::string_view text);
    void reserve(usize bytes);
    void reserve_touched(usize bytes);
    void reset() noexcept;

    [[nodiscard]] usize allocated_bytes() const noexcept;
    [[nodiscard]] usize used_bytes() const noexcept;
    [[nodiscard]] usize block_count() const noexcept;

private:
    struct Block {
        Block() = default;
        Block(std::byte* data, usize capacity, usize alignment) noexcept;
        Block(const Block&) = delete;
        Block& operator=(const Block&) = delete;
        Block(Block&& other) noexcept;
        Block& operator=(Block&& other) noexcept = delete;
        ~Block();

        void release() noexcept;

        std::byte* data = nullptr;
        usize capacity = 0;
        usize used = 0;
        usize alignment = alignof(std::max_align_t);
    };

    [[nodiscard]] static usize align_address(usize address, usize alignment) noexcept;
    [[nodiscard]] static usize normalize_alignment(usize alignment) noexcept;
    static void touch_memory(std::byte* data, usize bytes) noexcept;
    void add_block(usize min_capacity, usize alignment, bool touch_pages = false);

    usize block_size_ = BASE_BUMP_DEFAULT_BLOCK_BYTES;
    std::vector<Block> blocks_;
    usize allocated_bytes_ = 0;
};

template <typename T>
class BumpAllocatorAdapter {
public:
    using value_type = T;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::false_type;

    BumpAllocatorAdapter() noexcept = default;

    explicit BumpAllocatorAdapter(BumpAllocator& arena) noexcept : arena_(&arena)
    {
    }

    template <typename U>
    BumpAllocatorAdapter(const BumpAllocatorAdapter<U>& other) noexcept : arena_(other.arena_)
    {
    }

    [[nodiscard]] T* allocate(const std::size_t count)
    {
        if (count == 0) {
            return nullptr;
        }
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_array_new_length();
        }
        const std::size_t bytes = count * sizeof(T);
        if (this->arena_ == nullptr) {
            return static_cast<T*>(::operator new(bytes, std::align_val_t{alignof(T)}));
        }
        return static_cast<T*>(this->arena_->allocate(bytes, alignof(T)));
    }

    void deallocate(T* const pointer, const std::size_t) noexcept
    {
        if (this->arena_ == nullptr) {
            ::operator delete(pointer, std::align_val_t{alignof(T)});
        }
    }

    template <typename U>
    [[nodiscard]] friend bool operator==(const BumpAllocatorAdapter& lhs, const BumpAllocatorAdapter<U>& rhs) noexcept
    {
        return lhs.arena_ == rhs.arena_;
    }

    template <typename U>
    [[nodiscard]] friend bool operator!=(const BumpAllocatorAdapter& lhs, const BumpAllocatorAdapter<U>& rhs) noexcept
    {
        return !(lhs == rhs);
    }

private:
    template <typename>
    friend class BumpAllocatorAdapter;

    BumpAllocator* arena_ = nullptr;
};

template <typename T>
using BumpVector = std::vector<T, BumpAllocatorAdapter<T>>;

template <typename T>
using BumpDeque = std::deque<T, BumpAllocatorAdapter<T>>;

using BumpString = std::basic_string<char, std::char_traits<char>, BumpAllocatorAdapter<char>>;

template <typename Key, typename Value, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
using BumpUnorderedMap =
    std::unordered_map<Key, Value, Hash, KeyEqual, BumpAllocatorAdapter<std::pair<const Key, Value>>>;

template <typename Key, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
using BumpUnorderedSet = std::unordered_set<Key, Hash, KeyEqual, BumpAllocatorAdapter<Key>>;

} // namespace aurex::base
