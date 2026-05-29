#include <aurex/tooling/session.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace aurex::tooling {
namespace {

using ToolingIndexFactMap = std::unordered_map<std::string, std::vector<ToolingIndexedSemanticFact>>;

constexpr std::string_view TOOLING_INDEX_KIND_ITEM_SIGNATURE = "item_signature";
constexpr std::string_view TOOLING_INDEX_KIND_GENERIC_TEMPLATE_SIGNATURE = "generic_template_signature";
constexpr std::string_view TOOLING_INDEX_KIND_FUNCTION_BODY_SYNTAX = "function_body_syntax";
constexpr std::string_view TOOLING_INDEX_KIND_TYPE_CHECK_BODY = "type_check_body";
constexpr char TOOLING_INDEX_FACT_IDENTITY_SEPARATOR = '\x1E';

[[nodiscard]] const base::SourceFile* tooling_index_source_file_for_range(
    const IdeSnapshot& snapshot, const base::SourceRange range) noexcept
{
    const std::span<const base::SourceFile> files = snapshot.sources.files();
    if (range.source.value >= files.size()) {
        return nullptr;
    }
    return &files[range.source.value];
}

[[nodiscard]] ToolingTextRange tooling_index_text_range_for_snapshot(
    const IdeSnapshot& snapshot, const base::SourceRange range)
{
    ToolingTextRange result;
    result.range = range;
    const base::SourceFile* const file = tooling_index_source_file_for_range(snapshot, range);
    if (file == nullptr) {
        return result;
    }
    result.path = std::string(file->path());
    result.start = file->line_column(range.begin);
    result.end = file->line_column(range.end);
    return result;
}

[[nodiscard]] std::string tooling_index_stable_key_or_empty(const query::QueryKey key)
{
    return query::is_valid(key) ? query::debug_string(key) : std::string{};
}

[[nodiscard]] std::string tooling_index_stable_key_or_empty(const query::DefKey key)
{
    return query::is_valid(key) ? query::debug_string(key) : std::string{};
}

[[nodiscard]] std::string tooling_index_stable_key_or_empty(const query::MemberKey key)
{
    return query::is_valid(key) ? query::debug_string(key) : std::string{};
}

[[nodiscard]] std::string tooling_index_stable_key_or_empty(const query::BodyKey key)
{
    return query::is_valid(key) ? query::debug_string(key) : std::string{};
}

[[nodiscard]] std::string tooling_index_stable_key_or_empty(const query::GenericInstanceKey& key)
{
    return query::is_valid(key) ? query::debug_string(key) : std::string{};
}

[[nodiscard]] std::string_view tooling_index_semantic_fact_kind_name(const IdeSemanticFactKind kind) noexcept
{
    switch (kind) {
        case IdeSemanticFactKind::item_signature:
            return TOOLING_INDEX_KIND_ITEM_SIGNATURE;
        case IdeSemanticFactKind::generic_template_signature:
            return TOOLING_INDEX_KIND_GENERIC_TEMPLATE_SIGNATURE;
        case IdeSemanticFactKind::function_body_syntax:
            return TOOLING_INDEX_KIND_FUNCTION_BODY_SYNTAX;
        case IdeSemanticFactKind::type_check_body:
            return TOOLING_INDEX_KIND_TYPE_CHECK_BODY;
    }
    return TOOLING_INDEX_KIND_ITEM_SIGNATURE;
}

[[nodiscard]] query::ModuleKey tooling_index_module_key_for_snapshot(const IdeSnapshot& snapshot)
{
    std::vector<std::string_view> parts = snapshot.ast.module_path.parts;
    if (parts.empty()) {
        parts.push_back("ide");
    }
    return query::module_key_from_stable_id(snapshot.query.source_stage.file.package, query::stable_module_id(parts));
}

[[nodiscard]] query::MemberKind tooling_index_symbol_kind_to_member_kind(const query::StableSymbolKind kind) noexcept
{
    if (kind == query::StableSymbolKind::enum_case) {
        return query::MemberKind::enum_case;
    }
    if (kind == query::StableSymbolKind::struct_field) {
        return query::MemberKind::struct_field;
    }
    return query::MemberKind::invalid;
}

[[nodiscard]] base::SourceRange tooling_index_name_range_in_range(
    const IdeSnapshot& snapshot, const std::string_view name, const base::SourceRange fallback) noexcept
{
    for (const syntax::Token& token : snapshot.lossless.tokens()) {
        if (token.kind == syntax::TokenKind::identifier && token.text() == name && fallback.begin <= token.range.begin
            && token.range.end <= fallback.end) {
            return token.range;
        }
    }
    return fallback;
}

[[nodiscard]] query::DefKey tooling_index_symbol_def_key(const IdeSnapshot& snapshot,
    const query::StableDefId stable_id, const query::DefNamespace name_space, const query::DefKind kind,
    const std::string_view fallback_name, const base::SourceRange fallback_range)
{
    if (query::is_valid(stable_id)) {
        return query::def_key_from_stable_id(
            tooling_index_module_key_for_snapshot(snapshot).package, stable_id, name_space, kind);
    }
    const std::array<std::string_view, 1> path{fallback_name};
    return query::def_key(tooling_index_module_key_for_snapshot(snapshot), name_space, kind, path,
        static_cast<base::u32>(fallback_range.begin));
}

[[nodiscard]] query::MemberKey tooling_index_member_key(const IdeSnapshot& snapshot,
    const query::StableMemberKey stable_key, const query::DefKind owner_kind, const std::string_view fallback_name)
{
    const query::MemberKind kind = tooling_index_symbol_kind_to_member_kind(stable_key.kind);
    if (!query::is_valid(stable_key) || kind == query::MemberKind::invalid || owner_kind == query::DefKind::invalid) {
        return {};
    }
    const query::DefKey owner = query::def_key_from_stable_id(tooling_index_module_key_for_snapshot(snapshot).package,
        stable_key.owner, query::DefNamespace::type, owner_kind);
    if (!query::is_valid(owner)) {
        return {};
    }
    const query::MemberKey member = query::member_key(owner, kind, fallback_name, stable_key.disambiguator);
    return query::is_valid(member) ? member : query::MemberKey{};
}

[[nodiscard]] ToolingIndexedSemanticFact tooling_indexed_fact_from_semantic_fact(
    const ToolingSnapshotHandle& handle, const IdeSemanticFact& fact)
{
    return ToolingIndexedSemanticFact{
        handle.document,
        handle.version,
        fact.kind,
        fact.query,
        fact.definition,
        fact.member,
        fact.body,
        fact.generic_instance,
        tooling_index_text_range_for_snapshot(*handle.snapshot, fact.range),
        fact.name,
        std::string(tooling_index_semantic_fact_kind_name(fact.kind)),
        fact.detail,
        tooling_index_stable_key_or_empty(fact.query),
        tooling_index_stable_key_or_empty(fact.definition),
        tooling_index_stable_key_or_empty(fact.member),
        tooling_index_stable_key_or_empty(fact.body),
        tooling_index_stable_key_or_empty(fact.generic_instance),
        fact.part_index,
        fact.checked,
    };
}

[[nodiscard]] ToolingIndexedSemanticFact tooling_indexed_member_fact(const ToolingSnapshotHandle& handle,
    const query::DefKey definition, const query::MemberKey member, const query::GenericInstanceKey& generic_instance,
    const base::SourceRange range, const std::string_view name, const base::u32 part_index)
{
    return ToolingIndexedSemanticFact{
        handle.document,
        handle.version,
        IdeSemanticFactKind::item_signature,
        {},
        definition,
        member,
        {},
        generic_instance,
        tooling_index_text_range_for_snapshot(*handle.snapshot, range),
        std::string(name),
        std::string(TOOLING_INDEX_KIND_ITEM_SIGNATURE),
        std::string(TOOLING_INDEX_KIND_ITEM_SIGNATURE),
        {},
        tooling_index_stable_key_or_empty(definition),
        tooling_index_stable_key_or_empty(member),
        {},
        tooling_index_stable_key_or_empty(generic_instance),
        part_index,
        true,
    };
}

void tooling_index_sort_facts(std::vector<ToolingIndexedSemanticFact>& facts)
{
    std::sort(
        facts.begin(), facts.end(), [](const ToolingIndexedSemanticFact& lhs, const ToolingIndexedSemanticFact& rhs) {
            return std::tie(lhs.document.uri, lhs.range.range.begin, lhs.range.range.end, lhs.kind, lhs.name)
                < std::tie(rhs.document.uri, rhs.range.range.begin, rhs.range.range.end, rhs.kind, rhs.name);
        });
}

void tooling_index_push_fact(
    ToolingIndexFactMap& map, const std::string& stable_key, const ToolingIndexedSemanticFact& fact)
{
    if (stable_key.empty()) {
        return;
    }
    map[stable_key].push_back(fact);
}

void tooling_index_append_identity_part(std::string& identity, const std::string_view part)
{
    identity.push_back(TOOLING_INDEX_FACT_IDENTITY_SEPARATOR);
    identity.append(part);
}

[[nodiscard]] std::string tooling_index_fact_identity(const ToolingIndexedSemanticFact& fact)
{
    std::string identity;
    tooling_index_append_identity_part(identity, fact.kind);
    if (!fact.stable_query_key.empty()) {
        tooling_index_append_identity_part(identity, "query");
        tooling_index_append_identity_part(identity, fact.stable_query_key);
        return identity;
    }
    if (!fact.stable_member_key.empty()) {
        tooling_index_append_identity_part(identity, "member");
        tooling_index_append_identity_part(identity, fact.stable_member_key);
        return identity;
    }
    if (!fact.stable_body_key.empty()) {
        tooling_index_append_identity_part(identity, "body");
        tooling_index_append_identity_part(identity, fact.stable_body_key);
        return identity;
    }
    if (!fact.stable_definition_key.empty()) {
        tooling_index_append_identity_part(identity, "definition");
        tooling_index_append_identity_part(identity, fact.stable_definition_key);
        return identity;
    }
    if (!fact.stable_generic_instance_key.empty()) {
        tooling_index_append_identity_part(identity, "generic_instance");
        tooling_index_append_identity_part(identity, fact.stable_generic_instance_key);
        return identity;
    }
    tooling_index_append_identity_part(identity, "range");
    tooling_index_append_identity_part(identity, fact.name);
    tooling_index_append_identity_part(identity, std::to_string(fact.range.range.source.value));
    tooling_index_append_identity_part(identity, std::to_string(fact.range.range.begin));
    tooling_index_append_identity_part(identity, std::to_string(fact.range.range.end));
    return identity;
}

[[nodiscard]] bool tooling_index_same_range(const ToolingTextRange& lhs, const ToolingTextRange& rhs) noexcept
{
    return lhs.path == rhs.path && lhs.range.source.value == rhs.range.source.value
        && lhs.range.begin == rhs.range.begin && lhs.range.end == rhs.range.end && lhs.start.line == rhs.start.line
        && lhs.start.column == rhs.start.column && lhs.end.line == rhs.end.line && lhs.end.column == rhs.end.column;
}

[[nodiscard]] bool tooling_index_same_semantic_payload(
    const ToolingIndexedSemanticFact& lhs, const ToolingIndexedSemanticFact& rhs) noexcept
{
    return lhs.semantic_kind == rhs.semantic_kind && lhs.query == rhs.query && lhs.definition == rhs.definition
        && lhs.member == rhs.member && lhs.body == rhs.body && lhs.generic_instance == rhs.generic_instance
        && tooling_index_same_range(lhs.range, rhs.range) && lhs.name == rhs.name && lhs.kind == rhs.kind
        && lhs.detail == rhs.detail && lhs.stable_query_key == rhs.stable_query_key
        && lhs.stable_definition_key == rhs.stable_definition_key && lhs.stable_member_key == rhs.stable_member_key
        && lhs.stable_body_key == rhs.stable_body_key
        && lhs.stable_generic_instance_key == rhs.stable_generic_instance_key && lhs.part_index == rhs.part_index
        && lhs.checked == rhs.checked;
}

[[nodiscard]] std::unordered_map<std::string, ToolingIndexedSemanticFact> tooling_index_identity_map(
    const std::vector<ToolingIndexedSemanticFact>& facts)
{
    std::unordered_map<std::string, ToolingIndexedSemanticFact> by_identity;
    by_identity.reserve(facts.size());
    for (const ToolingIndexedSemanticFact& fact : facts) {
        by_identity[tooling_index_fact_identity(fact)] = fact;
    }
    return by_identity;
}

[[nodiscard]] ToolingWorkspaceIndexUpdateStats tooling_index_update_stats(
    const std::vector<ToolingIndexedSemanticFact>& previous, const std::vector<ToolingIndexedSemanticFact>& current)
{
    ToolingWorkspaceIndexUpdateStats stats;
    stats.previous_document_facts = previous.size();
    stats.current_document_facts = current.size();
    const std::unordered_map<std::string, ToolingIndexedSemanticFact> previous_by_identity =
        tooling_index_identity_map(previous);
    const std::unordered_map<std::string, ToolingIndexedSemanticFact> current_by_identity =
        tooling_index_identity_map(current);
    for (const auto& entry : previous_by_identity) {
        if (!current_by_identity.contains(entry.first)) {
            stats.removed_facts += 1;
        }
    }
    for (const auto& entry : current_by_identity) {
        const auto previous_entry = previous_by_identity.find(entry.first);
        if (previous_entry == previous_by_identity.end()) {
            stats.inserted_facts += 1;
            continue;
        }
        if (tooling_index_same_semantic_payload(previous_entry->second, entry.second)) {
            stats.retained_facts += 1;
            continue;
        }
        stats.replaced_facts += 1;
    }
    stats.updated = true;
    return stats;
}

[[nodiscard]] std::vector<ToolingIndexedSemanticFact> tooling_index_collect_snapshot_facts(
    const ToolingSnapshotHandle& handle)
{
    std::vector<ToolingIndexedSemanticFact> facts;
    facts.reserve(handle.snapshot->query.semantic_facts.size());
    for (const IdeSemanticFact& fact : handle.snapshot->query.semantic_facts) {
        if (!query::is_valid(fact.query)) {
            continue;
        }
        facts.push_back(tooling_indexed_fact_from_semantic_fact(handle, fact));
    }
    for (const auto& entry : handle.snapshot->checked.structs) {
        const sema::StructInfo& info = entry.second;
        for (const sema::StructFieldInfo& field : info.fields) {
            const query::MemberKey member = tooling_index_member_key(
                *handle.snapshot, field.stable_key, query::DefKind::struct_, field.name.view());
            if (!query::is_valid(member)) {
                continue;
            }
            const query::DefKey definition = tooling_index_symbol_def_key(*handle.snapshot, field.stable_key.owner,
                query::DefNamespace::member, query::DefKind::struct_field, field.name.view(), field.range);
            if (!query::is_valid(definition)) {
                continue;
            }
            const base::SourceRange range =
                tooling_index_name_range_in_range(*handle.snapshot, field.name.view(), field.range);
            facts.push_back(tooling_indexed_member_fact(
                handle, definition, member, info.generic_instance_key, range, field.name.view(), info.part_index));
        }
    }
    for (const auto& entry : handle.snapshot->checked.enum_cases) {
        const sema::EnumCaseInfo& info = entry.second;
        const query::MemberKey member = tooling_index_member_key(
            *handle.snapshot, info.stable_case_key, query::DefKind::enum_, info.case_name.view());
        if (!query::is_valid(member)) {
            continue;
        }
        const query::DefKey definition = tooling_index_symbol_def_key(*handle.snapshot, info.stable_id,
            query::DefNamespace::value, query::DefKind::enum_case, info.case_name.view(), info.range);
        if (!query::is_valid(definition)) {
            continue;
        }
        const base::SourceRange range =
            tooling_index_name_range_in_range(*handle.snapshot, info.case_name.view(), info.range);
        facts.push_back(tooling_indexed_member_fact(
            handle, definition, member, info.generic_instance_key, range, info.case_name.view(), info.part_index));
    }
    tooling_index_sort_facts(facts);
    return facts;
}

[[nodiscard]] base::usize tooling_index_fact_count(const ToolingIndexFactMap& map) noexcept
{
    base::usize count = 0;
    for (const auto& entry : map) {
        count += entry.second.size();
    }
    return count;
}

} // namespace

