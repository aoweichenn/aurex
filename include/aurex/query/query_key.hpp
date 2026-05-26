#pragma once

#include <aurex/query/stable_identity.hpp>

#include <span>
#include <string>
#include <string_view>

namespace aurex::query {

inline constexpr base::u16 QUERY_KEY_SCHEMA_VERSION = 1;
inline constexpr base::u32 QUERY_LEX_CONFIG_SCHEMA_VERSION = 1;
inline constexpr base::u32 QUERY_PARSER_CONFIG_SCHEMA_VERSION = 1;
inline constexpr bool QUERY_LEX_CONFIG_DEFAULT_RETAIN_TRIVIA = false;
inline constexpr bool QUERY_PARSER_CONFIG_DEFAULT_ENABLE_RECOVERY = true;
inline constexpr bool QUERY_PARSER_CONFIG_DEFAULT_BUILD_LOSSLESS_TREE = false;

enum class QueryKind : base::u8 {
    invalid = 0,
    file_content,
    lex_file,
    parse_file,
    module_graph,
    module_exports,
    item_list,
    item_signature,
    function_body_syntax,
    type_check_body,
    generic_template_signature,
    generic_instance_signature,
    generic_instance_body,
    diagnostics,
    lower_function_ir,
    module_package_exports,
};

enum class SourceRole : base::u8 {
    source = 0,
    virtual_buffer,
    generated,
};

enum class ModuleKind : base::u8 {
    source = 0,
    builtin,
    synthetic,
};

enum class ModulePartKind : base::u8 {
    primary = 0,
    fragment,
    generated,
};

enum class DefNamespace : base::u8 {
    value = 0,
    type,
    member,
    trait_,
    impl_,
    synthetic,
};

enum class DefKind : base::u8 {
    invalid = 0,
    function,
    method,
    value,
    const_,
    global,
    type_alias,
    struct_,
    enum_,
    enum_case,
    struct_field,
    generic_template,
    trait_,
    trait_method,
    associated_type,
    associated_const,
    synthetic,
};

enum class MemberKind : base::u8 {
    invalid = 0,
    struct_field,
    enum_case,
    trait_method,
    associated_type,
    associated_const,
    synthetic,
};

enum class BodySlotKind : base::u8 {
    function_body = 0,
    const_initializer,
    global_initializer,
    default_argument,
    trait_default_method,
    destructor_drop,
    closure_body,
};

enum class GenericParamKind : base::u8 {
    type = 0,
    const_,
    resource,
    lifetime,
};

struct PackageKey {
    StableFingerprint128 identity;
    base::u64 global_id = 0;

    [[nodiscard]] friend constexpr bool operator==(PackageKey lhs, PackageKey rhs) noexcept = default;
};

struct FileKey {
    PackageKey package;
    StableFingerprint128 path;
    StableFingerprint128 virtual_buffer;
    SourceRole role = SourceRole::source;
    base::u64 global_id = 0;

    [[nodiscard]] friend constexpr bool operator==(FileKey lhs, FileKey rhs) noexcept = default;
};

struct LexConfigKey {
    base::u32 schema = QUERY_LEX_CONFIG_SCHEMA_VERSION;
    bool retain_trivia = QUERY_LEX_CONFIG_DEFAULT_RETAIN_TRIVIA;
    base::u64 global_id = 0;

    [[nodiscard]] friend constexpr bool operator==(LexConfigKey lhs, LexConfigKey rhs) noexcept = default;
};

struct ParserConfigKey {
    LexConfigKey lex_config;
    base::u32 schema = QUERY_PARSER_CONFIG_SCHEMA_VERSION;
    bool enable_recovery = QUERY_PARSER_CONFIG_DEFAULT_ENABLE_RECOVERY;
    bool build_lossless_tree = QUERY_PARSER_CONFIG_DEFAULT_BUILD_LOSSLESS_TREE;
    base::u64 global_id = 0;

