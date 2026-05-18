#include <aurex/query/generic_instance_key.hpp>

#include <sstream>

namespace aurex::query {
namespace {

constexpr base::u64 QUERY_PARAM_ENV_KEY_MARKER = 0x5150454e56303131ULL;
constexpr base::u64 QUERY_GENERIC_INSTANCE_KEY_MARKER = 0x5147494e53543031ULL;

[[nodiscard]] base::u64 global_id_from_fingerprint(
    const base::u64 marker,
    const StableFingerprint128 fingerprint) noexcept {
    base::u64 global_id = stable_mix(marker, fingerprint.primary);
    global_id = stable_mix(global_id, fingerprint.secondary);
    global_id = stable_mix(global_id, fingerprint.byte_count);
    return global_id == 0 ? marker : global_id;
}

} // namespace

bool is_valid(const ParamEnvKey key) noexcept {
    return key.global_id != 0;
}

bool is_valid(const GenericInstanceKey& key) noexcept {
    return is_valid(key.template_def) && is_valid(key.param_env) && key.global_id != 0;
}

bool operator==(const GenericInstanceKey& lhs, const GenericInstanceKey& rhs) noexcept {
    if (lhs.template_def != rhs.template_def || lhs.const_args != rhs.const_args || lhs.param_env != rhs.param_env || lhs.global_id != rhs.global_id || lhs.type_args.size() != rhs.type_args.size()) {
        return false;
    }
    for (base::usize index = 0; index < lhs.type_args.size(); ++index) {
        if (lhs.type_args[index] != rhs.type_args[index]) {
            return false;
        }
    }
    return true;
}

bool operator!=(const GenericInstanceKey& lhs, const GenericInstanceKey& rhs) noexcept {
    return !(lhs == rhs);
}

ParamEnvKey param_env_key(const std::span<const std::string_view> predicates) noexcept {
    const StableFingerprint128 fingerprint = stable_fingerprint(predicates);
    return ParamEnvKey {
        fingerprint,
        static_cast<base::u32>(predicates.size()),
        global_id_from_fingerprint(QUERY_PARAM_ENV_KEY_MARKER, fingerprint),
    };
}

GenericInstanceKey generic_instance_key(
    const DefKey template_def,
    const std::span<const CanonicalTypeKey> type_args,
    const std::span<const StableFingerprint128> const_args,
    const ParamEnvKey param_env) {
    GenericInstanceKey key;
    key.template_def = template_def;
    key.type_args.reserve(type_args.size());
    for (const CanonicalTypeKey& type_arg : type_args) {
        key.type_args.push_back(type_arg);
    }
    key.const_args.reserve(const_args.size());
    for (const StableFingerprint128 const_arg : const_args) {
        key.const_args.push_back(const_arg);
    }
    key.param_env = param_env;
    StableKeyWriter writer;
    append_stable_key(writer, key.template_def);
    writer.write_u64(static_cast<base::u64>(key.type_args.size()));
    for (const CanonicalTypeKey& type_arg : key.type_args) {
        append_stable_key(writer, type_arg);
    }
    writer.write_u64(static_cast<base::u64>(key.const_args.size()));
    for (const StableFingerprint128 const_arg : key.const_args) {
        writer.write_fingerprint(const_arg);
    }
    append_stable_key(writer, key.param_env);
    key.global_id = global_id_from_fingerprint(QUERY_GENERIC_INSTANCE_KEY_MARKER, writer.fingerprint());
    return key;
}

void append_stable_key(StableKeyWriter& writer, const ParamEnvKey key) {
    writer.write_u64(QUERY_PARAM_ENV_KEY_MARKER);
    writer.write_fingerprint(key.predicates);
    writer.write_u32(key.predicate_count);
    writer.write_u64(key.global_id);
}

void append_stable_key(StableKeyWriter& writer, const GenericInstanceKey& key) {
    writer.write_u64(QUERY_GENERIC_INSTANCE_KEY_MARKER);
    append_stable_key(writer, key.template_def);
    writer.write_u64(static_cast<base::u64>(key.type_args.size()));
    for (const CanonicalTypeKey& type_arg : key.type_args) {
        append_stable_key(writer, type_arg);
    }
    writer.write_u64(static_cast<base::u64>(key.const_args.size()));
    for (const StableFingerprint128 const_arg : key.const_args) {
        writer.write_fingerprint(const_arg);
    }
    append_stable_key(writer, key.param_env);
    writer.write_u64(key.global_id);
}

std::string stable_serialize(const ParamEnvKey key) {
    StableKeyWriter writer;
    append_stable_key(writer, key);
    return writer.storage();
}

std::string stable_serialize(const GenericInstanceKey& key) {
    StableKeyWriter writer;
    append_stable_key(writer, key);
    return writer.storage();
}

StableFingerprint128 stable_key_fingerprint(const ParamEnvKey key) {
    StableKeyWriter writer;
    append_stable_key(writer, key);
    return writer.fingerprint();
}

StableFingerprint128 stable_key_fingerprint(const GenericInstanceKey& key) {
    StableKeyWriter writer;
    append_stable_key(writer, key);
    return writer.fingerprint();
}

std::string debug_string(const ParamEnvKey key) {
    std::ostringstream out;
    out << "ParamEnvKey{global=" << key.global_id
        << ",predicates=" << debug_string(key.predicates) << '}';
    return out.str();
}

std::string debug_string(const GenericInstanceKey& key) {
    std::ostringstream out;
    out << "GenericInstanceKey{global=" << key.global_id
        << ",fingerprint=" << debug_string(stable_key_fingerprint(key)) << '}';
    return out.str();
}

std::size_t ParamEnvKeyHash::operator()(const ParamEnvKey key) const {
    return stable_hash_value(stable_key_fingerprint(key));
}

std::size_t GenericInstanceKeyHash::operator()(const GenericInstanceKey& key) const {
    return stable_hash_value(stable_key_fingerprint(key));
}

} // namespace aurex::query
