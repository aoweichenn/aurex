#include <aurex/query/canonical_type_key.hpp>
#include <aurex/query/generic_instance_key.hpp>
#include <aurex/query/stable_key_decoder.hpp>

#include <limits>

namespace aurex::query {
namespace {

constexpr base::u64 QUERY_PACKAGE_KEY_MARKER = 0x51504b4759313031ULL;
constexpr base::u64 QUERY_PROJECT_KEY_MARKER = 0x5150524f4a303031ULL;
constexpr base::u64 QUERY_FILE_KEY_MARKER = 0x5146494c45593031ULL;
constexpr base::u64 QUERY_LEX_CONFIG_KEY_MARKER = 0x514c584346473031ULL;
constexpr base::u64 QUERY_PARSER_CONFIG_KEY_MARKER = 0x5150434647303031ULL;
constexpr base::u64 QUERY_LEX_FILE_KEY_MARKER = 0x514c5846494c4531ULL;
constexpr base::u64 QUERY_PARSE_FILE_KEY_MARKER = 0x5150525346494c45ULL;
constexpr base::u64 QUERY_MODULE_KEY_MARKER = 0x514d4f4459303031ULL;
constexpr base::u64 QUERY_MODULE_PART_KEY_MARKER = 0x514d504152543031ULL;
constexpr base::u64 QUERY_DEF_KEY_MARKER = 0x514445464b455931ULL;
constexpr base::u64 QUERY_MEMBER_KEY_MARKER = 0x514d454d4b455931ULL;
constexpr base::u64 QUERY_BODY_KEY_MARKER = 0x51424f4459303031ULL;
constexpr base::u64 QUERY_GENERIC_PARAM_KEY_MARKER = 0x51475041524d3031ULL;
constexpr base::u64 QUERY_QUERY_KEY_MARKER = 0x51554552594b3031ULL;
constexpr base::u64 QUERY_CANONICAL_TYPE_KEY_MARKER = 0x5143545950453031ULL;
constexpr base::u64 QUERY_PARAM_ENV_KEY_MARKER = 0x5150454e56303131ULL;
constexpr base::u64 QUERY_GENERIC_INSTANCE_KEY_MARKER = 0x5147494e53543031ULL;
constexpr base::usize QUERY_STABLE_U8_BYTES = sizeof(base::u8);
constexpr base::usize QUERY_STABLE_U16_BYTES = sizeof(base::u16);
constexpr base::usize QUERY_STABLE_U32_BYTES = sizeof(base::u32);
constexpr base::usize QUERY_STABLE_U64_BYTES = sizeof(base::u64);
constexpr base::usize QUERY_STABLE_FINGERPRINT_BYTES =
    QUERY_STABLE_U64_BYTES + QUERY_STABLE_U64_BYTES + QUERY_STABLE_U32_BYTES;
constexpr base::usize QUERY_STABLE_MIN_CANONICAL_TYPE_KEY_BYTES =
    QUERY_STABLE_U64_BYTES + QUERY_STABLE_U8_BYTES + QUERY_STABLE_U64_BYTES;
constexpr unsigned QUERY_STABLE_BYTE_SHIFT = 8;
constexpr base::u8 QUERY_STABLE_FALSE_BYTE = 0;
constexpr base::u8 QUERY_STABLE_TRUE_BYTE = 1;

class StableKeyReader final {
public:
    explicit StableKeyReader(const std::string_view bytes) noexcept : bytes_(bytes), offset_(0)
    {
    }

    [[nodiscard]] bool eof() const noexcept
    {
        return this->offset_ == this->bytes_.size();
    }

    [[nodiscard]] base::usize offset() const noexcept
    {
        return this->offset_;
    }

    [[nodiscard]] base::usize remaining() const noexcept
    {
        return this->bytes_.size() - this->offset_;
    }

    [[nodiscard]] std::string_view slice_from(const base::usize start) const noexcept
    {
        return this->bytes_.substr(start, this->offset_ - start);
    }

    [[nodiscard]] bool skip(const base::usize width) noexcept
    {
        if (width > this->remaining()) {
            return false;
        }
        this->offset_ += width;
        return true;
    }

