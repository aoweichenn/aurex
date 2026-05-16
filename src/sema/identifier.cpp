#include <aurex/sema/identifier.hpp>

namespace aurex::sema {
namespace {

constexpr base::u64 SEMA_LOOKUP_KEY_U32_SHIFT = 32;
constexpr std::size_t SEMA_LOOKUP_HASH_OWNER_SHIFT = 1;
constexpr base::u64 SEMA_GENERIC_PARAM_IDENTITY_HASH_SHIFT = 32;
constexpr base::u64 SEMA_STABLE_HASH_OFFSET = 14695981039346656037ULL;
constexpr base::u64 SEMA_STABLE_HASH_SECONDARY_OFFSET = 1099511628211ULL;
constexpr base::u64 SEMA_STABLE_HASH_PRIME = 1099511628211ULL;
constexpr base::u64 SEMA_STABLE_HASH_SECONDARY_PRIME = 14029467366897019727ULL;
constexpr base::u64 SEMA_STABLE_HASH_MIX_INCREMENT = 0x9e3779b97f4a7c15ULL;
constexpr base::u64 SEMA_STABLE_MODULE_MARKER = 0x4d4f44554c455f49ULL;
constexpr base::u64 SEMA_STABLE_DEF_MARKER = 0x4445465f49445f31ULL;
constexpr base::u64 SEMA_STABLE_MEMBER_MARKER = 0x4d454d42455231ULL;
constexpr base::u64 SEMA_STABLE_INCREMENTAL_MARKER = 0x494e43525f4b4559ULL;
constexpr unsigned SEMA_STABLE_HASH_MIX_LEFT_SHIFT = 6;
constexpr unsigned SEMA_STABLE_HASH_MIX_RIGHT_SHIFT = 2;

[[nodiscard]] base::u64 pack_lookup_key_parts(
    const base::u32 high,
    const base::u32 low
) noexcept {
    return (static_cast<base::u64>(high) << SEMA_LOOKUP_KEY_U32_SHIFT) |
        static_cast<base::u64>(low);
}

[[nodiscard]] base::u64 stable_mix(const base::u64 seed, const base::u64 value) noexcept {
    return seed ^ (value + SEMA_STABLE_HASH_MIX_INCREMENT +
                   (seed << SEMA_STABLE_HASH_MIX_LEFT_SHIFT) +
                   (seed >> SEMA_STABLE_HASH_MIX_RIGHT_SHIFT));
}

void mix_byte(StableFingerprint128& fingerprint, const unsigned char byte) noexcept {
    fingerprint.primary ^= static_cast<base::u64>(byte);
    fingerprint.primary *= SEMA_STABLE_HASH_PRIME;
    fingerprint.secondary ^= static_cast<base::u64>(byte) + SEMA_STABLE_HASH_MIX_INCREMENT;
    fingerprint.secondary *= SEMA_STABLE_HASH_SECONDARY_PRIME;
    fingerprint.byte_count += 1;
}

void mix_separator(StableFingerprint128& fingerprint, const base::u64 separator) noexcept {
    for (base::usize shift = 0; shift < sizeof(base::u64); ++shift) {
        mix_byte(fingerprint, static_cast<unsigned char>((separator >> (shift * 8U)) & 0xffU));
    }
}

} // namespace

std::size_t ModuleLookupKeyHash::operator()(const ModuleLookupKey key) const noexcept {
    return std::hash<base::u64> {}(pack_lookup_key_parts(key.module, key.name.value));
}

std::size_t IdentIdHash::operator()(const IdentId id) const noexcept {
    return std::hash<base::u32> {}(id.value);
}

std::size_t GenericParamIdentityHash::operator()(const GenericParamIdentity identity) const noexcept {
    return static_cast<std::size_t>(
        identity.value ^ (identity.value >> SEMA_GENERIC_PARAM_IDENTITY_HASH_SHIFT)
    );
}

StableFingerprint128 stable_fingerprint(const std::string_view text) noexcept {
    StableFingerprint128 fingerprint {
        syntax::stable_hash_text(text).value,
        SEMA_STABLE_HASH_SECONDARY_OFFSET,
        0,
    };
    for (const unsigned char byte : text) {
        fingerprint.secondary ^= static_cast<base::u64>(byte) + SEMA_STABLE_HASH_MIX_INCREMENT;
        fingerprint.secondary *= SEMA_STABLE_HASH_SECONDARY_PRIME;
        fingerprint.byte_count += 1;
    }
    if (fingerprint.secondary == 0) {
        fingerprint.secondary = SEMA_STABLE_HASH_SECONDARY_OFFSET;
    }
    return fingerprint;
}

StableFingerprint128 stable_fingerprint(const std::span<const std::string_view> parts) noexcept {
    StableFingerprint128 fingerprint {
        SEMA_STABLE_HASH_OFFSET,
        SEMA_STABLE_HASH_SECONDARY_OFFSET,
        0,
    };
    mix_separator(fingerprint, SEMA_STABLE_MODULE_MARKER);
    for (base::usize index = 0; index < parts.size(); ++index) {
        mix_separator(fingerprint, index);
        for (const unsigned char byte : parts[index]) {
            mix_byte(fingerprint, byte);
        }
    }
    if (fingerprint.primary == 0) {
        fingerprint.primary = SEMA_STABLE_HASH_OFFSET;
    }
    if (fingerprint.secondary == 0) {
        fingerprint.secondary = SEMA_STABLE_HASH_SECONDARY_OFFSET;
    }
    return fingerprint;
}

StableModuleId stable_module_id(const std::span<const std::string_view> module_path) noexcept {
    const StableFingerprint128 path = stable_fingerprint(module_path);
    const base::u64 global_id = stable_mix(
        stable_mix(path.primary, path.secondary),
        static_cast<base::u64>(module_path.size())
    );
    return StableModuleId {
        path,
        static_cast<base::u32>(module_path.size()),
        global_id == 0 ? SEMA_STABLE_MODULE_MARKER : global_id,
    };
}

StableDefId stable_definition_id(
    const StableModuleId module,
    const StableSymbolKind kind,
    const std::string_view name,
    const base::u32 disambiguator
) noexcept {
    const StableFingerprint128 name_fingerprint = stable_fingerprint(name);
    base::u64 global_id = stable_mix(SEMA_STABLE_DEF_MARKER, module.global_id);
    global_id = stable_mix(global_id, static_cast<base::u64>(kind));
    global_id = stable_mix(global_id, name_fingerprint.primary);
    global_id = stable_mix(global_id, name_fingerprint.secondary);
    global_id = stable_mix(global_id, disambiguator);
    return StableDefId {
        module,
        name_fingerprint,
        global_id == 0 ? SEMA_STABLE_DEF_MARKER : global_id,
        disambiguator,
        kind,
    };
}

StableMemberKey stable_member_key(
    const StableDefId owner,
    const StableSymbolKind kind,
    const std::string_view member_name,
    const base::u32 disambiguator
) noexcept {
    const StableFingerprint128 member_fingerprint = stable_fingerprint(member_name);
    base::u64 global_id = stable_mix(SEMA_STABLE_MEMBER_MARKER, owner.global_id);
    global_id = stable_mix(global_id, static_cast<base::u64>(kind));
    global_id = stable_mix(global_id, member_fingerprint.primary);
    global_id = stable_mix(global_id, member_fingerprint.secondary);
    global_id = stable_mix(global_id, disambiguator);
    return StableMemberKey {
        owner,
        member_fingerprint,
        global_id == 0 ? SEMA_STABLE_MEMBER_MARKER : global_id,
        disambiguator,
        kind,
    };
}

IncrementalKey stable_incremental_key(
    const StableDefId definition,
    const std::string_view semantic_fingerprint
) noexcept {
    const StableFingerprint128 fingerprint = stable_fingerprint(semantic_fingerprint);
    base::u64 global_id = stable_mix(SEMA_STABLE_INCREMENTAL_MARKER, definition.global_id);
    global_id = stable_mix(global_id, fingerprint.primary);
    global_id = stable_mix(global_id, fingerprint.secondary);
    return IncrementalKey {
        definition,
        fingerprint,
        global_id == 0 ? SEMA_STABLE_INCREMENTAL_MARKER : global_id,
    };
}

std::size_t MethodLookupKeyHash::operator()(const MethodLookupKey key) const noexcept {
    const std::size_t module_name_hash =
        std::hash<base::u64> {}(pack_lookup_key_parts(key.module, key.name.value));
    const std::size_t owner_hash = std::hash<base::u32> {}(key.owner_type);
    return module_name_hash ^ (owner_hash << SEMA_LOOKUP_HASH_OWNER_SHIFT);
}

std::size_t FunctionLookupKeyHash::operator()(const FunctionLookupKey key) const noexcept {
    const std::size_t module_name_hash =
        std::hash<base::u64> {}(pack_lookup_key_parts(key.module, key.name.value));
    const std::size_t owner_hash = std::hash<base::u32> {}(key.owner_type);
    return module_name_hash ^ (owner_hash << SEMA_LOOKUP_HASH_OWNER_SHIFT);
}

std::size_t EnumCaseLookupKeyHash::operator()(const EnumCaseLookupKey key) const noexcept {
    return std::hash<base::u64> {}(pack_lookup_key_parts(key.enum_type, key.case_name.value));
}

} // namespace aurex::sema
