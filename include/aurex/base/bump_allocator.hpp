#pragma once

#include <aurex/base/integer.hpp>

#include <cstddef>
#include <string_view>
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
    void reset() noexcept;

    [[nodiscard]] usize allocated_bytes() const noexcept;
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
    void add_block(usize min_capacity, usize alignment);

    usize block_size_ = BASE_BUMP_DEFAULT_BLOCK_BYTES;
    std::vector<Block> blocks_;
    usize allocated_bytes_ = 0;
};

} // namespace aurex::base
