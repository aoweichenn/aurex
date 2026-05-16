#include <aurex/sema/identifier.hpp>

namespace aurex::sema {
namespace {

constexpr base::u64 SEMA_LOOKUP_KEY_U32_SHIFT = 32;
constexpr std::size_t SEMA_LOOKUP_HASH_OWNER_SHIFT = 1;

[[nodiscard]] base::u64 pack_lookup_key_parts(
    const base::u32 high,
    const base::u32 low
) noexcept {
    return (static_cast<base::u64>(high) << SEMA_LOOKUP_KEY_U32_SHIFT) |
        static_cast<base::u64>(low);
}

} // namespace

std::size_t ModuleLookupKeyHash::operator()(const ModuleLookupKey key) const noexcept {
    return std::hash<base::u64> {}(pack_lookup_key_parts(key.module, key.name.value));
}

std::size_t IdentIdHash::operator()(const IdentId id) const noexcept {
    return std::hash<base::u32> {}(id.value);
}

std::size_t MethodLookupKeyHash::operator()(const MethodLookupKey key) const noexcept {
    const std::size_t module_name_hash =
        std::hash<base::u64> {}(pack_lookup_key_parts(key.module, key.name.value));
    const std::size_t owner_hash = std::hash<base::u32> {}(key.owner_type);
    return module_name_hash ^ (owner_hash << SEMA_LOOKUP_HASH_OWNER_SHIFT);
}

std::size_t EnumCaseLookupKeyHash::operator()(const EnumCaseLookupKey key) const noexcept {
    return std::hash<base::u64> {}(pack_lookup_key_parts(key.enum_type, key.case_name.value));
}

} // namespace aurex::sema
