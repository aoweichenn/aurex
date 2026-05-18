#pragma once

#include <aurex/base/result.hpp>
#include <aurex/query/canonical_type_key.hpp>
#include <aurex/sema/type.hpp>

#include <optional>

namespace aurex::sema {

class CanonicalTypeKeyResolver {
public:
    virtual ~CanonicalTypeKeyResolver() = default;

    [[nodiscard]] virtual std::optional<query::DefKey> nominal_type_key(
        TypeHandle handle,
        const TypeInfo& info) const = 0;
    [[nodiscard]] virtual std::optional<query::GenericParamKey> generic_param_key(
        TypeHandle handle,
        const TypeInfo& info) const = 0;
};

[[nodiscard]] base::Result<query::CanonicalTypeKey> build_canonical_type_key(
    const TypeTable& types,
    TypeHandle type,
    const CanonicalTypeKeyResolver& resolver);

} // namespace aurex::sema