    [[nodiscard]] bool read_u8(base::u8& value) noexcept
    {
        value = 0;
        if (!this->can_read(QUERY_STABLE_U8_BYTES)) {
            return false;
        }
        value = static_cast<base::u8>(static_cast<unsigned char>(this->bytes_[this->offset_]));
        this->offset_ += QUERY_STABLE_U8_BYTES;
        return true;
    }

    [[nodiscard]] bool read_u32(base::u32& value) noexcept
    {
        base::u64 raw = 0;
        if (!this->read_unsigned(QUERY_STABLE_U32_BYTES, raw)) {
            value = 0;
            return false;
        }
        value = static_cast<base::u32>(raw);
        return true;
    }

    [[nodiscard]] bool read_u64(base::u64& value) noexcept
    {
        return this->read_unsigned(QUERY_STABLE_U64_BYTES, value);
    }

private:
    [[nodiscard]] bool can_read(const base::usize width) const noexcept
    {
        return width <= this->remaining();
    }

    [[nodiscard]] bool read_unsigned(const base::usize width, base::u64& value) noexcept
    {
        value = 0;
        if (!this->can_read(width)) {
            return false;
        }
        for (base::usize index = 0; index < width; ++index) {
            const base::u64 byte =
                static_cast<base::u64>(static_cast<unsigned char>(this->bytes_[this->offset_ + index]));
            value |= byte << (index * QUERY_STABLE_BYTE_SHIFT);
        }
        this->offset_ += width;
        return true;
    }