void ToolingWorkspaceSemanticIndex::clear()
{
    this->facts_by_document_.clear();
    this->definitions_.clear();
    this->members_.clear();
    this->bodies_.clear();
    this->generic_instances_.clear();
    this->stats_ = {};
}

void ToolingWorkspaceSemanticIndex::erase_document_from_map(FactMap& map, const std::string_view document_key)
{
    for (auto it = map.begin(); it != map.end();) {
        std::vector<ToolingIndexedSemanticFact>& facts = it->second;
        std::erase_if(facts, [document_key](const ToolingIndexedSemanticFact& fact) {
            return tooling_document_store_key(fact.document) == document_key;
        });
        if (facts.empty()) {
            it = map.erase(it);
            continue;
        }
        ++it;
    }
}

void ToolingWorkspaceSemanticIndex::remove_document(const ToolingDocumentId& id)
{
    const std::string document_key = tooling_document_store_key(id);
    this->facts_by_document_.erase(document_key);
    erase_document_from_map(this->definitions_, document_key);
    erase_document_from_map(this->members_, document_key);
    erase_document_from_map(this->bodies_, document_key);
    erase_document_from_map(this->generic_instances_, document_key);
    this->rebuild_stats();
}

void ToolingWorkspaceSemanticIndex::index_snapshot(const ToolingSnapshotHandle& handle)
{
    static_cast<void>(this->update_snapshot(handle));
}

