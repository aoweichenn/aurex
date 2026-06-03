#include <aurex/infrastructure/query/query_interner.hpp>

#include <algorithm>
#include <functional>
#include <limits>
#include <utility>

namespace aurex::query {
namespace {

[[nodiscard]] base::usize query_node_id_index(const QueryNodeId id) noexcept
{
    return static_cast<base::usize>(id.value - QUERY_NODE_ID_FIRST_VALUE);
}

constexpr base::usize QUERY_INTERNER_MAX_NODE_COUNT = static_cast<base::usize>(std::numeric_limits<base::u32>::max());
constexpr std::string_view QUERY_INTERNER_NODE_ID_CONTEXT = "query interner node id";

} // namespace

QueryInterner::QueryInterner() noexcept : QueryInterner(QUERY_INTERNER_MAX_NODE_COUNT)
{
}

QueryInterner::QueryInterner(const base::usize max_node_count) noexcept
    : max_node_count_(std::min(max_node_count, QUERY_INTERNER_MAX_NODE_COUNT))
{
}

std::size_t QueryNodeIdHash::operator()(const QueryNodeId id) const noexcept
{
    return std::hash<base::u32>{}(id.value);
}

std::optional<QueryNodeId> QueryInterner::intern_key(const QueryKey key)
{
    if (!is_valid(key)) {
        return std::nullopt;
    }

    const auto found = this->ids_by_key_.find(key);
    if (found != this->ids_by_key_.end()) {
        return found->second;
    }
    if (!this->can_allocate_node_id()) {
        return std::nullopt;
    }

    const QueryNodeId id = this->next_node_id();
    this->identities_.push_back(QueryInternedIdentity{
        id,
        key,
        {},
        false,
    });
    this->ids_by_key_.emplace(key, id);
    return id;
}

std::optional<QueryNodeId> QueryInterner::intern_record(const QueryRecord& record)
{
    if (!query_record_stable_identity_is_valid(record)) {
        return std::nullopt;
    }

    std::optional<QueryNodeId> id = this->intern_key(record.key);
    if (!id.has_value() || !this->bind_record(*id, record)) {
        return std::nullopt;
    }
    return id;
}

bool QueryInterner::bind_record(const QueryNodeId id, const QueryRecord& record)
{
    if (!query_record_stable_identity_is_valid(record)) {
        return false;
    }

    QueryInternedIdentity* const identity = this->identity_for(id);
    if (identity == nullptr || identity->key != record.key) {
        return false;
    }
    if (identity->stable_identity_bound) {
        return identity->stable_key_bytes == record.stable_key_bytes;
    }

    identity->stable_key_bytes = record.stable_key_bytes;
    identity->stable_identity_bound = true;
    ++this->stable_identity_count_;
    return true;
}

std::optional<QueryNodeId> QueryInterner::find(const QueryKey key) const
{
    const auto found = this->ids_by_key_.find(key);
    if (found == this->ids_by_key_.end()) {
        return std::nullopt;
    }
    return found->second;
}

const QueryInternedIdentity* QueryInterner::find(const QueryNodeId id) const noexcept
{
    if (!is_valid(id)) {
        return nullptr;
    }

    const base::usize index = query_node_id_index(id);
    return index < this->identities_.size() ? &this->identities_[index] : nullptr;
}

std::optional<std::string_view> QueryInterner::stable_key_bytes(const QueryNodeId id) const noexcept
{
    const QueryInternedIdentity* const identity = this->find(id);
    if (identity == nullptr || !identity->stable_identity_bound) {
        return std::nullopt;
    }
    return std::string_view{identity->stable_key_bytes};
}

base::usize QueryInterner::size() const noexcept
{
    return this->identities_.size();
}

base::usize QueryInterner::stable_identity_count() const noexcept
{
    return this->stable_identity_count_;
}

QueryInternedIdentity* QueryInterner::identity_for(const QueryNodeId id) noexcept
{
    if (!is_valid(id)) {
        return nullptr;
    }

    const base::usize index = query_node_id_index(id);
    return index < this->identities_.size() ? &this->identities_[index] : nullptr;
}

bool QueryInterner::can_allocate_node_id() const noexcept
{
    return this->identities_.size() < this->max_node_count_;
}

QueryNodeId QueryInterner::next_node_id() const
{
    return QueryNodeId{
        base::checked_u32(base::checked_add_usize(
                              this->identities_.size(), QUERY_NODE_ID_FIRST_VALUE, QUERY_INTERNER_NODE_ID_CONTEXT),
            QUERY_INTERNER_NODE_ID_CONTEXT),
    };
}

} // namespace aurex::query
