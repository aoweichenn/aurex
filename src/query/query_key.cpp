#include <aurex/query/query_key.hpp>

#include <sstream>

namespace aurex::query {
namespace {

constexpr base::u64 QUERY_PACKAGE_KEY_MARKER = 0x51504b4759313031ULL;
constexpr base::u64 QUERY_FILE_KEY_MARKER = 0x5146494c45593031ULL;
constexpr base::u64 QUERY_MODULE_KEY_MARKER = 0x514d4f4459303031ULL;
constexpr base::u64 QUERY_MODULE_PART_KEY_MARKER = 0x514d504152543031ULL;
constexpr base::u64 QUERY_DEF_KEY_MARKER = 0x514445464b455931ULL;
constexpr base::u64 QUERY_MEMBER_KEY_MARKER = 0x514d454d4b455931ULL;
constexpr base::u64 QUERY_BODY_KEY_MARKER = 0x51424f4459303031ULL;
constexpr base::u64 QUERY_GENERIC_PARAM_KEY_MARKER = 0x51475041524d3031ULL;
constexpr base::u64 QUERY_QUERY_KEY_MARKER = 0x51554552594b3031ULL;
constexpr base::u32 QUERY_DEF_KEY_STABLE_ID_PATH_COMPONENT_COUNT = 1;

[[nodiscard]] base::u64 mix_key_field(const base::u64 seed, const base::u64 value) noexcept
{
    const base::u64 result = stable_mix(seed, value);
    return result == 0 ? seed : result;
}

[[nodiscard]] base::u64 nonzero_global_id(const base::u64 marker, const StableFingerprint128 fingerprint) noexcept
{
    base::u64 global_id = mix_key_field(marker, fingerprint.primary);
    global_id = mix_key_field(global_id, fingerprint.secondary);
    global_id = mix_key_field(global_id, fingerprint.byte_count);
    return global_id;
}

[[nodiscard]] base::u64 mix_key_fingerprint(base::u64 seed, const StableFingerprint128 fingerprint) noexcept
{
    seed = mix_key_field(seed, fingerprint.primary);
    seed = mix_key_field(seed, fingerprint.secondary);
    seed = mix_key_field(seed, fingerprint.byte_count);
    return seed;
}

[[nodiscard]] std::string finish_debug_string(
    const std::string_view name, const base::u64 global_id, const StableFingerprint128 fingerprint)
{
    std::ostringstream out;
    out << name << "{global=" << global_id << ",fingerprint=" << debug_string(fingerprint) << '}';
    return out.str();
}

template <typename Key>
[[nodiscard]] std::string serialize_with(void (*append)(StableKeyWriter&, Key), const Key key)
{
    StableKeyWriter writer;
    append(writer, key);
    return writer.storage();
}

template <typename Key>
[[nodiscard]] StableFingerprint128 fingerprint_with(void (*append)(StableKeyWriter&, Key), const Key key)
{
    StableKeyWriter writer;
    append(writer, key);
    return writer.fingerprint();
}

} // namespace

bool is_valid(const PackageKey key) noexcept
{
    return key.global_id != 0;
}

bool is_valid(const FileKey key) noexcept
{
    return is_valid(key.package) && key.global_id != 0;
}

bool is_valid(const ModuleKey key) noexcept
{
    return is_valid(key.package) && key.global_id != 0;
}

bool is_valid(const DefKey key) noexcept
{
    return is_valid(key.module) && key.kind != DefKind::invalid && key.global_id != 0;
}

bool is_valid(const MemberKey key) noexcept
{
    return is_valid(key.owner) && key.kind != MemberKind::invalid && key.global_id != 0;
}

bool is_valid(const BodyKey key) noexcept
{
    return is_valid(key.owner) && key.global_id != 0;
}

bool is_valid(const GenericParamKey key) noexcept
{
    return is_valid(key.owner) && key.global_id != 0;
}

bool is_valid(const QueryKey key) noexcept
{
    return key.kind != QueryKind::invalid && key.global_id != 0;
}

PackageKey package_key(const std::span<const std::string_view> identity_parts) noexcept
{
    const StableFingerprint128 identity = stable_fingerprint(identity_parts);
    return PackageKey{
        identity,
        nonzero_global_id(QUERY_PACKAGE_KEY_MARKER, identity),
    };
}

FileKey file_key(const PackageKey package, const std::string_view canonical_path, const SourceRole role,
    const std::string_view virtual_buffer) noexcept
{
    const StableFingerprint128 path = stable_fingerprint(canonical_path);
    const StableFingerprint128 virtual_id = stable_fingerprint(virtual_buffer);
    base::u64 global_id = mix_key_field(QUERY_FILE_KEY_MARKER, package.global_id);
    global_id = mix_key_fingerprint(global_id, path);
    global_id = mix_key_fingerprint(global_id, virtual_id);
    global_id = mix_key_field(global_id, static_cast<base::u64>(role));
    return FileKey{
        package,
        path,
        virtual_id,
        role,
        global_id,
    };
}

ModuleKey module_key(
    const PackageKey package, const std::span<const std::string_view> module_path, const ModuleKind kind) noexcept
{
    const StableFingerprint128 path = stable_fingerprint(module_path);
    base::u64 global_id = mix_key_field(QUERY_MODULE_KEY_MARKER, package.global_id);
    global_id = mix_key_fingerprint(global_id, path);
    global_id = mix_key_field(global_id, module_path.size());
    global_id = mix_key_field(global_id, static_cast<base::u64>(kind));
    return ModuleKey{
        package,
        path,
        static_cast<base::u32>(module_path.size()),
        kind,
        global_id,
    };
}

ModulePartKey module_part_key(const ModuleKey module, const FileKey file, const ModulePartKind kind,
    const std::string_view name, const base::u32 stable_index) noexcept
{
    const StableFingerprint128 part_name = stable_fingerprint(name);
    base::u64 global_id = mix_key_field(QUERY_MODULE_PART_KEY_MARKER, module.global_id);
    global_id = mix_key_field(global_id, file.global_id);
    global_id = mix_key_fingerprint(global_id, part_name);
    global_id = mix_key_field(global_id, stable_index);
    global_id = mix_key_field(global_id, static_cast<base::u64>(kind));
    return ModulePartKey{
        module,
        file,
        part_name,
        stable_index,
        kind,
        global_id,
    };
}

DefKey def_key(const ModuleKey module, const DefNamespace name_space, const DefKind kind,
    const std::span<const std::string_view> path, const base::u32 disambiguator) noexcept
{
    const StableFingerprint128 path_fingerprint = stable_fingerprint(path);
    base::u64 global_id = mix_key_field(QUERY_DEF_KEY_MARKER, module.global_id);
    global_id = mix_key_fingerprint(global_id, path_fingerprint);
    global_id = mix_key_field(global_id, path.size());
    global_id = mix_key_field(global_id, static_cast<base::u64>(name_space));
    global_id = mix_key_field(global_id, static_cast<base::u64>(kind));
    global_id = mix_key_field(global_id, disambiguator);
    return DefKey{
        module,
        path_fingerprint,
        static_cast<base::u32>(path.size()),
        name_space,
        kind,
        disambiguator,
        global_id,
    };
}

MemberKey member_key(
    const DefKey owner, const MemberKind kind, const std::string_view name, const base::u32 ordinal) noexcept
{
    const StableFingerprint128 member_name = stable_fingerprint(name);
    base::u64 global_id = mix_key_field(QUERY_MEMBER_KEY_MARKER, owner.global_id);
    global_id = mix_key_fingerprint(global_id, member_name);
    global_id = mix_key_field(global_id, ordinal);
    global_id = mix_key_field(global_id, static_cast<base::u64>(kind));
    return MemberKey{
        owner,
        member_name,
        kind,
        ordinal,
        global_id,
    };
}

BodyKey body_key(const DefKey owner, const BodySlotKind slot, const base::u32 ordinal) noexcept
{
    base::u64 global_id = mix_key_field(QUERY_BODY_KEY_MARKER, owner.global_id);
    global_id = mix_key_field(global_id, static_cast<base::u64>(slot));
    global_id = mix_key_field(global_id, ordinal);
    return BodyKey{
        owner,
        slot,
        ordinal,
        global_id,
    };
}

GenericParamKey generic_param_key(const DefKey owner, const base::u32 index, const GenericParamKind kind) noexcept
{
    base::u64 global_id = mix_key_field(QUERY_GENERIC_PARAM_KEY_MARKER, owner.global_id);
    global_id = mix_key_field(global_id, index);
    global_id = mix_key_field(global_id, static_cast<base::u64>(kind));
    return GenericParamKey{
        owner,
        index,
        kind,
        global_id,
    };
}

QueryKey query_key(const QueryKind kind, const StableFingerprint128 payload, const base::u16 schema) noexcept
{
    base::u64 global_id = mix_key_field(QUERY_QUERY_KEY_MARKER, static_cast<base::u64>(kind));
    global_id = mix_key_field(global_id, schema);
    global_id = mix_key_fingerprint(global_id, payload);
    return QueryKey{
        kind,
        schema,
        payload,
        global_id,
    };
}

ModuleKey module_key_from_stable_id(const StableModuleId stable_module) noexcept
{
    const PackageKey package = package_key(std::span<const std::string_view>{});
    return ModuleKey{
        package,
        stable_module.path,
        stable_module.part_count,
        ModuleKind::source,
        stable_module.global_id,
    };
}

DefKey def_key_from_stable_id(const StableDefId stable_id, const DefNamespace name_space, const DefKind kind) noexcept
{
    return DefKey{
        module_key_from_stable_id(stable_id.module),
        stable_id.name,
        QUERY_DEF_KEY_STABLE_ID_PATH_COMPONENT_COUNT,
        name_space,
        kind,
        stable_id.disambiguator,
        stable_id.global_id,
    };
}

void append_stable_key(StableKeyWriter& writer, const PackageKey key)
{
    writer.write_u64(QUERY_PACKAGE_KEY_MARKER);
    writer.write_fingerprint(key.identity);
    writer.write_u64(key.global_id);
}

void append_stable_key(StableKeyWriter& writer, const FileKey key)
{
    writer.write_u64(QUERY_FILE_KEY_MARKER);
    append_stable_key(writer, key.package);
    writer.write_fingerprint(key.path);
    writer.write_fingerprint(key.virtual_buffer);
    writer.write_u8(static_cast<base::u8>(key.role));
    writer.write_u64(key.global_id);
}

void append_stable_key(StableKeyWriter& writer, const ModuleKey key)
{
    writer.write_u64(QUERY_MODULE_KEY_MARKER);
    append_stable_key(writer, key.package);
    writer.write_fingerprint(key.path);
    writer.write_u32(key.path_component_count);
    writer.write_u8(static_cast<base::u8>(key.kind));
    writer.write_u64(key.global_id);
}

void append_stable_key(StableKeyWriter& writer, const ModulePartKey key)
{
    writer.write_u64(QUERY_MODULE_PART_KEY_MARKER);
    append_stable_key(writer, key.module);
    append_stable_key(writer, key.file);
    writer.write_fingerprint(key.name);
    writer.write_u32(key.stable_index);
    writer.write_u8(static_cast<base::u8>(key.kind));
    writer.write_u64(key.global_id);
}

void append_stable_key(StableKeyWriter& writer, const DefKey key)
{
    writer.write_u64(QUERY_DEF_KEY_MARKER);
    append_stable_key(writer, key.module);
    writer.write_fingerprint(key.path);
    writer.write_u32(key.path_component_count);
    writer.write_u8(static_cast<base::u8>(key.name_space));
    writer.write_u8(static_cast<base::u8>(key.kind));
    writer.write_u32(key.disambiguator);
    writer.write_u64(key.global_id);
}

void append_stable_key(StableKeyWriter& writer, const MemberKey key)
{
    writer.write_u64(QUERY_MEMBER_KEY_MARKER);
    append_stable_key(writer, key.owner);
    writer.write_fingerprint(key.name);
    writer.write_u8(static_cast<base::u8>(key.kind));
    writer.write_u32(key.ordinal);
    writer.write_u64(key.global_id);
}

void append_stable_key(StableKeyWriter& writer, const BodyKey key)
{
    writer.write_u64(QUERY_BODY_KEY_MARKER);
    append_stable_key(writer, key.owner);
    writer.write_u8(static_cast<base::u8>(key.slot));
    writer.write_u32(key.ordinal);
    writer.write_u64(key.global_id);
}

void append_stable_key(StableKeyWriter& writer, const GenericParamKey key)
{
    writer.write_u64(QUERY_GENERIC_PARAM_KEY_MARKER);
    append_stable_key(writer, key.owner);
    writer.write_u32(key.index);
    writer.write_u8(static_cast<base::u8>(key.kind));
    writer.write_u64(key.global_id);
}

void append_stable_key(StableKeyWriter& writer, const QueryKey key)
{
    writer.write_u64(QUERY_QUERY_KEY_MARKER);
    writer.write_u8(static_cast<base::u8>(key.kind));
    writer.write_u16(key.schema);
    writer.write_fingerprint(key.payload);
    writer.write_u64(key.global_id);
}

std::string stable_serialize(const PackageKey key)
{
    return serialize_with<PackageKey>(append_stable_key, key);
}

std::string stable_serialize(const FileKey key)
{
    return serialize_with<FileKey>(append_stable_key, key);
}

std::string stable_serialize(const ModuleKey key)
{
    return serialize_with<ModuleKey>(append_stable_key, key);
}

std::string stable_serialize(const ModulePartKey key)
{
    return serialize_with<ModulePartKey>(append_stable_key, key);
}

std::string stable_serialize(const DefKey key)
{
    return serialize_with<DefKey>(append_stable_key, key);
}

std::string stable_serialize(const MemberKey key)
{
    return serialize_with<MemberKey>(append_stable_key, key);
}

std::string stable_serialize(const BodyKey key)
{
    return serialize_with<BodyKey>(append_stable_key, key);
}

std::string stable_serialize(const GenericParamKey key)
{
    return serialize_with<GenericParamKey>(append_stable_key, key);
}

std::string stable_serialize(const QueryKey key)
{
    return serialize_with<QueryKey>(append_stable_key, key);
}

StableFingerprint128 stable_key_fingerprint(const PackageKey key)
{
    return fingerprint_with<PackageKey>(append_stable_key, key);
}

StableFingerprint128 stable_key_fingerprint(const FileKey key)
{
    return fingerprint_with<FileKey>(append_stable_key, key);
}

StableFingerprint128 stable_key_fingerprint(const ModuleKey key)
{
    return fingerprint_with<ModuleKey>(append_stable_key, key);
}

StableFingerprint128 stable_key_fingerprint(const ModulePartKey key)
{
    return fingerprint_with<ModulePartKey>(append_stable_key, key);
}

StableFingerprint128 stable_key_fingerprint(const DefKey key)
{
    return fingerprint_with<DefKey>(append_stable_key, key);
}

StableFingerprint128 stable_key_fingerprint(const MemberKey key)
{
    return fingerprint_with<MemberKey>(append_stable_key, key);
}

StableFingerprint128 stable_key_fingerprint(const BodyKey key)
{
    return fingerprint_with<BodyKey>(append_stable_key, key);
}

StableFingerprint128 stable_key_fingerprint(const GenericParamKey key)
{
    return fingerprint_with<GenericParamKey>(append_stable_key, key);
}

StableFingerprint128 stable_key_fingerprint(const QueryKey key)
{
    return fingerprint_with<QueryKey>(append_stable_key, key);
}

std::string debug_string(const PackageKey key)
{
    return finish_debug_string("PackageKey", key.global_id, key.identity);
}

std::string debug_string(const FileKey key)
{
    return finish_debug_string("FileKey", key.global_id, key.path);
}

std::string debug_string(const ModuleKey key)
{
    return finish_debug_string("ModuleKey", key.global_id, key.path);
}

std::string debug_string(const ModulePartKey key)
{
    return finish_debug_string("ModulePartKey", key.global_id, key.name);
}

std::string debug_string(const DefKey key)
{
    return finish_debug_string("DefKey", key.global_id, key.path);
}

std::string debug_string(const MemberKey key)
{
    return finish_debug_string("MemberKey", key.global_id, key.name);
}

std::string debug_string(const BodyKey key)
{
    return finish_debug_string("BodyKey", key.global_id, stable_key_fingerprint(key));
}

std::string debug_string(const GenericParamKey key)
{
    return finish_debug_string("GenericParamKey", key.global_id, stable_key_fingerprint(key));
}

std::string debug_string(const QueryKey key)
{
    return finish_debug_string("QueryKey", key.global_id, key.payload);
}

std::size_t PackageKeyHash::operator()(const PackageKey key) const
{
    return stable_hash_value(stable_key_fingerprint(key));
}

std::size_t FileKeyHash::operator()(const FileKey key) const
{
    return stable_hash_value(stable_key_fingerprint(key));
}

std::size_t ModuleKeyHash::operator()(const ModuleKey key) const
{
    return stable_hash_value(stable_key_fingerprint(key));
}

std::size_t DefKeyHash::operator()(const DefKey key) const
{
    return stable_hash_value(stable_key_fingerprint(key));
}

std::size_t QueryKeyHash::operator()(const QueryKey key) const
{
    return stable_hash_value(stable_key_fingerprint(key));
}

} // namespace aurex::query