ToolingWorkspaceIndexUpdateStats ToolingWorkspaceSemanticIndex::update_snapshot(const ToolingSnapshotHandle& handle)
{
    return this->update_snapshot(handle, this->facts_for_document(handle.document));
}

ToolingWorkspaceIndexUpdateStats ToolingWorkspaceSemanticIndex::update_snapshot(
    const ToolingSnapshotHandle& handle, const std::vector<ToolingIndexedSemanticFact>& previous_facts)
{
    if (!handle || handle.snapshot == nullptr) {
        return {};
    }
    std::vector<ToolingIndexedSemanticFact> current_facts = tooling_index_collect_snapshot_facts(handle);
    ToolingWorkspaceIndexUpdateStats update = tooling_index_update_stats(previous_facts, current_facts);

    this->remove_document(handle.document);
    const std::string document_key = tooling_document_store_key(handle.document);
    std::vector<ToolingIndexedSemanticFact>& document_facts = this->facts_by_document_[document_key];
    document_facts = std::move(current_facts);
    for (const ToolingIndexedSemanticFact& fact : document_facts) {
        tooling_index_push_fact(this->definitions_, fact.stable_definition_key, fact);
        tooling_index_push_fact(this->members_, fact.stable_member_key, fact);
        tooling_index_push_fact(this->bodies_, fact.stable_body_key, fact);
        tooling_index_push_fact(this->generic_instances_, fact.stable_generic_instance_key, fact);
    }
    this->rebuild_stats();
    return update;
}

