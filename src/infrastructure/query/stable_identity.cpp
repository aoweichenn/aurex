#include <aurex/infrastructure/query/stable_identity.hpp>

#include <sstream>

namespace aurex::query {
namespace {

constexpr base::u64 QUERY_STABLE_MODULE_MARKER = 0x4d4f44554c455f49ULL;
constexpr base::u64 QUERY_STABLE_DEF_MARKER = 0x4445465f49445f31ULL;
constexpr base::u64 QUERY_STABLE_MEMBER_MARKER = 0x4d454d42455231ULL;
constexpr base::u64 QUERY_STABLE_INCREMENTAL_MARKER = 0x494e43525f4b4559ULL;

[[nodiscard]] base::u64 nonzero_stable_id(const base::u64 marker, const base::u64 value) noexcept
{
    return value == 0 ? marker : value;
}

[[nodiscard]] base::u64 mix_identity_fingerprint(base::u64 seed, const StableFingerprint128 fingerprint) noexcept
{
    seed = stable_mix(seed, fingerprint.primary);
    seed = stable_mix(seed, fingerprint.secondary);
    return seed;
}

[[nodiscard]] std::string stable_identity_debug(
    const std::string_view name, const base::u64 global_id, const StableFingerprint128 fingerprint)
{
    std::ostringstream out;
    out << name << "{global=" << global_id << ",fingerprint=" << debug_string(fingerprint) << '}';
    return out.str();
}

} // namespace

bool is_valid(const StableModuleId id) noexcept
{
    return id.global_id != 0;
}

bool is_valid(const StableDefId id) noexcept
{
    return is_valid(id.module) && id.kind != StableSymbolKind::invalid && id.global_id != 0;
}

bool is_valid(const StableMemberKey key) noexcept
{
    return is_valid(key.owner) && key.kind != StableSymbolKind::invalid && key.global_id != 0;
}

bool is_valid(const IncrementalKey key) noexcept
{
    return is_valid(key.definition) && key.global_id != 0;
}

StableFingerprint128 stable_identity_fingerprint(const std::span<const std::string_view> parts) noexcept
{
    StableHashBuilder builder;
    builder.mix_u64(QUERY_STABLE_MODULE_MARKER);
    for (base::usize index = 0; index < parts.size(); ++index) {
        builder.mix_u64(static_cast<base::u64>(index));
        builder.mix_bytes(parts[index]);
    }
    return builder.finish();
}

StableModuleId stable_module_id(const std::span<const std::string_view> module_path) noexcept
{
    const StableFingerprint128 path = stable_identity_fingerprint(module_path);
    base::u64 global_id = stable_mix(path.primary, path.secondary);
    global_id = stable_mix(global_id, static_cast<base::u64>(module_path.size()));
    return StableModuleId{
        path,
        static_cast<base::u32>(module_path.size()),
        nonzero_stable_id(QUERY_STABLE_MODULE_MARKER, global_id),
    };
}

StableDefId stable_definition_id(const StableModuleId& module, const StableSymbolKind kind, const std::string_view name,
    const base::u32 disambiguator) noexcept
{
    const StableFingerprint128 name_fingerprint = stable_fingerprint(name);
    base::u64 global_id = stable_mix(QUERY_STABLE_DEF_MARKER, module.global_id);
    global_id = stable_mix(global_id, static_cast<base::u64>(kind));
    global_id = mix_identity_fingerprint(global_id, name_fingerprint);
    global_id = stable_mix(global_id, disambiguator);
    return StableDefId{
        module,
        name_fingerprint,
        nonzero_stable_id(QUERY_STABLE_DEF_MARKER, global_id),
        disambiguator,
        kind,
    };
}

StableMemberKey stable_member_key(const StableDefId& owner, const StableSymbolKind kind,
    const std::string_view member_name, const base::u32 disambiguator) noexcept
{
    const StableFingerprint128 member_fingerprint = stable_fingerprint(member_name);
    base::u64 global_id = stable_mix(QUERY_STABLE_MEMBER_MARKER, owner.global_id);
    global_id = stable_mix(global_id, static_cast<base::u64>(kind));
    global_id = mix_identity_fingerprint(global_id, member_fingerprint);
    global_id = stable_mix(global_id, disambiguator);
    return StableMemberKey{
        owner,
        member_fingerprint,
        nonzero_stable_id(QUERY_STABLE_MEMBER_MARKER, global_id),
        disambiguator,
        kind,
    };
}

IncrementalKey stable_incremental_key(
    const StableDefId& definition, const std::string_view semantic_fingerprint) noexcept
{
    const StableFingerprint128 fingerprint = stable_fingerprint(semantic_fingerprint);
    base::u64 global_id = stable_mix(QUERY_STABLE_INCREMENTAL_MARKER, definition.global_id);
    global_id = mix_identity_fingerprint(global_id, fingerprint);
    return IncrementalKey{
        definition,
        fingerprint,
        nonzero_stable_id(QUERY_STABLE_INCREMENTAL_MARKER, global_id),
    };
}

void append_stable_key(StableKeyWriter& writer, const StableModuleId id)
{
    writer.write_u64(QUERY_STABLE_MODULE_MARKER);
    writer.write_fingerprint(id.path);
    writer.write_u32(id.part_count);
    writer.write_u64(id.global_id);
}

void append_stable_key(StableKeyWriter& writer, const StableDefId id)
{
    writer.write_u64(QUERY_STABLE_DEF_MARKER);
    append_stable_key(writer, id.module);
    writer.write_fingerprint(id.name);
    writer.write_u8(static_cast<base::u8>(id.kind));
    writer.write_u32(id.disambiguator);
    writer.write_u64(id.global_id);
}

void append_stable_key(StableKeyWriter& writer, const StableMemberKey key)
{
    writer.write_u64(QUERY_STABLE_MEMBER_MARKER);
    append_stable_key(writer, key.owner);
    writer.write_fingerprint(key.member_name);
    writer.write_u8(static_cast<base::u8>(key.kind));
    writer.write_u32(key.disambiguator);
    writer.write_u64(key.global_id);
}

void append_stable_key(StableKeyWriter& writer, const IncrementalKey key)
{
    writer.write_u64(QUERY_STABLE_INCREMENTAL_MARKER);
    append_stable_key(writer, key.definition);
    writer.write_fingerprint(key.fingerprint);
    writer.write_u64(key.global_id);
}

std::string stable_serialize(const StableModuleId id)
{
    StableKeyWriter writer;
    append_stable_key(writer, id);
    return writer.storage();
}

std::string stable_serialize(const StableDefId id)
{
    StableKeyWriter writer;
    append_stable_key(writer, id);
    return writer.storage();
}

std::string stable_serialize(const StableMemberKey key)
{
    StableKeyWriter writer;
    append_stable_key(writer, key);
    return writer.storage();
}

std::string stable_serialize(const IncrementalKey key)
{
    StableKeyWriter writer;
    append_stable_key(writer, key);
    return writer.storage();
}

StableFingerprint128 stable_key_fingerprint(const StableModuleId id)
{
    StableKeyWriter writer;
    append_stable_key(writer, id);
    return writer.fingerprint();
}

StableFingerprint128 stable_key_fingerprint(const StableDefId id)
{
    StableKeyWriter writer;
    append_stable_key(writer, id);
    return writer.fingerprint();
}

StableFingerprint128 stable_key_fingerprint(const StableMemberKey key)
{
    StableKeyWriter writer;
    append_stable_key(writer, key);
    return writer.fingerprint();
}

StableFingerprint128 stable_key_fingerprint(const IncrementalKey key)
{
    StableKeyWriter writer;
    append_stable_key(writer, key);
    return writer.fingerprint();
}

std::string debug_string(const StableModuleId id)
{
    return stable_identity_debug("StableModuleId", id.global_id, id.path);
}

std::string debug_string(const StableDefId id)
{
    return stable_identity_debug("StableDefId", id.global_id, id.name);
}

std::string debug_string(const StableMemberKey key)
{
    return stable_identity_debug("StableMemberKey", key.global_id, key.member_name);
}

std::string debug_string(const IncrementalKey key)
{
    return stable_identity_debug("IncrementalKey", key.global_id, key.fingerprint);
}

} // namespace aurex::query
