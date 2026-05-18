#pragma once

#include <aurex/ir/ir.hpp>

#include <string_view>

namespace aurex::ir {

inline constexpr std::string_view IR_ENUM_TAG_FIELD_NAME = "tag";
inline constexpr std::string_view IR_ENUM_PAYLOAD_FIELD_NAME = "payload";

[[nodiscard]] bool is_payload_enum(const sema::TypeTable& types, sema::TypeHandle type) noexcept;
[[nodiscard]] sema::TypeHandle enum_tag_type(const sema::TypeTable& types, sema::TypeHandle enum_type) noexcept;
[[nodiscard]] sema::TypeHandle enum_payload_storage_type(
    const sema::TypeTable& types, sema::TypeHandle enum_type) noexcept;
[[nodiscard]] RecordLayout make_payload_enum_record(Module& module, sema::TypeHandle enum_type);

} // namespace aurex::ir
