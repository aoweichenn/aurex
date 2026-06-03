#pragma once

#include <aurex/infrastructure/query/query_result.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace aurex::query {

inline constexpr base::u32 QUERY_NODE_ID_INVALID_VALUE = 0;
inline constexpr base::u32 QUERY_NODE_ID_FIRST_VALUE = 1;

struct QueryNodeId {
    base::u32 value = QUERY_NODE_ID_INVALID_VALUE;

    [[nodiscard]] friend constexpr bool operator==(QueryNodeId lhs, QueryNodeId rhs) noexcept = default;
};

struct QueryInternedIdentity {
    QueryNodeId id;
    QueryKey key;
    std::string stable_key_bytes;
    bool stable_identity_bound = false;
};

[[nodiscard]] constexpr bool is_valid(QueryNodeId id) noexcept
{
    return id.value != QUERY_NODE_ID_INVALID_VALUE;
}

struct QueryNodeIdHash {
    [[nodiscard]] std::size_t operator()(QueryNodeId id) const noexcept;
};

class QueryInterner final {
public:
    QueryInterner() noexcept;
    explicit QueryInterner(base::usize max_node_count) noexcept;

    [[nodiscard]] std::optional<QueryNodeId> intern_key(QueryKey key);
    [[nodiscard]] std::optional<QueryNodeId> intern_record(const QueryRecord& record);
    [[nodiscard]] bool bind_record(QueryNodeId id, const QueryRecord& record);
    [[nodiscard]] std::optional<QueryNodeId> find(QueryKey key) const;
    [[nodiscard]] const QueryInternedIdentity* find(QueryNodeId id) const noexcept;
    [[nodiscard]] std::optional<std::string_view> stable_key_bytes(QueryNodeId id) const noexcept;
    [[nodiscard]] base::usize size() const noexcept;
    [[nodiscard]] base::usize stable_identity_count() const noexcept;

private:
    [[nodiscard]] QueryInternedIdentity* identity_for(QueryNodeId id) noexcept;
    [[nodiscard]] bool can_allocate_node_id() const noexcept;
    [[nodiscard]] QueryNodeId next_node_id() const;

    std::vector<QueryInternedIdentity> identities_;
    std::unordered_map<QueryKey, QueryNodeId, QueryKeyHash> ids_by_key_;
    base::usize stable_identity_count_ = 0;
    base::usize max_node_count_ = 0;
};

} // namespace aurex::query