    [[nodiscard]] friend constexpr bool operator==(ParserConfigKey lhs, ParserConfigKey rhs) noexcept = default;
};

struct LexFileKey {
    FileKey file;
    LexConfigKey config;
    base::u64 global_id = 0;

    [[nodiscard]] friend constexpr bool operator==(LexFileKey lhs, LexFileKey rhs) noexcept = default;
};

struct ParseFileKey {
    FileKey file;
    ParserConfigKey config;
    base::u64 global_id = 0;

    [[nodiscard]] friend constexpr bool operator==(ParseFileKey lhs, ParseFileKey rhs) noexcept = default;
};

struct ModuleKey {
    PackageKey package;
    StableFingerprint128 path;
    base::u32 path_component_count = 0;
    ModuleKind kind = ModuleKind::source;
    base::u64 global_id = 0;

    [[nodiscard]] friend constexpr bool operator==(ModuleKey lhs, ModuleKey rhs) noexcept = default;
};

struct ModulePartKey {
    ModuleKey module;
    FileKey file;
    StableFingerprint128 name;
    base::u32 stable_index = 0;
    ModulePartKind kind = ModulePartKind::primary;
    base::u64 global_id = 0;

    [[nodiscard]] friend constexpr bool operator==(ModulePartKey lhs, ModulePartKey rhs) noexcept = default;
};

struct DefKey {
    ModuleKey module;
    StableFingerprint128 path;
    base::u32 path_component_count = 0;
    DefNamespace name_space = DefNamespace::value;
    DefKind kind = DefKind::invalid;
    base::u32 disambiguator = 0;
    base::u64 global_id = 0;

    [[nodiscard]] friend constexpr bool operator==(DefKey lhs, DefKey rhs) noexcept = default;
};

struct MemberKey {
    DefKey owner;
    StableFingerprint128 name;
    MemberKind kind = MemberKind::invalid;
    base::u32 ordinal = 0;
    base::u64 global_id = 0;

    [[nodiscard]] friend constexpr bool operator==(MemberKey lhs, MemberKey rhs) noexcept = default;
};

struct BodyKey {
    DefKey owner;
    BodySlotKind slot = BodySlotKind::function_body;
    base::u32 ordinal = 0;
    base::u64 global_id = 0;

    [[nodiscard]] friend constexpr bool operator==(BodyKey lhs, BodyKey rhs) noexcept = default;
};

struct GenericParamKey {
    DefKey owner;
    base::u32 index = 0;
    GenericParamKind kind = GenericParamKind::type;
    base::u64 global_id = 0;

    [[nodiscard]] friend constexpr bool operator==(GenericParamKey lhs, GenericParamKey rhs) noexcept = default;
};

struct QueryKey {
    QueryKind kind = QueryKind::invalid;
    base::u16 schema = QUERY_KEY_SCHEMA_VERSION;
    StableFingerprint128 payload;
    base::u64 global_id = 0;