ToolingWorkspaceIndexStats ToolingWorkspaceSemanticIndex::stats() const noexcept
{
    return this->stats_;
}

std::vector<ToolingIndexedSemanticFact> ToolingWorkspaceSemanticIndex::all_facts() const
{
    std::vector<ToolingIndexedSemanticFact> facts;
    facts.reserve(this->stats_.facts);
    for (const auto& entry : this->facts_by_document_) {
        facts.insert(facts.end(), entry.second.begin(), entry.second.end());
    }
    tooling_index_sort_facts(facts);
    return facts;
}

std::vector<ToolingIndexedSemanticFact> ToolingWorkspaceSemanticIndex::facts_for_document(
    const ToolingDocumentId& id) const
{
    const auto found = this->facts_by_document_.find(tooling_document_store_key(id));
    if (found == this->facts_by_document_.end()) {
        return {};
    }
    std::vector<ToolingIndexedSemanticFact> facts = found->second;
    tooling_index_sort_facts(facts);
    return facts;
}

std::vector<ToolingIndexedSemanticFact> ToolingWorkspaceSemanticIndex::facts_for_key(
    const FactMap& map, const std::string_view stable_key)
{
    const auto found = map.find(std::string(stable_key));
    if (found == map.end()) {
        return {};
    }
    std::vector<ToolingIndexedSemanticFact> facts = found->second;
    tooling_index_sort_facts(facts);
    return facts;
}

