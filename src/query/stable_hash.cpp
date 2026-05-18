#include <aurex/query/stable_hash.hpp>

#include <iomanip>
#include <sstream>

namespace aurex::query {
namespace {

constexpr base::u64 QUERY_STABLE_HASH_OFFSET = 14695981039346656037ULL;
constexpr base::u64 QUERY_STABLE_HASH_SECONDARY_OFFSET = 1099511628211ULL;
constexpr base::u64 QUERY_STABLE_HASH_PRIME = 1099511628211ULL;
constexpr base::u64 QUERY_STABLE_HASH_SECONDARY_PRIME = 14029467366897019727ULL;
constexpr base::u64 QUERY_STABLE_HASH_MIX_INCREMENT = 0x9e3779b97f4a7c15ULL;
constexpr base::u64 QUERY_STABLE_PARTS_MARKER = 0x5155455259504152ULL;
constexpr base::u64 QUERY_STABLE_STRING_MARKER = 0x5155455259535452ULL;
constexpr base::u64 QUERY_STABLE_FINGERPRINT_MARKER = 0x5155455259465054ULL;
constexpr base::usize QUERY_STABLE_U16_BYTES = sizeof(base::u16);
constexpr base::usize QUERY_STABLE_U32_BYTES = sizeof(base::u32);
constexpr base::usize QUERY_STABLE_U64_BYTES = sizeof(base::u64);
constexpr unsigned QUERY_STABLE_HASH_MIX_LEFT_SHIFT = 6;
constexpr unsigned QUERY_STABLE_HASH_MIX_RIGHT_SHIFT = 2;
constexpr unsigned QUERY_STABLE_BYTE_SHIFT = 8;
constexpr base::u64 QUERY_STABLE_BYTE_MASK = 0xffU;
constexpr base::u32 QUERY_STABLE_MAX_BYTE_COUNT = 0xffffffffU;
constexpr int QUERY_FINGERPRINT_HEX_WIDTH = 16;
constexpr char QUERY_FINGERPRINT_HEX_FILL = '0';

} // namespace

base::u64 stable_mix(const base::u64 seed, const base::u64 value) noexcept
{
    return seed
        ^ (value + QUERY_STABLE_HASH_MIX_INCREMENT + (seed << QUERY_STABLE_HASH_MIX_LEFT_SHIFT)
            + (seed >> QUERY_STABLE_HASH_MIX_RIGHT_SHIFT));
}

StableHashBuilder::StableHashBuilder() noexcept
    : fingerprint_{
          QUERY_STABLE_HASH_OFFSET,
          QUERY_STABLE_HASH_SECONDARY_OFFSET,
          0,
      }
{
}

void StableHashBuilder::mix_u8(const base::u8 value) noexcept
{
    this->mix_raw_byte(value);
}

void StableHashBuilder::mix_u16(const base::u16 value) noexcept
{
    for (base::usize index = 0; index < QUERY_STABLE_U16_BYTES; ++index) {
        this->mix_raw_byte(static_cast<base::u8>(
            (static_cast<base::u64>(value) >> (index * QUERY_STABLE_BYTE_SHIFT)) & QUERY_STABLE_BYTE_MASK));
    }
}

void StableHashBuilder::mix_u32(const base::u32 value) noexcept
{
    for (base::usize index = 0; index < QUERY_STABLE_U32_BYTES; ++index) {
        this->mix_raw_byte(static_cast<base::u8>(
            (static_cast<base::u64>(value) >> (index * QUERY_STABLE_BYTE_SHIFT)) & QUERY_STABLE_BYTE_MASK));
    }
}

void StableHashBuilder::mix_u64(const base::u64 value) noexcept
{
    for (base::usize index = 0; index < QUERY_STABLE_U64_BYTES; ++index) {
        this->mix_raw_byte(
            static_cast<base::u8>((value >> (index * QUERY_STABLE_BYTE_SHIFT)) & QUERY_STABLE_BYTE_MASK));
    }
}

void StableHashBuilder::mix_bool(const bool value) noexcept
{
    this->mix_u8(value ? base::u8{1} : base::u8{0});
}

void StableHashBuilder::mix_bytes(const std::string_view bytes) noexcept
{
    for (const unsigned char byte : bytes) {
        this->mix_raw_byte(static_cast<base::u8>(byte));
    }
}

void StableHashBuilder::mix_string(const std::string_view value) noexcept
{
    this->mix_u64(QUERY_STABLE_STRING_MARKER);
    this->mix_u64(static_cast<base::u64>(value.size()));
    this->mix_bytes(value);
}

void StableHashBuilder::mix_fingerprint(const StableFingerprint128 fingerprint) noexcept
{
    this->mix_u64(QUERY_STABLE_FINGERPRINT_MARKER);
    this->mix_u64(fingerprint.primary);
    this->mix_u64(fingerprint.secondary);
    this->mix_u32(fingerprint.byte_count);
}

StableFingerprint128 StableHashBuilder::finish() const noexcept
{
    StableFingerprint128 result = this->fingerprint_;
    if (result.primary == 0) {
        result.primary = QUERY_STABLE_HASH_OFFSET;
    }
    if (result.secondary == 0) {
        result.secondary = QUERY_STABLE_HASH_SECONDARY_OFFSET;
    }
    return result;
}

void StableHashBuilder::mix_raw_byte(const base::u8 value) noexcept
{
    this->fingerprint_.primary ^= static_cast<base::u64>(value);
    this->fingerprint_.primary *= QUERY_STABLE_HASH_PRIME;
    this->fingerprint_.secondary ^= static_cast<base::u64>(value) + QUERY_STABLE_HASH_MIX_INCREMENT;
    this->fingerprint_.secondary *= QUERY_STABLE_HASH_SECONDARY_PRIME;
    if (this->fingerprint_.byte_count != QUERY_STABLE_MAX_BYTE_COUNT) {
        this->fingerprint_.byte_count += 1;
    }
}

StableFingerprint128 stable_fingerprint(const std::string_view text) noexcept
{
    StableHashBuilder builder;
    builder.mix_bytes(text);
    return builder.finish();
}

StableFingerprint128 stable_fingerprint(const std::span<const std::string_view> parts) noexcept
{
    StableHashBuilder builder;
    builder.mix_u64(QUERY_STABLE_PARTS_MARKER);
    builder.mix_u64(static_cast<base::u64>(parts.size()));
    for (base::usize index = 0; index < parts.size(); ++index) {
        builder.mix_u64(static_cast<base::u64>(index));
        builder.mix_string(parts[index]);
    }
    return builder.finish();
}

std::size_t stable_hash_value(const StableFingerprint128 fingerprint) noexcept
{
    base::u64 hash = stable_mix(fingerprint.primary, fingerprint.secondary);
    hash = stable_mix(hash, fingerprint.byte_count);
    return static_cast<std::size_t>(hash ^ (hash >> QUERY_STABLE_HASH_MIX_RIGHT_SHIFT));
}

std::string debug_string(const StableFingerprint128 fingerprint)
{
    std::ostringstream out;
    out << std::hex << std::setfill(QUERY_FINGERPRINT_HEX_FILL) << std::setw(QUERY_FINGERPRINT_HEX_WIDTH)
        << fingerprint.primary << ':' << std::setw(QUERY_FINGERPRINT_HEX_WIDTH) << fingerprint.secondary << std::dec
        << ':' << fingerprint.byte_count;
    return out.str();
}

StableKeyWriter::StableKeyWriter() : bytes_(), hash_()
{
}

void StableKeyWriter::write_u8(const base::u8 value)
{
    this->write_raw_byte(value);
}

void StableKeyWriter::write_u16(const base::u16 value)
{
    for (base::usize index = 0; index < QUERY_STABLE_U16_BYTES; ++index) {
        this->write_raw_byte(static_cast<base::u8>(
            (static_cast<base::u64>(value) >> (index * QUERY_STABLE_BYTE_SHIFT)) & QUERY_STABLE_BYTE_MASK));
    }
}

void StableKeyWriter::write_u32(const base::u32 value)
{
    for (base::usize index = 0; index < QUERY_STABLE_U32_BYTES; ++index) {
        this->write_raw_byte(static_cast<base::u8>(
            (static_cast<base::u64>(value) >> (index * QUERY_STABLE_BYTE_SHIFT)) & QUERY_STABLE_BYTE_MASK));
    }
}

void StableKeyWriter::write_u64(const base::u64 value)
{
    for (base::usize index = 0; index < QUERY_STABLE_U64_BYTES; ++index) {
        this->write_raw_byte(
            static_cast<base::u8>((value >> (index * QUERY_STABLE_BYTE_SHIFT)) & QUERY_STABLE_BYTE_MASK));
    }
}

void StableKeyWriter::write_bool(const bool value)
{
    this->write_u8(value ? base::u8{1} : base::u8{0});
}

void StableKeyWriter::write_string(const std::string_view value)
{
    this->write_u64(static_cast<base::u64>(value.size()));
    for (const unsigned char byte : value) {
        this->write_raw_byte(static_cast<base::u8>(byte));
    }
}

void StableKeyWriter::write_fingerprint(const StableFingerprint128 fingerprint)
{
    this->write_u64(fingerprint.primary);
    this->write_u64(fingerprint.secondary);
    this->write_u32(fingerprint.byte_count);
}

std::string_view StableKeyWriter::bytes() const noexcept
{
    return this->bytes_;
}

const std::string& StableKeyWriter::storage() const noexcept
{
    return this->bytes_;
}

StableFingerprint128 StableKeyWriter::fingerprint() const noexcept
{
    return this->hash_.finish();
}

void StableKeyWriter::write_raw_byte(const base::u8 value)
{
    this->bytes_.push_back(static_cast<char>(value));
    this->hash_.mix_u8(value);
}

std::size_t StableFingerprintHash::operator()(const StableFingerprint128 fingerprint) const noexcept
{
    return stable_hash_value(fingerprint);
}

} // namespace aurex::query