    [[nodiscard]] friend constexpr bool operator==(QueryKey lhs, QueryKey rhs) noexcept = default;
};

[[nodiscard]] bool is_valid(PackageKey key) noexcept;
[[nodiscard]] bool is_valid(FileKey key) noexcept;
[[nodiscard]] bool is_valid(LexConfigKey key) noexcept;
[[nodiscard]] bool is_valid(ParserConfigKey key) noexcept;
[[nodiscard]] bool is_valid(LexFileKey key) noexcept;
[[nodiscard]] bool is_valid(ParseFileKey key) noexcept;
[[nodiscard]] bool is_valid(ModuleKey key) noexcept;
[[nodiscard]] bool is_valid(ModulePartKey key) noexcept;
[[nodiscard]] bool is_valid(DefKey key) noexcept;
[[nodiscard]] bool is_valid(MemberKey key) noexcept;
[[nodiscard]] bool is_valid(BodyKey key) noexcept;
[[nodiscard]] bool is_valid(GenericParamKey key) noexcept;
[[nodiscard]] bool is_valid(QueryKey key) noexcept;

[[nodiscard]] PackageKey package_key(std::span<const std::string_view> identity_parts) noexcept;
[[nodiscard]] FileKey file_key(PackageKey package, std::string_view canonical_path,
    SourceRole role = SourceRole::source, std::string_view virtual_buffer = {}) noexcept;
[[nodiscard]] LexConfigKey lex_config_key(bool retain_trivia = QUERY_LEX_CONFIG_DEFAULT_RETAIN_TRIVIA) noexcept;
[[nodiscard]] ParserConfigKey parser_config_key(bool enable_recovery = QUERY_PARSER_CONFIG_DEFAULT_ENABLE_RECOVERY,
    bool build_lossless_tree = QUERY_PARSER_CONFIG_DEFAULT_BUILD_LOSSLESS_TREE) noexcept;
[[nodiscard]] ParserConfigKey parser_config_key(LexConfigKey lex_config,
    bool enable_recovery = QUERY_PARSER_CONFIG_DEFAULT_ENABLE_RECOVERY,
    bool build_lossless_tree = QUERY_PARSER_CONFIG_DEFAULT_BUILD_LOSSLESS_TREE) noexcept;
[[nodiscard]] LexFileKey lex_file_key(FileKey file, LexConfigKey config = lex_config_key()) noexcept;
[[nodiscard]] ParseFileKey parse_file_key(FileKey file, ParserConfigKey config = parser_config_key()) noexcept;
[[nodiscard]] ModuleKey module_key(
    PackageKey package, std::span<const std::string_view> module_path, ModuleKind kind = ModuleKind::source) noexcept;
[[nodiscard]] ModulePartKey module_part_key(
    ModuleKey module, FileKey file, ModulePartKind kind, std::string_view name, base::u32 stable_index = 0) noexcept;
[[nodiscard]] DefKey def_key(ModuleKey module, DefNamespace name_space, DefKind kind,
    std::span<const std::string_view> path, base::u32 disambiguator = 0) noexcept;
[[nodiscard]] MemberKey member_key(
    DefKey owner, MemberKind kind, std::string_view name, base::u32 ordinal = 0) noexcept;
[[nodiscard]] BodyKey body_key(DefKey owner, BodySlotKind slot, base::u32 ordinal = 0) noexcept;
[[nodiscard]] GenericParamKey generic_param_key(
    DefKey owner, base::u32 index, GenericParamKind kind = GenericParamKind::type) noexcept;
[[nodiscard]] QueryKey query_key(
    QueryKind kind, StableFingerprint128 payload, base::u16 schema = QUERY_KEY_SCHEMA_VERSION) noexcept;
[[nodiscard]] ModuleKey module_key_from_stable_id(PackageKey package, StableModuleId stable_module) noexcept;
[[nodiscard]] ModuleKey module_key_from_stable_id(StableModuleId stable_module) noexcept;
[[nodiscard]] DefKey def_key_from_stable_id(
    PackageKey package, StableDefId stable_id, DefNamespace name_space, DefKind kind) noexcept;
[[nodiscard]] DefKey def_key_from_stable_id(StableDefId stable_id, DefNamespace name_space, DefKind kind) noexcept;

void append_stable_key(StableKeyWriter& writer, PackageKey key);
void append_stable_key(StableKeyWriter& writer, FileKey key);
void append_stable_key(StableKeyWriter& writer, LexConfigKey key);
void append_stable_key(StableKeyWriter& writer, ParserConfigKey key);
void append_stable_key(StableKeyWriter& writer, LexFileKey key);
void append_stable_key(StableKeyWriter& writer, ParseFileKey key);
void append_stable_key(StableKeyWriter& writer, ModuleKey key);
void append_stable_key(StableKeyWriter& writer, ModulePartKey key);
void append_stable_key(StableKeyWriter& writer, DefKey key);
void append_stable_key(StableKeyWriter& writer, MemberKey key);
void append_stable_key(StableKeyWriter& writer, BodyKey key);
void append_stable_key(StableKeyWriter& writer, GenericParamKey key);
void append_stable_key(StableKeyWriter& writer, QueryKey key);

[[nodiscard]] std::string stable_serialize(PackageKey key);
[[nodiscard]] std::string stable_serialize(FileKey key);
[[nodiscard]] std::string stable_serialize(LexConfigKey key);
[[nodiscard]] std::string stable_serialize(ParserConfigKey key);
[[nodiscard]] std::string stable_serialize(LexFileKey key);
[[nodiscard]] std::string stable_serialize(ParseFileKey key);
[[nodiscard]] std::string stable_serialize(ModuleKey key);
[[nodiscard]] std::string stable_serialize(ModulePartKey key);
[[nodiscard]] std::string stable_serialize(DefKey key);
[[nodiscard]] std::string stable_serialize(MemberKey key);
[[nodiscard]] std::string stable_serialize(BodyKey key);
[[nodiscard]] std::string stable_serialize(GenericParamKey key);
[[nodiscard]] std::string stable_serialize(QueryKey key);

[[nodiscard]] StableFingerprint128 stable_key_fingerprint(PackageKey key);
[[nodiscard]] StableFingerprint128 stable_key_fingerprint(FileKey key);
[[nodiscard]] StableFingerprint128 stable_key_fingerprint(LexConfigKey key);
[[nodiscard]] StableFingerprint128 stable_key_fingerprint(ParserConfigKey key);
[[nodiscard]] StableFingerprint128 stable_key_fingerprint(LexFileKey key);
[[nodiscard]] StableFingerprint128 stable_key_fingerprint(ParseFileKey key);
[[nodiscard]] StableFingerprint128 stable_key_fingerprint(ModuleKey key);
[[nodiscard]] StableFingerprint128 stable_key_fingerprint(ModulePartKey key);
[[nodiscard]] StableFingerprint128 stable_key_fingerprint(DefKey key);
[[nodiscard]] StableFingerprint128 stable_key_fingerprint(MemberKey key);
[[nodiscard]] StableFingerprint128 stable_key_fingerprint(BodyKey key);
[[nodiscard]] StableFingerprint128 stable_key_fingerprint(GenericParamKey key);
[[nodiscard]] StableFingerprint128 stable_key_fingerprint(QueryKey key);

[[nodiscard]] std::string debug_string(PackageKey key);
[[nodiscard]] std::string debug_string(FileKey key);
[[nodiscard]] std::string debug_string(LexConfigKey key);
[[nodiscard]] std::string debug_string(ParserConfigKey key);
[[nodiscard]] std::string debug_string(LexFileKey key);
[[nodiscard]] std::string debug_string(ParseFileKey key);
[[nodiscard]] std::string debug_string(ModuleKey key);
[[nodiscard]] std::string debug_string(ModulePartKey key);
[[nodiscard]] std::string debug_string(DefKey key);
[[nodiscard]] std::string debug_string(MemberKey key);
[[nodiscard]] std::string debug_string(BodyKey key);
[[nodiscard]] std::string debug_string(GenericParamKey key);
[[nodiscard]] std::string debug_string(QueryKey key);

struct PackageKeyHash {
    [[nodiscard]] std::size_t operator()(PackageKey key) const;
};

struct FileKeyHash {
    [[nodiscard]] std::size_t operator()(FileKey key) const;
};

struct LexFileKeyHash {
    [[nodiscard]] std::size_t operator()(LexFileKey key) const;
};

struct ParseFileKeyHash {
    [[nodiscard]] std::size_t operator()(ParseFileKey key) const;
};

struct ModuleKeyHash {
    [[nodiscard]] std::size_t operator()(ModuleKey key) const;
};

struct ModulePartKeyHash {
    [[nodiscard]] std::size_t operator()(ModulePartKey key) const;
};

struct DefKeyHash {
    [[nodiscard]] std::size_t operator()(DefKey key) const;
};

struct QueryKeyHash {
    [[nodiscard]] std::size_t operator()(QueryKey key) const;
};

} // namespace aurex::query