std::vector<ToolingIndexedSemanticFact> ToolingWorkspaceSemanticIndex::definitions(const query::DefKey key) const
{
    return facts_for_key(this->definitions_, tooling_index_stable_key_or_empty(key));
}

std::vector<ToolingIndexedSemanticFact> ToolingWorkspaceSemanticIndex::members(const query::MemberKey key) const
{
    return facts_for_key(this->members_, tooling_index_stable_key_or_empty(key));
}

std::vector<ToolingIndexedSemanticFact> ToolingWorkspaceSemanticIndex::bodies(const query::BodyKey key) const
{
    return facts_for_key(this->bodies_, tooling_index_stable_key_or_empty(key));
}

std::vector<ToolingIndexedSemanticFact> ToolingWorkspaceSemanticIndex::generic_instances(
    const query::GenericInstanceKey& key) const
{
    return facts_for_key(this->generic_instances_, tooling_index_stable_key_or_empty(key));
}

void ToolingWorkspaceSemanticIndex::rebuild_stats() noexcept
{
    this->stats_ = {};
    this->stats_.documents = this->facts_by_document_.size();
    this->stats_.facts = tooling_index_fact_count(this->facts_by_document_);
    this->stats_.definitions = tooling_index_fact_count(this->definitions_);
    this->stats_.members = tooling_index_fact_count(this->members_);
    this->stats_.bodies = tooling_index_fact_count(this->bodies_);
    this->stats_.generic_instances = tooling_index_fact_count(this->generic_instances_);
}

} // namespace aurex::tooling
