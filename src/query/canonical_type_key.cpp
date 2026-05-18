#include <aurex/query/canonical_type_key.hpp>

#include <sstream>
#include <utility>

namespace aurex::query {
namespace {

constexpr base::u64 QUERY_CANONICAL_TYPE_KEY_MARKER = 0x5143545950453031ULL;
constexpr base::usize QUERY_CANONICAL_TYPE_STACK_RESERVE = 16;

[[nodiscard]] std::string_view canonical_type_kind_name(const CanonicalTypeKind kind) noexcept
{
    switch (kind) {
        case CanonicalTypeKind::invalid:
            return "invalid";
        case CanonicalTypeKind::builtin:
            return "builtin";
        case CanonicalTypeKind::pointer:
            return "pointer";
        case CanonicalTypeKind::reference:
            return "reference";
        case CanonicalTypeKind::array:
            return "array";
        case CanonicalTypeKind::slice:
            return "slice";
        case CanonicalTypeKind::tuple:
            return "tuple";
        case CanonicalTypeKind::function:
            return "function";
        case CanonicalTypeKind::nominal:
            return "nominal";
        case CanonicalTypeKind::generic_param:
            return "generic_param";
        case CanonicalTypeKind::const_arg:
            return "const_arg";
        case CanonicalTypeKind::associated_type_projection:
            return "associated_type_projection";
        case CanonicalTypeKind::trait_object:
            return "trait_object";
    }
    return "invalid";
}

[[nodiscard]] bool same_shallow(const CanonicalTypeKey& lhs, const CanonicalTypeKey& rhs) noexcept
{
    return lhs.kind == rhs.kind && lhs.builtin == rhs.builtin && lhs.mutability == rhs.mutability
        && lhs.array_count == rhs.array_count && lhs.function_call_conv == rhs.function_call_conv
        && lhs.function_is_unsafe == rhs.function_is_unsafe && lhs.function_is_variadic == rhs.function_is_variadic
        && lhs.function_param_count == rhs.function_param_count && lhs.nominal_def == rhs.nominal_def
        && lhs.generic_param == rhs.generic_param && lhs.associated_member == rhs.associated_member
        && lhs.const_value == rhs.const_value && lhs.children.size() == rhs.children.size();
}

void write_type_header(StableKeyWriter& writer, const CanonicalTypeKey& key)
{
    writer.write_u64(QUERY_CANONICAL_TYPE_KEY_MARKER);
    writer.write_u8(static_cast<base::u8>(key.kind));
    switch (key.kind) {
        case CanonicalTypeKind::builtin:
            writer.write_u8(static_cast<base::u8>(key.builtin));
            break;
        case CanonicalTypeKind::pointer:
        case CanonicalTypeKind::reference:
        case CanonicalTypeKind::slice:
            writer.write_u8(static_cast<base::u8>(key.mutability));
            break;
        case CanonicalTypeKind::array:
            writer.write_u64(key.array_count);
            break;
        case CanonicalTypeKind::function:
            writer.write_u8(static_cast<base::u8>(key.function_call_conv));
            writer.write_bool(key.function_is_unsafe);
            writer.write_bool(key.function_is_variadic);
            writer.write_u32(key.function_param_count);
            break;
        case CanonicalTypeKind::nominal:
            append_stable_key(writer, key.nominal_def);
            break;
        case CanonicalTypeKind::generic_param:
            append_stable_key(writer, key.generic_param);
            break;
        case CanonicalTypeKind::const_arg:
            writer.write_fingerprint(key.const_value);
            break;
        case CanonicalTypeKind::associated_type_projection:
            append_stable_key(writer, key.associated_member);
            break;
        case CanonicalTypeKind::trait_object:
        case CanonicalTypeKind::tuple:
        case CanonicalTypeKind::invalid:
            break;
    }
    writer.write_u64(static_cast<base::u64>(key.children.size()));
}

} // namespace

bool operator==(const CanonicalTypeKey& lhs, const CanonicalTypeKey& rhs) noexcept
{
    std::vector<std::pair<const CanonicalTypeKey*, const CanonicalTypeKey*>> pending;
    pending.reserve(QUERY_CANONICAL_TYPE_STACK_RESERVE);
    pending.emplace_back(&lhs, &rhs);

    while (!pending.empty()) {
        const auto [left, right] = pending.back();
        pending.pop_back();
        if (!same_shallow(*left, *right)) {
            return false;
        }
        for (base::usize index = 0; index < left->children.size(); ++index) {
            pending.emplace_back(&left->children[index], &right->children[index]);
        }
    }
    return true;
}

bool operator!=(const CanonicalTypeKey& lhs, const CanonicalTypeKey& rhs) noexcept
{
    return !(lhs == rhs);
}

bool is_valid(const CanonicalTypeKey& key) noexcept
{
    return key.kind != CanonicalTypeKind::invalid;
}

CanonicalTypeKey canonical_builtin(const BuiltinTypeKey builtin)
{
    CanonicalTypeKey key;
    key.kind = CanonicalTypeKind::builtin;
    key.builtin = builtin;
    return key;
}

CanonicalTypeKey canonical_pointer(const PointerMutabilityKey mutability, CanonicalTypeKey pointee)
{
    CanonicalTypeKey key;
    key.kind = CanonicalTypeKind::pointer;
    key.mutability = mutability;
    key.children.push_back(std::move(pointee));
    return key;
}

CanonicalTypeKey canonical_reference(const PointerMutabilityKey mutability, CanonicalTypeKey pointee)
{
    CanonicalTypeKey key;
    key.kind = CanonicalTypeKind::reference;
    key.mutability = mutability;
    key.children.push_back(std::move(pointee));
    return key;
}

CanonicalTypeKey canonical_array(const base::u64 count, CanonicalTypeKey element)
{
    CanonicalTypeKey key;
    key.kind = CanonicalTypeKind::array;
    key.array_count = count;
    key.children.push_back(std::move(element));
    return key;
}

CanonicalTypeKey canonical_slice(const PointerMutabilityKey mutability, CanonicalTypeKey element)
{
    CanonicalTypeKey key;
    key.kind = CanonicalTypeKind::slice;
    key.mutability = mutability;
    key.children.push_back(std::move(element));
    return key;
}

CanonicalTypeKey canonical_tuple(const std::span<const CanonicalTypeKey> elements)
{
    CanonicalTypeKey key;
    key.kind = CanonicalTypeKind::tuple;
    key.children.reserve(elements.size());
    for (const CanonicalTypeKey& element : elements) {
        key.children.push_back(element);
    }
    return key;
}

CanonicalTypeKey canonical_function(const FunctionCallConvKey call_conv, const bool is_unsafe, const bool is_variadic,
    const std::span<const CanonicalTypeKey> params, const CanonicalTypeKey& return_type)
{
    CanonicalTypeKey key;
    key.kind = CanonicalTypeKind::function;
    key.function_call_conv = call_conv;
    key.function_is_unsafe = is_unsafe;
    key.function_is_variadic = is_variadic;
    key.function_param_count = static_cast<base::u32>(params.size());
    key.children.reserve(params.size() + 1);
    for (const CanonicalTypeKey& param : params) {
        key.children.push_back(param);
    }
    key.children.push_back(return_type);
    return key;
}

CanonicalTypeKey canonical_nominal(const DefKey definition, const std::span<const CanonicalTypeKey> args)
{
    CanonicalTypeKey key;
    key.kind = CanonicalTypeKind::nominal;
    key.nominal_def = definition;
    key.children.reserve(args.size());
    for (const CanonicalTypeKey& arg : args) {
        key.children.push_back(arg);
    }
    return key;
}

CanonicalTypeKey canonical_generic_param(const GenericParamKey parameter)
{
    CanonicalTypeKey key;
    key.kind = CanonicalTypeKind::generic_param;
    key.generic_param = parameter;
    return key;
}

CanonicalTypeKey canonical_const_arg(const StableFingerprint128 value)
{
    CanonicalTypeKey key;
    key.kind = CanonicalTypeKind::const_arg;
    key.const_value = value;
    return key;
}

CanonicalTypeKey canonical_associated_type_projection(CanonicalTypeKey base_type, const MemberKey associated_member)
{
    CanonicalTypeKey key;
    key.kind = CanonicalTypeKind::associated_type_projection;
    key.associated_member = associated_member;
    key.children.push_back(std::move(base_type));
    return key;
}

void append_stable_key(StableKeyWriter& writer, const CanonicalTypeKey& key)
{
    std::vector<const CanonicalTypeKey*> pending;
    pending.reserve(QUERY_CANONICAL_TYPE_STACK_RESERVE);
    pending.push_back(&key);

    while (!pending.empty()) {
        const CanonicalTypeKey* const current = pending.back();
        pending.pop_back();
        write_type_header(writer, *current);
        for (base::usize index = current->children.size(); index > 0; --index) {
            pending.push_back(&current->children[index - 1]);
        }
    }
}

std::string stable_serialize(const CanonicalTypeKey& key)
{
    StableKeyWriter writer;
    append_stable_key(writer, key);
    return writer.storage();
}

StableFingerprint128 stable_key_fingerprint(const CanonicalTypeKey& key)
{
    StableKeyWriter writer;
    append_stable_key(writer, key);
    return writer.fingerprint();
}

std::string debug_string(const CanonicalTypeKey& key)
{
    std::ostringstream out;
    out << "CanonicalTypeKey{kind=" << canonical_type_kind_name(key.kind)
        << ",fingerprint=" << debug_string(stable_key_fingerprint(key)) << '}';
    return out.str();
}

std::size_t CanonicalTypeKeyHash::operator()(const CanonicalTypeKey& key) const
{
    return stable_hash_value(stable_key_fingerprint(key));
}

} // namespace aurex::query