    std::string_view bytes_;
    base::usize offset_;
};

[[nodiscard]] bool read_marker(StableKeyReader& reader, const base::u64 expected_marker) noexcept
{
    base::u64 marker = 0;
    return reader.read_u64(marker) && marker == expected_marker;
}

[[nodiscard]] bool skip_fingerprint(StableKeyReader& reader) noexcept
{
    return reader.skip(QUERY_STABLE_FINGERPRINT_BYTES);
}

[[nodiscard]] bool skip_fingerprints(StableKeyReader& reader, const base::u64 count) noexcept
{
    const base::u64 available_count = static_cast<base::u64>(reader.remaining() / QUERY_STABLE_FINGERPRINT_BYTES);
    if (count > available_count) {
        return false;
    }
    return reader.skip(static_cast<base::usize>(count) * QUERY_STABLE_FINGERPRINT_BYTES);
}

[[nodiscard]] bool read_nonzero_u64(StableKeyReader& reader) noexcept
{
    base::u64 value = 0;
    return reader.read_u64(value) && value != 0;
}

[[nodiscard]] bool read_bool_value(StableKeyReader& reader) noexcept
{
    base::u8 value = 0;
    return reader.read_u8(value) && (value == QUERY_STABLE_FALSE_BYTE || value == QUERY_STABLE_TRUE_BYTE);
}

[[nodiscard]] bool read_enum_value(StableKeyReader& reader, const base::u8 minimum, const base::u8 maximum) noexcept
{
    base::u8 value = 0;
    return reader.read_u8(value) && minimum <= value && value <= maximum;
}

template <typename Enum>
[[nodiscard]] constexpr base::u8 enum_byte(const Enum value) noexcept
{
    return static_cast<base::u8>(value);
}

[[nodiscard]] bool read_source_role(StableKeyReader& reader) noexcept
{
    return read_enum_value(reader, enum_byte(SourceRole::source), enum_byte(SourceRole::generated));
}

[[nodiscard]] bool read_module_kind(StableKeyReader& reader) noexcept
{
    return read_enum_value(reader, enum_byte(ModuleKind::source), enum_byte(ModuleKind::synthetic));
}

[[nodiscard]] bool read_module_part_kind(StableKeyReader& reader) noexcept
{
    return read_enum_value(reader, enum_byte(ModulePartKind::primary), enum_byte(ModulePartKind::generated));
}

[[nodiscard]] bool read_def_namespace(StableKeyReader& reader) noexcept
{
    return read_enum_value(reader, enum_byte(DefNamespace::value), enum_byte(DefNamespace::synthetic));
}

[[nodiscard]] bool read_def_kind(StableKeyReader& reader) noexcept
{
    return read_enum_value(reader, enum_byte(DefKind::function), enum_byte(DefKind::synthetic));
}

[[nodiscard]] bool read_member_kind(StableKeyReader& reader) noexcept
{
    return read_enum_value(reader, enum_byte(MemberKind::struct_field), enum_byte(MemberKind::synthetic));
}

[[nodiscard]] bool read_body_slot_kind(StableKeyReader& reader) noexcept
{
    return read_enum_value(reader, enum_byte(BodySlotKind::function_body), enum_byte(BodySlotKind::closure_body));
}

[[nodiscard]] bool read_generic_param_kind(StableKeyReader& reader) noexcept
{
    return read_enum_value(reader, enum_byte(GenericParamKind::type), enum_byte(GenericParamKind::lifetime));
}

[[nodiscard]] bool read_query_kind(StableKeyReader& reader) noexcept
{
    return read_enum_value(reader, enum_byte(QueryKind::project_graph), enum_byte(QueryKind::module_part));
}

[[nodiscard]] bool read_builtin_type_kind(StableKeyReader& reader) noexcept
{
    return read_enum_value(reader, enum_byte(BuiltinTypeKey::void_), enum_byte(BuiltinTypeKey::char_));
}

[[nodiscard]] bool read_pointer_mutability(StableKeyReader& reader) noexcept
{
    return read_enum_value(reader, enum_byte(PointerMutabilityKey::mut), enum_byte(PointerMutabilityKey::const_));
}

[[nodiscard]] bool read_function_call_conv(StableKeyReader& reader) noexcept
{
    return read_enum_value(reader, enum_byte(FunctionCallConvKey::aurex), enum_byte(FunctionCallConvKey::c));
}

[[nodiscard]] bool skip_package_key(StableKeyReader& reader) noexcept;
[[nodiscard]] bool skip_project_key(StableKeyReader& reader) noexcept;
[[nodiscard]] bool skip_file_key(StableKeyReader& reader) noexcept;
[[nodiscard]] bool skip_lex_config_key(StableKeyReader& reader) noexcept;
[[nodiscard]] bool skip_module_key(StableKeyReader& reader) noexcept;
[[nodiscard]] bool skip_module_part_key(StableKeyReader& reader) noexcept;
[[nodiscard]] bool skip_def_key(StableKeyReader& reader) noexcept;
[[nodiscard]] bool skip_member_key(StableKeyReader& reader) noexcept;
[[nodiscard]] bool skip_body_key(StableKeyReader& reader) noexcept;
[[nodiscard]] bool skip_generic_param_key(StableKeyReader& reader) noexcept;
[[nodiscard]] bool skip_canonical_type_key(StableKeyReader& reader) noexcept;
[[nodiscard]] bool skip_param_env_key(StableKeyReader& reader) noexcept;
[[nodiscard]] bool skip_generic_instance_key(StableKeyReader& reader) noexcept;
[[nodiscard]] bool skip_query_key(StableKeyReader& reader) noexcept;

[[nodiscard]] std::optional<std::string_view> read_file_key_slice(StableKeyReader& reader) noexcept
{
    const base::usize start = reader.offset();
    if (!skip_file_key(reader)) {
        return std::nullopt;
    }
    return reader.slice_from(start);
}

[[nodiscard]] std::optional<std::string_view> read_lex_config_key_slice(StableKeyReader& reader) noexcept
{
    const base::usize start = reader.offset();
    if (!skip_lex_config_key(reader)) {
        return std::nullopt;
    }
    return reader.slice_from(start);
}

[[nodiscard]] std::optional<std::string_view> read_module_key_slice(StableKeyReader& reader) noexcept
{
    const base::usize start = reader.offset();
    if (!skip_module_key(reader)) {
        return std::nullopt;
    }
    return reader.slice_from(start);
}

[[nodiscard]] std::optional<std::string_view> read_module_part_key_module_slice(StableKeyReader& reader) noexcept
{
    if (!read_marker(reader, QUERY_MODULE_PART_KEY_MARKER)) {
        return std::nullopt;
    }
    return read_module_key_slice(reader);
}

[[nodiscard]] std::optional<std::string_view> read_def_key_slice(StableKeyReader& reader) noexcept
{
    const base::usize start = reader.offset();
    if (!skip_def_key(reader)) {
        return std::nullopt;
    }
    return reader.slice_from(start);
}

[[nodiscard]] std::optional<std::string_view> read_parser_config_key_lex_config_slice(StableKeyReader& reader) noexcept
{
    if (!read_marker(reader, QUERY_PARSER_CONFIG_KEY_MARKER)) {
        return std::nullopt;
    }
    std::optional<std::string_view> lex_config = read_lex_config_key_slice(reader);
    base::u32 schema = 0;
    if (!lex_config.has_value() || !reader.read_u32(schema) || schema != QUERY_PARSER_CONFIG_SCHEMA_VERSION
        || !read_bool_value(reader) || !read_bool_value(reader) || !read_nonzero_u64(reader)) {
        return std::nullopt;
    }
    return lex_config;
}

[[nodiscard]] bool skip_package_key(StableKeyReader& reader) noexcept
{
    return read_marker(reader, QUERY_PACKAGE_KEY_MARKER) && skip_fingerprint(reader) && read_nonzero_u64(reader);
}

[[nodiscard]] bool skip_project_key(StableKeyReader& reader) noexcept
{
    return read_marker(reader, QUERY_PROJECT_KEY_MARKER) && skip_fingerprint(reader) && read_nonzero_u64(reader);
}

[[nodiscard]] bool skip_file_key(StableKeyReader& reader) noexcept
{
    return read_marker(reader, QUERY_FILE_KEY_MARKER) && skip_package_key(reader) && skip_fingerprint(reader)
        && skip_fingerprint(reader) && read_source_role(reader) && read_nonzero_u64(reader);
}

[[nodiscard]] bool skip_lex_config_key(StableKeyReader& reader) noexcept
{
    base::u32 schema = 0;
    return read_marker(reader, QUERY_LEX_CONFIG_KEY_MARKER) && reader.read_u32(schema)
        && schema == QUERY_LEX_CONFIG_SCHEMA_VERSION && read_bool_value(reader) && read_nonzero_u64(reader);
}

[[nodiscard]] bool skip_module_key(StableKeyReader& reader) noexcept
{
    return read_marker(reader, QUERY_MODULE_KEY_MARKER) && skip_package_key(reader) && skip_fingerprint(reader)
        && reader.skip(QUERY_STABLE_U32_BYTES) && read_module_kind(reader) && read_nonzero_u64(reader);
}

[[nodiscard]] bool skip_module_part_key(StableKeyReader& reader) noexcept
{
    return read_marker(reader, QUERY_MODULE_PART_KEY_MARKER) && skip_module_key(reader) && skip_file_key(reader)
        && skip_fingerprint(reader) && reader.skip(QUERY_STABLE_U32_BYTES) && read_module_part_kind(reader)
        && read_nonzero_u64(reader);
}

[[nodiscard]] bool skip_def_key(StableKeyReader& reader) noexcept
{
    return read_marker(reader, QUERY_DEF_KEY_MARKER) && skip_module_key(reader) && skip_fingerprint(reader)
        && reader.skip(QUERY_STABLE_U32_BYTES) && read_def_namespace(reader) && read_def_kind(reader)
        && reader.skip(QUERY_STABLE_U32_BYTES) && read_nonzero_u64(reader);
}

[[nodiscard]] bool skip_member_key(StableKeyReader& reader) noexcept
{
    return read_marker(reader, QUERY_MEMBER_KEY_MARKER) && skip_def_key(reader) && skip_fingerprint(reader)
        && read_member_kind(reader) && reader.skip(QUERY_STABLE_U32_BYTES) && read_nonzero_u64(reader);
}

[[nodiscard]] bool skip_body_key(StableKeyReader& reader) noexcept
{
    return read_marker(reader, QUERY_BODY_KEY_MARKER) && skip_def_key(reader) && read_body_slot_kind(reader)
        && reader.skip(QUERY_STABLE_U32_BYTES) && read_nonzero_u64(reader);
}

[[nodiscard]] bool skip_generic_param_key(StableKeyReader& reader) noexcept
{
    return read_marker(reader, QUERY_GENERIC_PARAM_KEY_MARKER) && skip_def_key(reader)
        && reader.skip(QUERY_STABLE_U32_BYTES) && read_generic_param_kind(reader) && read_nonzero_u64(reader);
}

[[nodiscard]] bool read_canonical_type_kind(StableKeyReader& reader, CanonicalTypeKind& kind) noexcept
{
    base::u8 value = 0;
    if (!reader.read_u8(value) || value <= enum_byte(CanonicalTypeKind::invalid)
        || value > enum_byte(CanonicalTypeKind::trait_object)) {
        kind = CanonicalTypeKind::invalid;
        return false;
    }
    kind = static_cast<CanonicalTypeKind>(value);
    return true;
}

[[nodiscard]] bool canonical_type_child_count_is_expected(
    const CanonicalTypeKind kind, const base::u32 function_param_count, const base::u64 child_count) noexcept
{
    switch (kind) {
        case CanonicalTypeKind::builtin:
        case CanonicalTypeKind::generic_param:
        case CanonicalTypeKind::const_arg:
        case CanonicalTypeKind::trait_object:
            return child_count == 0;
        case CanonicalTypeKind::pointer:
        case CanonicalTypeKind::reference:
        case CanonicalTypeKind::array:
        case CanonicalTypeKind::slice:
        case CanonicalTypeKind::associated_type_projection:
            return child_count == 1;
        case CanonicalTypeKind::function:
            return child_count == static_cast<base::u64>(function_param_count) + 1;
        case CanonicalTypeKind::tuple:
        case CanonicalTypeKind::nominal:
            return true;
        case CanonicalTypeKind::invalid:
            return false;
    }
    return false;
}

[[nodiscard]] bool skip_canonical_type_header(StableKeyReader& reader, base::u64& child_count) noexcept
{
    if (!read_marker(reader, QUERY_CANONICAL_TYPE_KEY_MARKER)) {
        return false;
    }
    CanonicalTypeKind kind = CanonicalTypeKind::invalid;
    base::u32 function_param_count = 0;
    if (!read_canonical_type_kind(reader, kind)) {
        return false;
    }
    switch (kind) {
        case CanonicalTypeKind::builtin:
            if (!read_builtin_type_kind(reader)) {
                return false;
            }
            break;
        case CanonicalTypeKind::pointer:
        case CanonicalTypeKind::reference:
        case CanonicalTypeKind::slice:
            if (!read_pointer_mutability(reader)) {
                return false;
            }
            break;
        case CanonicalTypeKind::array: {
            if (!reader.skip(QUERY_STABLE_U64_BYTES)) {
                return false;
            }
            break;
        }
        case CanonicalTypeKind::function:
            if (!read_function_call_conv(reader) || !read_bool_value(reader) || !read_bool_value(reader)
                || !reader.read_u32(function_param_count)) {
                return false;
            }
            break;
        case CanonicalTypeKind::nominal:
            if (!skip_def_key(reader)) {
                return false;
            }
            break;
        case CanonicalTypeKind::generic_param:
            if (!skip_generic_param_key(reader)) {
                return false;
            }
            break;
        case CanonicalTypeKind::const_arg:
            if (!skip_fingerprint(reader)) {
                return false;
            }
            break;
        case CanonicalTypeKind::associated_type_projection:
            if (!skip_member_key(reader)) {
                return false;
            }
            break;
        case CanonicalTypeKind::trait_object:
        case CanonicalTypeKind::tuple:
            break;
        case CanonicalTypeKind::invalid:
            return false;
    }
    return reader.read_u64(child_count)
        && canonical_type_child_count_is_expected(kind, function_param_count, child_count);
}

[[nodiscard]] bool skip_canonical_type_key(StableKeyReader& reader) noexcept
{
    base::u64 pending_nodes = 1;
    while (pending_nodes != 0) {
        base::u64 child_count = 0;
        if (!skip_canonical_type_header(reader, child_count)) {
            return false;
        }
        if (child_count > std::numeric_limits<base::u64>::max() - (pending_nodes - 1)) {
            return false;
        }
        pending_nodes = pending_nodes - 1 + child_count;
    }
    return true;
}

[[nodiscard]] bool skip_canonical_type_keys(StableKeyReader& reader, const base::u64 count) noexcept
{
    const base::u64 maximum_root_count =
        static_cast<base::u64>(reader.remaining() / QUERY_STABLE_MIN_CANONICAL_TYPE_KEY_BYTES);
    if (count > maximum_root_count) {
        return false;
    }
    for (base::u64 index = 0; index < count; ++index) {
        if (!skip_canonical_type_key(reader)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool skip_param_env_key(StableKeyReader& reader) noexcept
{
    return read_marker(reader, QUERY_PARAM_ENV_KEY_MARKER) && skip_fingerprint(reader)
        && reader.skip(QUERY_STABLE_U32_BYTES) && read_nonzero_u64(reader);
}

[[nodiscard]] bool skip_generic_instance_key(StableKeyReader& reader) noexcept
{
    base::u64 type_arg_count = 0;
    base::u64 const_arg_count = 0;
    return read_marker(reader, QUERY_GENERIC_INSTANCE_KEY_MARKER) && skip_def_key(reader)
        && reader.read_u64(type_arg_count) && skip_canonical_type_keys(reader, type_arg_count)
        && reader.read_u64(const_arg_count) && skip_fingerprints(reader, const_arg_count) && skip_param_env_key(reader)
        && read_nonzero_u64(reader);
}

[[nodiscard]] bool skip_query_key(StableKeyReader& reader) noexcept
{
    return read_marker(reader, QUERY_QUERY_KEY_MARKER) && read_query_kind(reader) && reader.skip(QUERY_STABLE_U16_BYTES)
        && skip_fingerprint(reader) && read_nonzero_u64(reader);
}

template <typename SkipKey>
[[nodiscard]] bool stable_key_has_layout(const std::string_view bytes, SkipKey skip_key) noexcept
{
    StableKeyReader reader(bytes);
    return skip_key(reader) && reader.eof();
}

} // namespace

bool stable_key_has_file_key_layout(const std::string_view bytes) noexcept
{
    return stable_key_has_layout(bytes, skip_file_key);
}

bool stable_key_has_project_key_layout(const std::string_view bytes) noexcept
{
    return stable_key_has_layout(bytes, skip_project_key);
}

bool stable_key_has_module_key_layout(const std::string_view bytes) noexcept
{
    return stable_key_has_layout(bytes, skip_module_key);
}

bool stable_key_has_module_part_key_layout(const std::string_view bytes) noexcept
{
    return stable_key_has_layout(bytes, skip_module_part_key);
}

bool stable_key_has_body_key_layout(const std::string_view bytes) noexcept
{
    return stable_key_has_layout(bytes, skip_body_key);
}

bool stable_key_has_generic_instance_key_layout(const std::string_view bytes) noexcept
{
    return stable_key_has_layout(bytes, skip_generic_instance_key);
}

bool stable_key_has_query_key_layout(const std::string_view bytes) noexcept
{
    return stable_key_has_layout(bytes, skip_query_key);
}

bool stable_key_layout_matches_query_kind(const QueryKind kind, const std::string_view bytes) noexcept
{
    switch (kind) {
        case QueryKind::project_graph:
            return stable_key_has_project_key_layout(bytes);
        case QueryKind::file_content:
            return stable_key_has_file_key_layout(bytes);
        case QueryKind::lex_file:
            return decode_lex_file_key_identity(bytes).has_value();
        case QueryKind::parse_file:
            return decode_parse_file_key_identity(bytes).has_value();
        case QueryKind::module_part:
            return stable_key_has_module_part_key_layout(bytes);
        case QueryKind::module_graph:
        case QueryKind::module_exports:
        case QueryKind::module_package_exports:
        case QueryKind::item_list:
            return stable_key_has_module_key_layout(bytes);
        case QueryKind::item_signature:
        case QueryKind::generic_template_signature:
            return decode_def_key_identity(bytes).has_value();
        case QueryKind::function_body_syntax:
        case QueryKind::type_check_body:
            return stable_key_has_body_key_layout(bytes);
        case QueryKind::generic_instance_signature:
        case QueryKind::generic_instance_body:
            return stable_key_has_generic_instance_key_layout(bytes);
        case QueryKind::lower_function_ir:
            return stable_key_has_body_key_layout(bytes) || stable_key_has_generic_instance_key_layout(bytes);
        case QueryKind::diagnostics:
            return stable_key_has_query_key_layout(bytes);
        case QueryKind::invalid:
            return false;
    }
    return false;
}

std::optional<DecodedLexFileKeyIdentity> decode_lex_file_key_identity(const std::string_view bytes) noexcept
{
    StableKeyReader reader(bytes);
    std::optional<std::string_view> file;
    std::optional<std::string_view> lex_config;
    if (!read_marker(reader, QUERY_LEX_FILE_KEY_MARKER) || !(file = read_file_key_slice(reader)).has_value()
        || !(lex_config = read_lex_config_key_slice(reader)).has_value() || !read_nonzero_u64(reader)
        || !reader.eof()) {
        return std::nullopt;
    }
    return DecodedLexFileKeyIdentity{
        *file,
        *lex_config,
    };
}

std::optional<DecodedParseFileKeyIdentity> decode_parse_file_key_identity(const std::string_view bytes) noexcept
{
    StableKeyReader reader(bytes);
    std::optional<std::string_view> file;
    std::optional<std::string_view> lex_config;
    if (!read_marker(reader, QUERY_PARSE_FILE_KEY_MARKER) || !(file = read_file_key_slice(reader)).has_value()
        || !(lex_config = read_parser_config_key_lex_config_slice(reader)).has_value() || !read_nonzero_u64(reader)
        || !reader.eof()) {
        return std::nullopt;
    }
    return DecodedParseFileKeyIdentity{
        *file,
        *lex_config,
    };
}

std::optional<DecodedModulePartKeyIdentity> decode_module_part_key_identity(const std::string_view bytes) noexcept
{
    StableKeyReader reader(bytes);
    std::optional<std::string_view> module;
    std::optional<std::string_view> file;
    if (!(module = read_module_part_key_module_slice(reader)).has_value()
        || !(file = read_file_key_slice(reader)).has_value() || !skip_fingerprint(reader)
        || !reader.skip(QUERY_STABLE_U32_BYTES) || !read_module_part_kind(reader) || !read_nonzero_u64(reader)
        || !reader.eof()) {
        return std::nullopt;
    }
    return DecodedModulePartKeyIdentity{
        *module,
        *file,
    };
}

std::optional<DecodedDefKeyIdentity> decode_def_key_identity(const std::string_view bytes) noexcept
{
    StableKeyReader reader(bytes);
    std::optional<std::string_view> module;
    if (!read_marker(reader, QUERY_DEF_KEY_MARKER) || !(module = read_module_key_slice(reader)).has_value()
        || !skip_fingerprint(reader) || !reader.skip(QUERY_STABLE_U32_BYTES) || !read_def_namespace(reader)
        || !read_def_kind(reader) || !reader.skip(QUERY_STABLE_U32_BYTES) || !read_nonzero_u64(reader)
        || !reader.eof()) {
        return std::nullopt;
    }
    return DecodedDefKeyIdentity{*module};
}

std::optional<DecodedBodyKeyIdentity> decode_body_key_identity(const std::string_view bytes) noexcept
{
    StableKeyReader reader(bytes);
    std::optional<std::string_view> owner;
    if (!read_marker(reader, QUERY_BODY_KEY_MARKER) || !(owner = read_def_key_slice(reader)).has_value()
        || !read_body_slot_kind(reader) || !reader.skip(QUERY_STABLE_U32_BYTES) || !read_nonzero_u64(reader)
        || !reader.eof()) {
        return std::nullopt;
    }
    return DecodedBodyKeyIdentity{*owner};
}

std::optional<DecodedGenericInstanceKeyIdentity> decode_generic_instance_key_identity(
    const std::string_view bytes) noexcept
{
    StableKeyReader reader(bytes);
    std::optional<std::string_view> template_def;
    base::u64 type_arg_count = 0;
    base::u64 const_arg_count = 0;
    if (!read_marker(reader, QUERY_GENERIC_INSTANCE_KEY_MARKER)
        || !(template_def = read_def_key_slice(reader)).has_value() || !reader.read_u64(type_arg_count)
        || !skip_canonical_type_keys(reader, type_arg_count) || !reader.read_u64(const_arg_count)
        || !skip_fingerprints(reader, const_arg_count) || !skip_param_env_key(reader) || !read_nonzero_u64(reader)
        || !reader.eof()) {
        return std::nullopt;
    }
    return DecodedGenericInstanceKeyIdentity{*template_def};
}

} // namespace aurex::query
