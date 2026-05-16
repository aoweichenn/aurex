#pragma once

#include <aurex/base/bump_allocator.hpp>

#include <functional>
#include <utility>

namespace aurex::sema {

template <typename T>
using SemaVector = base::BumpVector<T>;

template <typename T>
using SemaDeque = base::BumpDeque<T>;

template <
    typename Key,
    typename Value,
    typename Hash = std::hash<Key>,
    typename KeyEqual = std::equal_to<Key>>
using SemaMap = base::BumpUnorderedMap<Key, Value, Hash, KeyEqual>;

template <
    typename Key,
    typename Hash = std::hash<Key>,
    typename KeyEqual = std::equal_to<Key>>
using SemaSet = base::BumpUnorderedSet<Key, Hash, KeyEqual>;

template <typename T>
[[nodiscard]] SemaVector<T> make_sema_vector(base::BumpAllocator& arena) {
    return SemaVector<T> {base::BumpAllocatorAdapter<T> {arena}};
}

template <typename T>
[[nodiscard]] SemaDeque<T> make_sema_deque(base::BumpAllocator& arena) {
    return SemaDeque<T> {base::BumpAllocatorAdapter<T> {arena}};
}

template <
    typename Key,
    typename Value,
    typename Hash = std::hash<Key>,
    typename KeyEqual = std::equal_to<Key>>
[[nodiscard]] SemaMap<Key, Value, Hash, KeyEqual> make_sema_map(
    base::BumpAllocator& arena,
    Hash hash = Hash {},
    KeyEqual equal = KeyEqual {}
) {
    return SemaMap<Key, Value, Hash, KeyEqual>(
        0,
        std::move(hash),
        std::move(equal),
        base::BumpAllocatorAdapter<std::pair<const Key, Value>> {arena}
    );
}

template <
    typename Key,
    typename Hash = std::hash<Key>,
    typename KeyEqual = std::equal_to<Key>>
[[nodiscard]] SemaSet<Key, Hash, KeyEqual> make_sema_set(
    base::BumpAllocator& arena,
    Hash hash = Hash {},
    KeyEqual equal = KeyEqual {}
) {
    return SemaSet<Key, Hash, KeyEqual>(
        0,
        std::move(hash),
        std::move(equal),
        base::BumpAllocatorAdapter<Key> {arena}
    );
}

} // namespace aurex::sema
